// Crossover.h  -  multiband splitting for Neptune MB
//  * Minimum phase: Linkwitz-Riley tree (6 dB = complementary 1st order, 12 = LR2, 24 = LR4, 48 = LR8)
//    with all-pass compensation so the bands always sum back to a flat magnitude.
//  * Linear phase: zero-phase FIR low-passes with the same |LR| magnitude, band k = LP_k - LP_(k-1).
#pragma once
#include <juce_dsp/juce_dsp.h>
#include "StyleEngine.h"

namespace nep
{
constexpr int kMaxBands = 6;

// A first-order section  (b0 + b1 z^-1) / (1 + a1 z^-1)
struct OnePole
{
    double b0 = 1, b1 = 0, a1 = 0, z = 0;
    void reset() { z = 0; }
    inline double process (double x) { const double y = b0 * x + z; z = b1 * x - a1 * y; return y; }
    void lowpass  (double fs, double f) { const double k = std::tan (M_PI * f / fs); b0 = b1 = k / (1 + k); a1 = (k - 1) / (1 + k); }
    void highpass (double fs, double f) { const double k = std::tan (M_PI * f / fs); b0 = 1 / (1 + k); b1 = -b0; a1 = (k - 1) / (1 + k); }
    void allpass  (double fs, double f) { const double k = std::tan (M_PI * f / fs); const double c = (k - 1) / (k + 1); b0 = c; b1 = 1; a1 = c; }
};

// One crossover point for one channel
struct CrossoverPoint
{
    int slope = 24;      // dB/oct
    double f = 1000;
    OnePole lp1, hp1, lp1b, hp1b, ap1;
    Biquad lp[4], hp[4], ap[2];

    void set (double fs, double freq, int newSlope)
    {
        f = freq; slope = newSlope;
        lp1.lowpass (fs, f); lp1b.lowpass (fs, f); hp1.highpass (fs, f); hp1b.highpass (fs, f); ap1.allpass (fs, f);
        if (slope == 24)
        {
            for (int i = 0; i < 2; ++i) { lp[i].lowpass (fs, f, 0.70710678); hp[i].highpass (fs, f, 0.70710678); }
            setAllpass (ap[0], fs, f, 0.70710678);
        }
        else if (slope == 48)
        {
            const double q[2] = { 0.54119610, 1.30656296 };
            for (int i = 0; i < 4; ++i) { lp[i].lowpass (fs, f, q[i & 1]); hp[i].highpass (fs, f, q[i & 1]); }
            setAllpass (ap[0], fs, f, q[0]); setAllpass (ap[1], fs, f, q[1]);
        }
    }
    static void setAllpass (Biquad& b, double fs, double f, double q)
    {
        const double w = 2 * M_PI * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        b.set (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
    }
    void reset() { lp1.reset(); hp1.reset(); lp1b.reset(); hp1b.reset(); ap1.reset(); for (auto& b : lp) b.reset(); for (auto& b : hp) b.reset(); for (auto& b : ap) b.reset(); }

    inline void split (double x, double& lo, double& hi)
    {
        switch (slope)
        {
            case 6:  lo = lp1.process (x); hi = x - lo; break;
            case 12: lo = lp1b.process (lp1.process (x)); hi = -hp1b.process (hp1.process (x)); break;
            case 48: lo = lp[3].process (lp[2].process (lp[1].process (lp[0].process (x))));
                     hi = hp[3].process (hp[2].process (hp[1].process (hp[0].process (x)))); break;
            default: lo = lp[1].process (lp[0].process (x)); hi = hp[1].process (hp[0].process (x)); break;
        }
    }
    // The phase that the split adds, applied to bands that did not go through this split.
    // (Separate state from split(): it is used on other signals.)
    struct AP { OnePole a1; Biquad b[2]; };
    inline double allpass (AP& s, double x) const
    {
        switch (slope)
        {
            case 6:  return x;
            case 12: return s.a1.process (x);
            case 48: return s.b[1].process (s.b[0].process (x));
            default: return s.b[0].process (x);
        }
    }
    void configureAP (AP& s) const { s.a1 = ap1; s.b[0].b0 = ap[0].b0; s.b[0].b1 = ap[0].b1; s.b[0].b2 = ap[0].b2; s.b[0].a1 = ap[0].a1; s.b[0].a2 = ap[0].a2;
                                      s.b[1].b0 = ap[1].b0; s.b[1].b1 = ap[1].b1; s.b[1].b2 = ap[1].b2; s.b[1].a1 = ap[1].a1; s.b[1].a2 = ap[1].a2; }
};

class MinPhaseSplitter
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); for (int c = 0; c < 2; ++c) for (int k = 0; k < kMaxBands - 1; ++k) lastF[k] = -1; }
    void reset()
    {
        for (int c = 0; c < 2; ++c) for (int k = 0; k < kMaxBands - 1; ++k)
        {
            xo[c][k].reset();
            for (int b = 0; b < kMaxBands; ++b) { aps[c][k][b].a1.reset(); for (auto& q : aps[c][k][b].b) q.reset(); }
        }
    }
    // numBands 1..6, freqs ascending (numBands-1 values), slopes per crossover
    void setup (int numBands, const double* freqs, const int* slopes)
    {
        nb = numBands;
        for (int k = 0; k < nb - 1; ++k)
        {
            if (std::fabs (freqs[k] - lastF[k]) < 1e-6 && slopes[k] == lastS[k]) continue;
            lastF[k] = freqs[k]; lastS[k] = slopes[k];
            for (int c = 0; c < 2; ++c)
            {
                // keep the filter states, swap the coefficients -> click-free when the split moves
                auto& p = xo[c][k];
                CrossoverPoint tmp; tmp.set (fs, freqs[k], slopes[k]);
                copyCoeffs (p, tmp);
                for (int b = 0; b < kMaxBands; ++b) copyAP (aps[c][k][b], tmp);
            }
        }
    }
    // in: one channel sample, out: per band sample
    inline void process (int ch, double x, double* out)
    {
        double rem = x;
        for (int k = 0; k < nb - 1; ++k)
        {
            double lo, hi; xo[ch][k].split (rem, lo, hi);
            out[k] = lo; rem = hi;
        }
        out[nb - 1] = rem;
        // phase-align: band b passes through all-passes of crossovers k > b
        for (int b = 0; b < nb - 1; ++b)
            for (int k = b + 1; k < nb - 1; ++k)
                out[b] = xo[ch][k].allpass (aps[ch][k][b], out[b]);
    }

