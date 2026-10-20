#pragma once
// Small DSP building blocks used by the engine (no JUCE needed)

#include <algorithm>
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

} // namespace hd
