#pragma once
// ============================================================================
//  HyperDimEngine - own take on a "Hyper / Dimension" effect (no JUCE needed)
//
//  Each section is off (skipped) while its mix / amount is at 0.
//
//  HYPER      1-17 voices tapping one short delay line. Every voice's delay time
//             is swept by its own slow oscillator, which makes its pitch drift
//             sharp and flat (delay change = pitch change). The voices beat
//             against each other like a unison. Retrig resets all voices to
//             zero pitch offset on note-on ("laser zap").
//  DIMENSION  Short, slowly modulated stereo delay taps from the mid signal,
//             added with opposite polarity left/right for width.
//  WIDE       Widener with six modes (all keep the bass below ~150 Hz mono where
//             that matters):
//             COMB         two short delays added left / subtracted right, staggered
//                          comb peaks, mono sum exactly unchanged
//             TWIN DRIVE   different distortion per channel (tanh left, sine fold right)
//             PHASE SMEAR  different slowly moving allpass chains per channel
//             FREQ SHIFT   +/- 2.2 Hz single-sideband shift per channel (binaural beat)
//             PRISM        8-band mid signal, every band auto-pans on its own LFO
//             LO-FI TWINS  different bit depth + sample rate per channel
//  OUTPUT     Global dry/wet, global bass mono, a phase guard (keeps the correlation the
//             effect causes above +0.2), then a loudness match (K-weighted, ITU-R BS.1770)
//             that keeps the output as loud as the input.
//
//  The engine can run oversampled (the host side up/downsamples); pass the factor
//  to prepare() so the rate dependent parts sound the same at every factor.
// ============================================================================

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace hd
{

class DelayLine
{
public:
    // keeps the existing buffer when it is already big enough (no allocation)
    void init (int maxSamples)
    {
        if ((int) buf.size() >= maxSamples + 8) { clear(); w = 0; return; }
        int n = 1;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // x[n - d], d >= 2 samples (fractional), 4-point Hermite
    float read (double d) const
    {
        const double pos = (double) (w - 1) - d;
        const double fl = std::floor (pos);
        const float f = (float) (pos - fl);
        const int i = (int) fl;
        const float y0 = buf[(size_t) ((i - 1) & mask)];
        const float y1 = buf[(size_t) (i & mask)];
        const float y2 = buf[(size_t) ((i + 1) & mask)];
        const float y3 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (double fc, double sr, double q = 0.70710678)
    {
        const double w = 6.283185307179586 * fc / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
        b0 = (1.0 - c) * 0.5 / a0; b1 = (1.0 - c) / a0; b2 = b0; a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
    }
    void highpass (double fc, double sr, double q = 0.70710678)
    {
        const double w = 6.283185307179586 * fc / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
        b0 = (1.0 + c) * 0.5 / a0; b1 = -(1.0 + c) / a0; b2 = b0; a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
    }
    // ITU-R BS.1770 K-weighting, stage 1: high shelf (+4 dB above ~1.7 kHz)
    void kShelf (double sr)
    {
        const double K = std::tan (3.141592653589793 * 1681.974450955533 / sr), Q = 0.7071752369554196;
        const double Vh = std::pow (10.0, 3.999843853973347 / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        b0 = (Vh + Vb * K / Q + K * K) / a0; b1 = 2.0 * (K * K - Vh) / a0; b2 = (Vh - Vb * K / Q + K * K) / a0;
        a1 = 2.0 * (K * K - 1.0) / a0; a2 = (1.0 - K / Q + K * K) / a0;
    }
    // stage 2: RLB high pass (~38 Hz)
    void kHighpass (double sr)
    {
        const double K = std::tan (3.141592653589793 * 38.13547087602444 / sr), Q = 0.5003270373238773;
        const double a0 = 1.0 + K / Q + K * K;
        b0 = 1.0; b1 = -2.0; b2 = 1.0; a1 = 2.0 * (K * K - 1.0) / a0; a2 = (1.0 - K / Q + K * K) / a0;
    }
    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0.0; }
};

// Linkwitz-Riley 4th order split: low + high sum back to a flat magnitude
struct Crossover
{
    Biquad lp[2], hp[2];
    void set (double fc, double sr) { for (auto& b : lp) b.lowpass (fc, sr); for (auto& b : hp) b.highpass (fc, sr); }
    void reset() { for (auto& b : lp) b.reset(); for (auto& b : hp) b.reset(); }
    void split (float x, float& low, float& high)
    {
        low  = lp[1].process (lp[0].process (x));
        high = hp[1].process (hp[0].process (x));
    }
};

struct Allpass1
{
    float z = 0.0f;
    float process (float x, float a) { const float y = a * x + z; z = x - a * y; return y; }
};

struct Allpass2   // (a^2 - z^-2) / (1 - a^2 z^-2), used for the Hilbert pair
{
    double a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    double process (double x)
    {
        const double y = a2 * (x + y2) - x2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
    void reset() { x1 = x2 = y1 = y2 = 0.0; }
};

// 90 degree phase pair (Niemitalo 8-pole allpass network), good from ~40 Hz to ~20 kHz
struct Hilbert
{
    Allpass2 a[4], b[4];
    double prev = 0.0;
    Hilbert()
    {
        const double ca[4] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
        const double cb[4] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
        for (int i = 0; i < 4; ++i) { a[i].a2 = ca[i] * ca[i]; b[i].a2 = cb[i] * cb[i]; }
    }
    void reset() { for (auto& s : a) s.reset(); for (auto& s : b) s.reset(); prev = 0.0; }
    void process (double x, double& re, double& im)
    {
        double u = prev, v = x;
        for (auto& s : a) u = s.process (u);
        for (auto& s : b) v = s.process (v);
        prev = x;
        re = u; im = v;
    }
};

class Engine
{
public:
    static constexpr int kMaxVoices = 17;
    static constexpr int kNumWideModes = 6;

    struct Params
    {
        int   unison = 4;          // 1..17 voices
        float rateHz = 0.8f;       // how fast the voices' pitch oscillates
        float detuneCents = 20.0f; // how far (peak) the pitch oscillates
        float spread = 0.7f;       // 0..1 stereo spread of the voices
        float hyperMix = 0.5f;     // 0..1
        float dimSize = 0.5f;      // 0..1
        float dimMix = 0.3f;       // 0..1
        bool  wide = false;        // widener on/off
        int   wideMode = 0;        // 0 comb, 1 twin drive, 2 phase smear, 3 freq shift, 4 prism, 5 lo-fi twins
        float wideAmount = 1.0f;   // 0..1 how much of the widener is blended in
        float wideSpacing = 0.5f;  // 0..1 mode specific; 0.5 = the original setting of every mode
        float bassMonoHz = 20.0f;  // global: everything below is made mono; <= 20 = off
        float globalMix = 1.0f;    // global dry/wet 0..1
        bool  phaseGuard = true;   // keep the correlation the effect adds above kGuardFloor
    };

    std::atomic<float> vizCents[kMaxVoices];

    Engine() { for (auto& a : vizCents) a.store (0.0f); }

    // sampleRate is the rate the engine runs at (already multiplied by the oversampling factor).
    // Call it once with the highest rate first: later calls at lower rates do not allocate.
    void prepare (double sampleRate, int channels, int oversampling = 1)
    {
        sr = sampleRate;
        os = std::max (1, oversampling);
        nCh = std::max (1, std::min (2, channels));
        const int maxD = (int) std::ceil (0.10 * sr);
        for (auto& l : line) l.init (maxD);
        dimLine.init ((int) std::ceil (0.05 * sr));
        wideLine.init ((int) std::ceil (0.005 * sr));
        setupWide();
        tauCoef = 1.0 - std::exp (-1.0 / (0.002 * sr));
        powCoef = 1.0 - std::exp (-1.0 / (0.30 * sr));   // loudness window ~300 ms
        guardPowCoef = 1.0 - std::exp (-1.0 / (0.05 * sr));  // phase guard: ~50 ms, quicker than the meter
        guardDown = 1.0 - std::exp (-1.0 / (0.01 * sr));     // narrows quickly
        guardUp = 1.0 - std::exp (-1.0 / (0.40 * sr));       // and widens again slowly
        gainCoef = 1.0 - std::exp (-1.0 / (0.12 * sr));  // gain rises slowly (~120 ms)
        attackCoef = 1.0 - std::exp (-1.0 / (0.008 * sr)); // and falls fast (~8 ms) so jumps never overshoot
        mixCoef = 1.0 - std::exp (-1.0 / (0.02 * sr));
        spaceCoef = 1.0 - std::exp (-1.0 / (0.03 * sr));
        offDecay = std::exp (-1.0 / (0.4 * sr));
        driveCoef = 1.0 - std::exp (-1.0 / (0.1 * sr));
        for (int c = 0; c < 2; ++c)
            for (auto* k : { kwIn[c], kwOut[c] }) { k[0].kShelf (sr); k[1].kHighpass (sr); }
        reset();
    }

    void reset()
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
        guardInM = guardInS = guardOutM = guardOutS = 0.0;
        sideGain = 1.0;
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

    // All voices restart from zero pitch offset and diverge again from the same point.
    // The delay time itself stays continuous (it relaxes slowly to the new sweep),
    // so there is no click, only a short pitch glide.
    void retrig()
    {
        for (int v = 0; v < kMaxVoices; ++v)
        {
            theta[(size_t) v] = 0.0;
            tauOff[(size_t) v] = std::max (0.0, lastTarget[(size_t) v] - baseTau);
        }
    }

    void setParams (const Params& p) { prm = p; }

    void process (float* const* io, int n)
    {
        const int N = std::max (0, std::min (kMaxVoices, prm.unison));
        static constexpr double rateMul[kMaxVoices] = { 1.00, 1.07, 0.93, 1.13, 0.89, 1.19, 0.83, 1.25, 0.78,
                                                        1.31, 0.74, 1.37, 0.70, 1.43, 0.66, 1.49, 0.62 };
        static constexpr double ampMul[kMaxVoices]  = { 1.00, 0.85, 0.70, 0.92, 0.60, 0.78, 0.50, 0.88, 0.66,
                                                        0.95, 0.55, 0.74, 0.82, 0.58, 0.90, 0.64, 0.72 };

        double amp[kMaxVoices], inc[kMaxVoices], omega[kMaxVoices], gl[kMaxVoices], gr[kMaxVoices];
        for (int v = 0; v < N; ++v)
        {
            const double r = std::max (0.001, (double) prm.rateHz * rateMul[v]);
            omega[v] = twoPi * r;
            inc[v] = omega[v] / sr;
            const double c = std::pow (2.0, (double) prm.detuneCents * ampMul[v] / 1200.0) - 1.0;
            amp[v] = std::min (0.03, c / omega[v]);   // seconds of delay sweep
            const double s = N > 1 ? ((double) v / (double) (N - 1)) * 2.0 - 1.0 : 0.0;
            const double sig = s * (double) prm.spread;  // -1..1
            const double ang = (sig + 1.0) * 0.25 * 3.14159265358979;
            gl[v] = std::cos (ang) * 1.41421356;
            gr[v] = std::sin (ang) * 1.41421356;
        }
        const float wetScale = N > 0 ? 1.0f / std::sqrt ((float) N) : 0.0f;

        const double dimBase1 = (0.003 + 0.012 * (double) prm.dimSize);
        const double dimBase2 = (0.0045 + 0.014 * (double) prm.dimSize);
        const double dimDepth = (0.0004 + 0.0016 * (double) prm.dimSize);
        const double dimInc = twoPi * 0.35 / sr;

        auto kWeight = [] (Biquad* f, float x) { return (double) f[1].process (f[0].process (x)); };

        const bool bassOn = prm.bassMonoHz > 20.5f;
        if (bassOn && std::fabs ((double) prm.bassMonoHz - bmFreq) > 0.01)
        {
            bmFreq = (double) prm.bassMonoHz;
            bmL.set (bmFreq, sr); bmR.set (bmFreq, sr);
        }
        if (! bassOn) bmFreq = -1.0;

        for (int i = 0; i < n; ++i)
        {
            float l = io[0][i];
            float r = nCh > 1 ? io[1][i] : l;
            line[0].push (l);
            line[1].push (r);

            hmS += ((double) prm.hyperMix - hmS) * mixCoef;
            dmS += ((double) prm.dimMix - dmS) * mixCoef;

            float outL = l, outR = r;
            if (N > 0 && (hmS > 1.0e-4 || prm.hyperMix > 0.0f))   // Hyper off at mix 0: skip the voices
            {
                float wl = 0.0f, wr = 0.0f;
                for (int v = 0; v < N; ++v)
                {
                    auto& th = theta[(size_t) v];
                    th += inc[v];
                    if (th >= twoPi) th -= twoPi;
                    auto& off = tauOff[(size_t) v];
                    off *= offDecay;
                    const double target = baseTau + amp[v] * (1.0 - std::cos (th)) + off;
                    lastTarget[(size_t) v] = target;
                    auto& t = tau[(size_t) v];
                    t += (target - t) * tauCoef;
                    const double d = std::max (2.0, t * sr);
                    wl += line[0].read (d) * (float) gl[v];
                    wr += line[1].read (d) * (float) gr[v];
                }
                wl *= wetScale; wr *= wetScale;
                const float m = (float) hmS;
                outL = l + m * (wl - l);
                outR = r + m * (wr - r);
            }

            // ---- Dimension --------------------------------------------------
            dimLine.push (0.5f * (outL + outR));
            dimPhase += dimInc;
            if (dimPhase >= twoPi) dimPhase -= twoPi;
            const double lfo = std::sin (dimPhase);
            const float dl = dimLine.read (std::max (2.0, (dimBase1 + dimDepth * lfo) * sr));
            const float dr = dimLine.read (std::max (2.0, (dimBase2 - dimDepth * lfo) * sr));
            const float dg = 0.8f * (float) dmS;
            outL += dg * dl;
            outR -= dg * dr;

            // ---- Wide ------------------------------------------------------------
            wideLine.push (combHp.process (0.5f * (outL + outR)));   // bass stays out of the comb
            {
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
                if (wideS > 0.0005)
                {
                    float wl = outL, wr = outR;
                    wideProcess (outL, outR, wl, wr);
                    const float amt = (float) (wideS * amtS);
                    outL += amt * (wl - outL);
                    outR += amt * (wr - outR);
                }
            }

            // ---- Global dry/wet (before bass mono and the loudness match, so the level stays put)
            gmS += ((double) prm.globalMix - gmS) * mixCoef;
            outL = l + (float) gmS * (outL - l);
            outR = r + (float) gmS * (outR - r);

            // ---- Global bass mono: everything below the cutoff is summed to mono
            if (bassOn)
            {
                float ll, lh, rl, rh;
                bmL.split (outL, ll, lh);
                bmR.split (outR, rl, rh);
                const float bass = 0.5f * (ll + rl);
                outL = bass + lh;
                outR = bass + rh;
            }

            // ---- Phase guard: if the effect pushes the correlation below kGuardFloor, turn the side
            //      signal down until it is back there. Width the input already had is left alone,
            //      and nothing changes while the correlation is fine.
            if (prm.phaseGuard && nCh > 1)
            {
                const double im = 0.5 * ((double) l + r), is = 0.5 * ((double) l - r);
                const double om = 0.5 * ((double) outL + outR), os = 0.5 * ((double) outL - outR);
                guardInM  += (im * im - guardInM)  * guardPowCoef;
                guardInS  += (is * is - guardInS)  * guardPowCoef;
                guardOutM += (om * om - guardOutM) * guardPowCoef;
                guardOutS += (os * os - guardOutS) * guardPowCoef;

                // correlation r = (M^2 - S^2) / (M^2 + S^2), so r >= floor  <=>  S^2 / M^2 <= (1 - floor) / (1 + floor)
                constexpr double floorRatio = (1.0 - kGuardFloor) / (1.0 + kGuardFloor);
                const double allowed = std::max (floorRatio, guardInS / (guardInM + 1.0e-12));
                const double want = guardOutS > allowed * guardOutM ? std::sqrt (allowed * guardOutM / (guardOutS + 1.0e-12)) : 1.0;
                sideGain += (want - sideGain) * (want < sideGain ? guardDown : guardUp);

                const double side = os * sideGain;
                outL = (float) (om + side);
                outR = (float) (om - side);
            }

            // ---- Loudness match: keep the output as loud as the input, whatever the voice
            //      count, mixes, widener or dimension do to it. Both sides are K-weighted, so
            //      added highs (drive, lo-fi) count as louder, the way the ear hears them.
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

            io[0][i] = outL;
            if (nCh > 1) io[1][i] = outR;
        }

        // visualisation: instantaneous pitch offset of each voice
        for (int v = 0; v < kMaxVoices; ++v)
        {
            if (v < N)
            {
                const double ratio = 1.0 - amp[v] * omega[v] * std::sin (theta[(size_t) v]);
                vizCents[v].store ((float) (1200.0 * std::log2 (std::max (0.5, ratio))), std::memory_order_relaxed);
            }
            else
                vizCents[v].store (0.0f, std::memory_order_relaxed);
        }
    }

private:
    // ---------------------------------------------------------------- wide modes
    void setupWide()
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

    float smearCoef (float a) const   // a in -0.95..0.95 (1x value) -> value for the current rate
    {
        const float p = (a + 0.95f) / 1.9f * (float) (kSmearTable - 1);
        const int i = std::max (0, std::min (kSmearTable - 2, (int) p));
        const float f = p - (float) i;
        return smearTable[(size_t) i] + f * (smearTable[(size_t) i + 1] - smearTable[(size_t) i]);
    }

    void resetWide()
    {
        wideLine.clear();
        xoL.reset(); xoR.reset(); dcFold.reset(); combHp.reset(); driveEnv = 0.05;
        for (auto& a : smearL) a = {};
        for (auto& a : smearR) a = {};
        hilL.reset(); hilR.reset();
        for (auto& x : prismXo) x.reset();
        crushAccL = crushAccR = 1.0; crushHoldL = crushHoldR = 0.0f;
    }

    static float crush (float x, int bits)
    {
        const float lv = (float) (1 << (bits - 1));
        return std::round (x * lv) / lv;
    }

    void wideProcess (float L, float R, float& oL, float& oR)
    {
        switch (wideActive)
        {
            case 0:   // COMB: staggered peaks, mono sum unchanged
            {
                const double f = std::pow (2.0, (spcS - 0.5) * 2.0);   // delays 0.5x .. 2x
                const float c = 0.5f * wideLine.read (0.00083 * f * sr) + 0.3f * wideLine.read (0.00137 * f * sr);
                oL = L + c;
                oR = R - c;
                break;
            }
            case 1:   // TWIN DRIVE: a different waveshaper on each side, bass stays mono
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
                break;
            }
            case 2:   // PHASE SMEAR: two different, slowly moving allpass chains
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
                break;
            }
            case 3:   // FREQ SHIFT: +2.2 Hz left, -2.2 Hz right, bass untouched and mono
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
                break;
            }
            case 4:   // PRISM: every band of the mid signal pans on its own LFO
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
                break;
            }
            default:  // LO-FI TWINS: different bit depth and sample rate per side, bass stays mono
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
                break;
            }
        }
    }

    static constexpr double twoPi = 6.283185307179586;
    static constexpr double baseTau = 0.001;

    static constexpr int kSmearTable = 256;

    Params prm;
    double sr = 44100.0;
    int nCh = 2, os = 1;
    double tauCoef = 0.1, mixCoef = 0.01, offDecay = 0.9999;
    double hmS = 0.5, dmS = 0.3, wideS = 0.0, amtS = 1.0, spcS = 0.5, spaceCoef = 0.001, bmFreq = -1.0;
    Crossover bmL, bmR;
    double powCoef = 0.0001, gainCoef = 0.001, attackCoef = 0.01, powIn = 0.0, powOut = 0.0, normGain = 1.0;
    DelayLine line[2];
    DelayLine dimLine, wideLine;
    int wideActive = 0;
    Crossover xoL, xoR;
    Biquad dcFold, combHp;
    double driveEnv = 0.05, driveCoef = 0.0002;
    double gmS = 1.0;
    static constexpr double kGuardFloor = 0.2;   // lowest correlation the phase guard lets the effect cause
    double guardPowCoef = 0.0001, guardDown = 0.001, guardUp = 0.0001;
    double guardInM = 0.0, guardInS = 0.0, guardOutM = 0.0, guardOutS = 0.0, sideGain = 1.0;
    Biquad kwIn[2][2], kwOut[2][2];   // K-weighting for the loudness match, [channel][stage]
    std::array<float, kSmearTable> smearTable {};
    Allpass1 smearL[4], smearR[4];
    double smearPhase = 0.0, fsPhase = 0.0;
    Hilbert hilL, hilR;
    Crossover prismXo[7];
    double prismPhase[8] {}, prismInc[8] {};
    double crushAccL = 1.0, crushAccR = 1.0, crushRatioL = 0.3, crushRatioR = 0.2;
    float crushHoldL = 0.0f, crushHoldR = 0.0f;
    std::array<double, kMaxVoices> theta {}, tau {}, tauOff {}, lastTarget {};
    double dimPhase = 0.0;
};

} // namespace hd