private:
    static void copyBq (Biquad& d, const Biquad& s) { d.b0 = s.b0; d.b1 = s.b1; d.b2 = s.b2; d.a1 = s.a1; d.a2 = s.a2; }
    static void copyOp (OnePole& d, const OnePole& s) { d.b0 = s.b0; d.b1 = s.b1; d.a1 = s.a1; }
    static void copyCoeffs (CrossoverPoint& d, const CrossoverPoint& s)
    {
        const bool slopeChanged = d.slope != s.slope;
        d.slope = s.slope; d.f = s.f;
        copyOp (d.lp1, s.lp1); copyOp (d.lp1b, s.lp1b); copyOp (d.hp1, s.hp1); copyOp (d.hp1b, s.hp1b); copyOp (d.ap1, s.ap1);
        for (int i = 0; i < 4; ++i) { copyBq (d.lp[i], s.lp[i]); copyBq (d.hp[i], s.hp[i]); }
        for (int i = 0; i < 2; ++i) copyBq (d.ap[i], s.ap[i]);
        if (slopeChanged) d.reset();
    }
    static void copyAP (CrossoverPoint::AP& d, const CrossoverPoint& s)
    { copyOp (d.a1, s.ap1); copyBq (d.b[0], s.ap[0]); copyBq (d.b[1], s.ap[1]); }

    double fs = 48000;
    int nb = 1;
    double lastF[kMaxBands - 1] {};
    int lastS[kMaxBands - 1] {};
    CrossoverPoint xo[2][kMaxBands - 1];
    CrossoverPoint::AP aps[2][kMaxBands - 1][kMaxBands];
};

// Zero-phase FIR low-pass with |H| = 1 / (1 + (f/fc)^n), n = slope / 6  -> complementary bands.
inline void designLinearPhaseLP (std::vector<float>& h, int len, double fs, double fc, int slope)
{
    const int nfft = 1 << 15;     // fine frequency grid
    const double n = slope / 6.0;
    std::vector<std::complex<float>> spec (nfft), time (nfft);
    for (int i = 0; i < nfft; ++i)
    {
        const int k = i <= nfft / 2 ? i : nfft - i;
        const double f = fs * k / nfft;
        spec[i] = { (float) (1.0 / (1.0 + std::pow (f / fc, n))), 0.0f };
    }
    juce::dsp::FFT fft (15);
    fft.perform (spec.data(), time.data(), true);
    h.assign ((size_t) len, 0.0f);
    const int half = len / 2;
    double sum = 0;
    for (int i = -half; i <= half; ++i)
    {
        const double w = 0.5 * (1 + std::cos (M_PI * i / (half + 1)));   // Hann window
        const float v = time[(size_t) ((i + nfft) % nfft)].real();
        h[(size_t) (i + half)] = (float) (v * w);
        sum += v * w;
    }
    if (sum != 0) for (auto& v : h) v = (float) (v / sum);              // exact unity at DC
}
} // namespace nep
