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
    output.prepare (sr, nCh);
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
    output.reset();
    amtS = (double) prm.wideAmount; spcS = (double) prm.wideSpacing;
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
    return b;
}

void Engine::process (float* const* io, int n)
{
    const Block block = prepareBlock();
    output.beginBlock();

    for (int i = 0; i < n; ++i)
    {
        const float l = io[0][i];
        const float r = nCh > 1 ? io[1][i] : l;

        float outL = l, outR = r;
        hyper (l, r, outL, outR, block);
        dimension (outL, outR, block);
        spacey (outL, outR);
        output.process (l, r, outL, outR);

        io[0][i] = outL;
        if (nCh > 1) io[1][i] = outR;
    }

    updateViz (block);
    output.endBlock();
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
