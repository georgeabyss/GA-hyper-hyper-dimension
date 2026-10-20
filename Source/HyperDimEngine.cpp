#include "HyperDimEngine.h"

namespace hd
{

void Engine::prepare (double sampleRate, int channels, int oversampling)
{
    sr = sampleRate;
    os = std::max (1, oversampling);
    nCh = std::max (1, std::min (2, channels));
    for (auto& l : line) l.init ((int) std::ceil (0.10 * sr));
    dimLine.init ((int) std::ceil (0.05 * sr));
    wideLine.init ((int) std::ceil (0.005 * sr));
    setupWide();

    auto coef = [this] (double seconds) { return 1.0 - std::exp (-1.0 / (seconds * sr)); };
    tauCoef = coef (0.002);
    mixCoef = coef (0.02);
    spaceCoef = coef (0.03);
    offDecay = std::exp (-1.0 / (0.4 * sr));
    driveCoef = coef (0.1);
    guardPowCoef = coef (0.05);   // phase guard: ~50 ms, quicker than the meter
    guardDown = coef (0.01);      // narrows quickly
    guardUp = coef (0.40);        // and widens again slowly
    powCoef = coef (0.30);        // loudness window ~300 ms
    gainCoef = coef (0.12);       // gain rises slowly (~120 ms)
    attackCoef = coef (0.008);    // and falls fast (~8 ms) so jumps never overshoot
    for (int c = 0; c < 2; ++c)
        for (auto* k : { kwIn[c], kwOut[c] }) { k[0].kShelf (sr); k[1].kHighpass (sr); }
    reset();
}

void Engine::reset()
{
    for (auto& l : line) l.clear();
    dimLine.clear();
    wideLine.clear();
    wideActive = std::max (0, std::min (kNumWideModes - 1, prm.wideMode));
    wideS = prm.wide ? 1.0 : 0.0;
    resetWide();
    powIn = powOut = 0.0;
    normGain = 1.0;
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 2; ++k) { kwIn[c][k].reset(); kwOut[c][k].reset(); }
    gmS = (double) prm.globalMix;
    guardInM = guardInS = guardDM = guardDS = guardCrossM = guardCrossS = 0.0;
    sideGain = midGain = 1.0;
    amtS = (double) prm.wideAmount; spcS = (double) prm.wideSpacing;
    bmL.reset(); bmR.reset(); bmFreq = -1.0;
    for (int v = 0; v < kMaxVoices; ++v)
    {
        theta[(size_t) v] = twoPi * std::fmod (0.61803398875 * (v + 1), 1.0);
        tau[(size_t) v] = baseTau;
        tauOff[(size_t) v] = 0.0;
        lastTarget[(size_t) v] = baseTau;
    }
    dimPhase = 0.0;
    hmS = (double) prm.hyperMix;
    dmS = (double) prm.dimMix;
}

void Engine::retrig()
{
    for (int v = 0; v < kMaxVoices; ++v)
    {
        theta[(size_t) v] = 0.0;
        tauOff[(size_t) v] = std::max (0.0, lastTarget[(size_t) v] - baseTau);
    }
}

Engine::Block Engine::prepareBlock()
{
    static constexpr double rateMul[kMaxVoices] = { 1.00, 1.07, 0.93, 1.13, 0.89, 1.19, 0.83, 1.25, 0.78,
                                                    1.31, 0.74, 1.37, 0.70, 1.43, 0.66, 1.49, 0.62 };
    static constexpr double ampMul[kMaxVoices]  = { 1.00, 0.85, 0.70, 0.92, 0.60, 0.78, 0.50, 0.88, 0.66,
                                                    0.95, 0.55, 0.74, 0.82, 0.58, 0.90, 0.64, 0.72 };
    Block b;
    b.voices = std::max (0, std::min (kMaxVoices, prm.unison));
    for (int v = 0; v < b.voices; ++v)
    {
        b.omega[v] = twoPi * std::max (0.001, (double) prm.rateHz * rateMul[v]);
        b.inc[v] = b.omega[v] / sr;
        const double c = std::pow (2.0, (double) prm.detuneCents * ampMul[v] / 1200.0) - 1.0;
        b.amp[v] = std::min (0.03, c / b.omega[v]);   // seconds of delay sweep
        const double s = b.voices > 1 ? ((double) v / (double) (b.voices - 1)) * 2.0 - 1.0 : 0.0;
        const double ang = (s * (double) prm.spread + 1.0) * 0.25 * 3.14159265358979;   // equal-power pan
        b.gainL[v] = std::cos (ang) * 1.41421356;
        b.gainR[v] = std::sin (ang) * 1.41421356;
    }
    b.wetScale = b.voices > 0 ? 1.0f / std::sqrt ((float) b.voices) : 0.0f;

    b.dimBase1 = 0.003 + 0.012 * (double) prm.dimSize;
    b.dimBase2 = 0.0045 + 0.014 * (double) prm.dimSize;
    b.dimDepth = 0.0004 + 0.0016 * (double) prm.dimSize;
    b.dimInc = twoPi * 0.35 / sr;

    b.bassMono = prm.bassMonoHz > 20.5f;
    if (b.bassMono && std::fabs ((double) prm.bassMonoHz - bmFreq) > 0.01)
    {
        bmFreq = (double) prm.bassMonoHz;
        bmL.set (bmFreq, sr); bmR.set (bmFreq, sr);
    }
    if (! b.bassMono)
        bmFreq = -1.0;
    return b;
}

