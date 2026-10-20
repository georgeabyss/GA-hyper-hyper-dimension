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
//  SPACEY     Widener with six modes (all keep the bass below ~150 Hz mono where
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
//  With every section off the output is the input, sample for sample.
//  The engine can run oversampled (the host side up/downsamples); pass the factor
//  to prepare() so the rate dependent parts sound the same at every factor.
// ============================================================================

#include <array>
#include <atomic>
#include "DspBlocks.h"

namespace hd
{

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
    void prepare (double sampleRate, int channels, int oversampling = 1);
    void reset();

    // All voices restart from zero pitch offset and diverge again from the same point.
    // The delay time itself stays continuous (it relaxes slowly to the new sweep),
    // so there is no click, only a short pitch glide.
    void retrig();

    void setParams (const Params& p) { prm = p; }
    void process (float* const* io, int n);

private:
    // values worked out once per block
    struct Block
    {
        int voices = 0;
        double amp[kMaxVoices], inc[kMaxVoices], omega[kMaxVoices], gainL[kMaxVoices], gainR[kMaxVoices];
        float wetScale = 0.0f;
        double dimBase1 = 0.0, dimBase2 = 0.0, dimDepth = 0.0, dimInc = 0.0;
        bool bassMono = false;
    };
    Block prepareBlock();

    // the stages, in signal order; l / r is the input sample, outL / outR the running result
    void hyper (float l, float r, float& outL, float& outR, const Block&);
    void dimension (float& outL, float& outR, const Block&);
    void spacey (float& outL, float& outR);
    void dryWet (float l, float r, float& outL, float& outR);
    void bassMono (float& outL, float& outR);
    void phaseGuard (float l, float r, float& outL, float& outR);
    void loudnessMatch (float l, float r, float& outL, float& outR);
    void updateViz (const Block&);

    // Spacey modes: (L, R) in, (oL, oR) out
    void setupWide();
    void resetWide();
    void wideProcess (float L, float R, float& oL, float& oR);
    void comb (float L, float R, float& oL, float& oR);
    void twinDrive (float L, float R, float& oL, float& oR);
    void phaseSmear (float L, float R, float& oL, float& oR);
    void freqShift (float L, float R, float& oL, float& oR);
    void prism (float L, float R, float& oL, float& oR);
    void loFiTwins (float L, float R, float& oL, float& oR);
    float smearCoef (float a) const;
    static float crush (float x, int bits);

    static constexpr double twoPi = 6.283185307179586;
    static constexpr double baseTau = 0.001;
    static constexpr int kSmearTable = 256;
    static constexpr double kGuardFloor = 0.2;   // lowest correlation the phase guard lets the effect cause

    Params prm;
    double sr = 44100.0;
    int nCh = 2, os = 1;

    // smoothing
    double mixCoef = 0.01, spaceCoef = 0.001;
    double hmS = 0.5, dmS = 0.3, wideS = 0.0, amtS = 1.0, spcS = 0.5, gmS = 1.0;

    // hyper
    DelayLine line[2];
    double tauCoef = 0.1, offDecay = 0.9999;
    std::array<double, kMaxVoices> theta {}, tau {}, tauOff {}, lastTarget {};

    // dimension
    DelayLine dimLine;
    double dimPhase = 0.0;

    // spacey
    int wideActive = 0;
    DelayLine wideLine;
    Crossover xoL, xoR;
    Biquad dcFold, combHp;
    double driveEnv = 0.05, driveCoef = 0.0002;
    std::array<float, kSmearTable> smearTable {};
    Allpass1 smearL[4], smearR[4];
    double smearPhase = 0.0, fsPhase = 0.0;
    Hilbert hilL, hilR;
    Crossover prismXo[7];
    double prismPhase[8] {}, prismInc[8] {};
    double crushAccL = 1.0, crushAccR = 1.0, crushRatioL = 0.3, crushRatioR = 0.2;
    float crushHoldL = 0.0f, crushHoldR = 0.0f;

    // output
    Crossover bmL, bmR;
    double bmFreq = -1.0;
    double guardPowCoef = 0.0001, guardDown = 0.001, guardUp = 0.0001;
    double guardInM = 0.0, guardInS = 0.0, guardDM = 0.0, guardDS = 0.0, guardCrossM = 0.0, guardCrossS = 0.0;
    double sideGain = 1.0, midGain = 1.0;   // how much of the effect's side / mid change is kept
    Biquad kwIn[2][2], kwOut[2][2];   // K-weighting for the loudness match, [channel][stage]
    double powCoef = 0.0001, gainCoef = 0.001, attackCoef = 0.01, powIn = 0.0, powOut = 0.0, normGain = 1.0;
};

} // namespace hd