void Engine::process (float* const* io, int n)
{
    const Block block = prepareBlock();

    for (int i = 0; i < n; ++i)
    {
        const float l = io[0][i];
        const float r = nCh > 1 ? io[1][i] : l;

        float outL = l, outR = r;
        hyper (l, r, outL, outR, block);
        dimension (outL, outR, block);
        spacey (outL, outR);
        dryWet (l, r, outL, outR);
        if (block.bassMono)
            bassMono (outL, outR);
        phaseGuard (l, r, outL, outR);
        loudnessMatch (l, r, outL, outR);

        io[0][i] = outL;
        if (nCh > 1) io[1][i] = outR;
    }

    updateViz (block);
}

//==============================================================================
void Engine::hyper (float l, float r, float& outL, float& outR, const Block& b)
{
    line[0].push (l);
    line[1].push (r);
    hmS += ((double) prm.hyperMix - hmS) * mixCoef;
    if (b.voices == 0 || (hmS <= 1.0e-4 && prm.hyperMix <= 0.0f))   // off at mix 0: skip the voices
        return;

    float wl = 0.0f, wr = 0.0f;
    for (int v = 0; v < b.voices; ++v)
    {
        auto& th = theta[(size_t) v];
        th += b.inc[v];
        if (th >= twoPi) th -= twoPi;
        auto& off = tauOff[(size_t) v];
        off *= offDecay;
        const double target = baseTau + b.amp[v] * (1.0 - std::cos (th)) + off;
        lastTarget[(size_t) v] = target;
        auto& t = tau[(size_t) v];
        t += (target - t) * tauCoef;
        const double d = std::max (2.0, t * sr);
        wl += line[0].read (d) * (float) b.gainL[v];
        wr += line[1].read (d) * (float) b.gainR[v];
    }
    const float m = (float) hmS;
    outL = l + m * (wl * b.wetScale - l);
    outR = r + m * (wr * b.wetScale - r);
}

void Engine::dimension (float& outL, float& outR, const Block& b)
{
    dmS += ((double) prm.dimMix - dmS) * mixCoef;
    dimLine.push (0.5f * (outL + outR));
    dimPhase += b.dimInc;
    if (dimPhase >= twoPi) dimPhase -= twoPi;
    const double lfo = std::sin (dimPhase);
    const float dl = dimLine.read (std::max (2.0, (b.dimBase1 + b.dimDepth * lfo) * sr));
    const float dr = dimLine.read (std::max (2.0, (b.dimBase2 - b.dimDepth * lfo) * sr));
    const float dg = 0.8f * (float) dmS;
    outL += dg * dl;
    outR -= dg * dr;
}

void Engine::spacey (float& outL, float& outR)
{
    wideLine.push (combHp.process (0.5f * (outL + outR)));   // bass stays out of the comb

    const int req = std::max (0, std::min (kNumWideModes - 1, prm.wideMode));
    const double target = (prm.wide && req == wideActive) ? 1.0 : 0.0;
    wideS += (target - wideS) * mixCoef;
    amtS += ((double) prm.wideAmount - amtS) * spaceCoef;
    spcS += ((double) prm.wideSpacing - spcS) * spaceCoef;
    if (wideS < 0.0005 && req != wideActive)   // mode change: fade out, switch, fade in
    {
        wideActive = req;
        resetWide();
    }
    if (wideS <= 0.0005)
        return;

    float wl = outL, wr = outR;
    wideProcess (outL, outR, wl, wr);
    const float amt = (float) (wideS * amtS);
    outL += amt * (wl - outL);
    outR += amt * (wr - outR);
}

// before bass mono and the loudness match, so the level stays put
void Engine::dryWet (float l, float r, float& outL, float& outR)
{
    gmS += ((double) prm.globalMix - gmS) * mixCoef;
    outL = l + (float) gmS * (outL - l);
    outR = r + (float) gmS * (outR - r);
}

// everything below the cutoff is summed to mono
void Engine::bassMono (float& outL, float& outR)
{
    float ll, lh, rl, rh;
    bmL.split (outL, ll, lh);
    bmR.split (outR, rl, rh);
    const float bass = 0.5f * (ll + rl);
    outL = bass + lh;
    outR = bass + rh;
}

// If the effect pushes the correlation below kGuardFloor, what the effect changed is pulled back
// until the correlation is there again: first the side signal it added, and only if that is not
// enough, the mid signal it took away (Prism lowers the mid when it pans bands outwards).
// The input itself is never touched, so with every section off the output is exactly the input.
void Engine::phaseGuard (float l, float r, float& outL, float& outR)
{
    if (! prm.phaseGuard || nCh < 2)
        return;

    const double im = 0.5 * ((double) l + r), is = 0.5 * ((double) l - r);
    const double dM = 0.5 * ((double) outL + outR) - im;   // what the effect did to the mid
    const double dS = 0.5 * ((double) outL - outR) - is;   // ... and to the side
    auto track = [this] (double& avg, double x) { avg += (x - avg) * guardPowCoef; };
    track (guardInM, im * im);  track (guardInS, is * is);
    track (guardDM, dM * dM);   track (guardDS, dS * dS);
    track (guardCrossM, im * dM);  track (guardCrossS, is * dS);

    // correlation r = (M^2 - S^2) / (M^2 + S^2), so r >= floor  <=>  S^2 <= ratio M^2 with
    // ratio = (1 - floor) / (1 + floor). The input's own width is allowed too, but the effect
    // may not take it past r = 0 (ratio 1).
    constexpr double floorRatio = (1.0 - kGuardFloor) / (1.0 + kGuardFloor);
    const double ratio = std::clamp (guardInS / (guardInM + 1.0e-12), floorRatio, 1.0);

    // powers with the effect's mid / side scaled by gm / gs
    auto midPower  = [this] (double gm) { return guardInM + 2.0 * gm * guardCrossM + gm * gm * guardDM; };
    auto sidePower = [this] (double gs) { return guardInS + 2.0 * gs * guardCrossS + gs * gs * guardDS; };
    auto fits = [&] (double gm, double gs) { return sidePower (gs) <= ratio * midPower (gm) + 1.0e-15; };

    // largest g in 0..1 for which fits holds (fits (0) is the input itself)
    auto largest = [] (auto&& ok)
    {
        if (ok (1.0)) return 1.0;
        if (! ok (0.0)) return 0.0;
        double lo = 0.0, hi = 1.0;
        for (int i = 0; i < 12; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            (ok (mid) ? lo : hi) = mid;
        }
        return lo;
    };
    const double wantS = largest ([&] (double g) { return fits (1.0, g); });
    const double wantM = wantS > 0.0 ? 1.0 : largest ([&] (double g) { return fits (g, 0.0); });

    auto follow = [this] (double& gain, double want) { gain += (want - gain) * (want < gain ? guardDown : guardUp); };
    follow (sideGain, wantS);
    follow (midGain, wantM);

    // exactly 0 where the effect changed nothing
    const float deltaM = (float) ((midGain - 1.0) * dM), deltaS = (float) ((sideGain - 1.0) * dS);
    outL += deltaM + deltaS;
    outR += deltaM - deltaS;
}

// Keep the output as loud as the input, whatever the voice count, mixes, widener or dimension
// do to it. Both sides are K-weighted, so added highs (drive, lo-fi) count as louder, the way
// the ear hears them.
void Engine::loudnessMatch (float l, float r, float& outL, float& outR)
{
    auto kWeight = [] (Biquad* f, float x) { return (double) f[1].process (f[0].process (x)); };
    const float monoR = nCh > 1 ? outR : outL;   // mono: only the left channel is output
    const double kil = kWeight (kwIn[0], l), kir = kWeight (kwIn[1], r);
    const double kol = kWeight (kwOut[0], outL), kor = kWeight (kwOut[1], monoR);
    powIn  += (0.5 * (kil * kil + kir * kir) - powIn) * powCoef;
    powOut += (0.5 * (kol * kol + kor * kor) - powOut) * powCoef;
    if (powIn > 1.0e-9 && powOut > 1.0e-12)
    {
        const double want = std::min (2.0, std::max (0.08, std::sqrt (powIn / powOut)));
        normGain += (want - normGain) * (want < normGain ? attackCoef : gainCoef);
    }
    outL *= (float) normGain;
    outR *= (float) normGain;
}

// instantaneous pitch offset of each voice
void Engine::updateViz (const Block& b)
{
    for (int v = 0; v < kMaxVoices; ++v)
    {
        if (v < b.voices)
        {
            const double ratio = 1.0 - b.amp[v] * b.omega[v] * std::sin (theta[(size_t) v]);
            vizCents[v].store ((float) (1200.0 * std::log2 (std::max (0.5, ratio))), std::memory_order_relaxed);
        }
        else
            vizCents[v].store (0.0f, std::memory_order_relaxed);
    }
}

//==============================================================================
// Spacey modes

void Engine::setupWide()
{
    xoL.set (150.0, sr); xoR.set (150.0, sr);
    dcFold.highpass (20.0, sr);
    combHp.highpass (200.0, sr);
    const double fc[7] = { 150.0, 300.0, 600.0, 1200.0, 2400.0, 4800.0, 9000.0 };
    for (int k = 0; k < 7; ++k) prismXo[k].set (fc[k], sr);
    for (int k = 0; k < 8; ++k)
    {
        prismPhase[k] = twoPi * std::fmod (0.61803398875 * (k + 1), 1.0);
        prismInc[k] = twoPi * (0.11 + 0.065 * k) / sr;
    }
    crushRatioL = std::min (1.0, 14000.0 / sr);
    crushRatioR = std::min (1.0, 9000.0 / sr);

    // Phase Smear coefficients are tuned for 1x. A first order allpass (a + z^-1) / (1 + a z^-1)
    // has its -90 degree point where tan (pi f / fs) = (1 + a) / (1 - a); this table moves every
    // coefficient to the same frequency at the oversampled rate.
    for (int i = 0; i < kSmearTable; ++i)
    {
        const double a = -0.95 + 1.9 * (double) i / (double) (kSmearTable - 1);
        const double t = std::tan (std::atan ((1.0 + a) / (1.0 - a)) / (double) os);
        smearTable[(size_t) i] = (float) ((t - 1.0) / (t + 1.0));
    }
}

void Engine::resetWide()
{
    wideLine.clear();
    xoL.reset(); xoR.reset(); dcFold.reset(); combHp.reset(); driveEnv = 0.05;
    for (auto& a : smearL) a = {};
    for (auto& a : smearR) a = {};
    hilL.reset(); hilR.reset();
    for (auto& x : prismXo) x.reset();
    crushAccL = crushAccR = 1.0; crushHoldL = crushHoldR = 0.0f;
}

float Engine::smearCoef (float a) const   // a in -0.95..0.95 (1x value) -> value for the current rate
{
    const float p = (a + 0.95f) / 1.9f * (float) (kSmearTable - 1);
    const int i = std::max (0, std::min (kSmearTable - 2, (int) p));
    const float f = p - (float) i;
    return smearTable[(size_t) i] + f * (smearTable[(size_t) i + 1] - smearTable[(size_t) i]);
}

float Engine::crush (float x, int bits)
{
    const float lv = (float) (1 << (bits - 1));
    return std::round (x * lv) / lv;
}

void Engine::wideProcess (float L, float R, float& oL, float& oR)
{
    switch (wideActive)
    {
        case 0:  comb (L, R, oL, oR); break;
        case 1:  twinDrive (L, R, oL, oR); break;
        case 2:  phaseSmear (L, R, oL, oR); break;
        case 3:  freqShift (L, R, oL, oR); break;
        case 4:  prism (L, R, oL, oR); break;
        default: loFiTwins (L, R, oL, oR); break;
    }
}

// staggered comb peaks, added left and subtracted right: the mono sum is unchanged
void Engine::comb (float L, float R, float& oL, float& oR)
{
    const double f = std::pow (2.0, (spcS - 0.5) * 2.0);   // delays 0.5x .. 2x
    const float c = 0.5f * wideLine.read (0.00083 * f * sr) + 0.3f * wideLine.read (0.00137 * f * sr);
    oL = L + c;
    oR = R - c;
}

// a different waveshaper on each side, bass stays mono
void Engine::twinDrive (float L, float R, float& oL, float& oR)
{
    float ll, lh, rl, rh;
    xoL.split (L, ll, lh);
    xoR.split (R, rl, rh);
    const float bass = 0.5f * (ll + rl);
    // drive follows the input level, so the character does not depend on how loud the input is
    driveEnv += ((double) (0.5f * (std::fabs (lh) + std::fabs (rh))) - driveEnv) * driveCoef;
    const float kb = (float) std::min (8.0, std::max (0.5, 0.2 / (driveEnv + 1.0e-4)));
    const float k = kb * (float) std::pow (2.0, (spcS - 0.5) * 3.0);   // spacing = drive; output scale follows kb only
    const float a = std::tanh (5.0f * k * lh) / (2.8f * kb);
    const float b = dcFold.process (std::sin (11.0f * k * rh + 0.6f) / (2.8f * kb));
    oL = bass + a;
    oR = bass + b;
}

// two different, slowly moving allpass chains
void Engine::phaseSmear (float L, float R, float& oL, float& oR)
{
    smearPhase += twoPi * 0.17 * std::pow (2.0, (spcS - 0.5) * 4.0) / sr;
    if (smearPhase >= twoPi) smearPhase -= twoPi;
    static constexpr float cl[4] = { 0.80f, 0.40f, -0.20f, -0.60f };
    static constexpr float cr[4] = { 0.60f, 0.10f, -0.50f, -0.85f };
    float yl = L, yr = R;
    for (int k = 0; k < 4; ++k)
    {
        const float dep = (float) (0.16 * spcS);
        const float ml = dep * (float) std::sin (smearPhase + 0.9 * k);
        const float mr = dep * (float) std::sin (smearPhase * 1.13 + 1.3 * k + 2.0);
        yl = smearL[(size_t) k].process (yl, smearCoef (std::max (-0.95f, std::min (0.95f, cl[k] + ml))));
        yr = smearR[(size_t) k].process (yr, smearCoef (std::max (-0.95f, std::min (0.95f, cr[k] + mr))));
    }
    oL = yl;
    oR = yr;
}

// +2.2 Hz left, -2.2 Hz right, bass untouched and mono
void Engine::freqShift (float L, float R, float& oL, float& oR)
{
    float ll, lh, rl, rh;
    xoL.split (L, ll, lh);
    xoR.split (R, rl, rh);
    fsPhase += twoPi * (0.2 + 4.0 * spcS) / sr;
    if (fsPhase >= twoPi) fsPhase -= twoPi;
    const double cs = std::cos (fsPhase), sn = std::sin (fsPhase);
    double reL, imL, reR, imR;
    hilL.process (lh, reL, imL);
    hilR.process (rh, reR, imR);
    const float bass = 0.5f * (ll + rl);
    oL = bass + (float) (reL * cs - imL * sn);
    oR = bass + (float) (reR * cs + imR * sn);
}

// every band of the mid signal pans on its own LFO
void Engine::prism (float L, float R, float& oL, float& oR)
{
    const float M = 0.5f * (L + R), S = 0.5f * (L - R);
    float rest = M, sumL = 0.0f, sumR = 0.0f;
    for (int k = 0; k < 8; ++k)
    {
        float band;
        // Linkwitz-Riley split: neighbouring bands stay in phase where they overlap, so
        // panning them apart never cancels in mono
        if (k < 7) { float hi; prismXo[k].split (rest, band, hi); rest = hi; }
        else band = rest;

        double pan = 0.0;
        if (k > 0)   // lowest band stays centred
        {
            prismPhase[k] += prismInc[k];
            if (prismPhase[k] >= twoPi) prismPhase[k] -= twoPi;
            pan = std::min (0.98, 1.7 * spcS) * std::sin (prismPhase[k]);
        }
        const double ang = (pan + 1.0) * 0.25 * 3.14159265358979;
        sumL += band * (float) (std::cos (ang) * 1.41421356);
        sumR += band * (float) (std::sin (ang) * 1.41421356);
    }
    oL = S + sumL;
    oR = -S + sumR;
}

// different bit depth and sample rate per side, bass stays mono
void Engine::loFiTwins (float L, float R, float& oL, float& oR)
{
    const int crushBitsL = std::max (3, std::min (12, (int) std::lround (4.0 + 6.0 * (1.0 - spcS))));
    const int crushBitsR = std::max (2, crushBitsL - 2);
    float ll, lh, rl, rh;
    xoL.split (L, ll, lh);
    xoR.split (R, rl, rh);
    crushAccL += crushRatioL; if (crushAccL >= 1.0) { crushAccL -= 1.0; crushHoldL = crush (lh, crushBitsL); }
    crushAccR += crushRatioR; if (crushAccR >= 1.0) { crushAccR -= 1.0; crushHoldR = crush (rh, crushBitsR); }
    const float bass = 0.5f * (ll + rl);
    oL = bass + crushHoldL;
    oR = bass + crushHoldR;
}

} // namespace hd
