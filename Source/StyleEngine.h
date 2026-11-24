// StyleEngine.h  -  the distortion "styles" of Neptune MB.
// Plain C++ (no JUCE) so the very same code is used by the plug-in and by the
// offline fitting tool (fit/fit_styles.py) that tunes every style against
// published black-box measurements of the reference plug-in.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstring>

namespace nep
{
constexpr double kPi = 3.14159265358979323846;   // nep::kPi is not defined by MSVC

enum StyleKind : int { kGeneric = 0, kFoldback = 1, kRectify = 2, kDestroy = 3, kBreakdown = 4, kSmudge = 5, kTwoStage = 6 };

// One style = a small fixed signal chain whose numbers come from the fit.
//   pre high-pass -> pre peak -> drive gain -> (+bias) soft clipper -> post peak -> post low-pass (cab) -> DC block -> make-up
struct StyleDef
{
    int    kind;
    double g0, g100, gamma;        // drive gain in dB at 0 % and 100 %, curve exponent
    double knee;                   // soft-clip hardness (1 = very soft, 12 = almost hard)
    double b0, b100;               // asymmetry (bias) at 0 % / 100 %  -> even harmonics
    double preHp;                  // Hz, 0 = off
    double prePkF, prePkDb, prePkQ;
    double postPkF, postPkDb, postPkQ;
    double postLp;                 // Hz, 0 = off (cabinet / tape roll-off)
    int    postLpOrder;            // number of 2nd-order sections (0..3)
    double s0, s100;               // style specific (fold gain, crush rate, sub mix, 2nd-stage gain ...)
    double knee2;                  // 2nd (power-amp) stage hardness, kTwoStage only
    double c0, c50, c100;          // make-up gain in dB at 0 / 50 / 100 % drive
};

// ---------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    { b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0; }

    void lowpass (double fs, double f, double q = 0.70710678)
    {
        f = std::min (f, fs * 0.45);
        const double w = 2 * nep::kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double q = 0.70710678)
    {
        const double w = 2 * nep::kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double f, double db, double q)
    {
        f = std::min (f, fs * 0.45);
        const double A = std::pow (10.0, db / 40), w = 2 * nep::kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double f, double db, double q = 0.70710678)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * nep::kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / (2 * q), r = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + r), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - r),
             (A + 1) + (A - 1) * c + r, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - r);
    }
    void highShelf (double fs, double f, double db, double q = 0.70710678)
    {
        f = std::min (f, fs * 0.45);
        const double A = std::pow (10.0, db / 40), w = 2 * nep::kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / (2 * q), r = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + r), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - r),
             (A + 1) - (A - 1) * c + r, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - r);
    }
};

inline double softClip (double x, double p)          // x / (1 + |x|^p)^(1/p)
{
    const double ax = std::fabs (x);
    if (ax < 1e-9) return x;
    return x / std::pow (1.0 + std::pow (ax, p), 1.0 / p);
}
inline double fold (double x)                           // triangle wave-folder, unity slope at 0
{
    double t = std::fmod (x + 1.0, 4.0);
    if (t < 0) t += 4.0;
    return 1.0 - std::fabs (t - 2.0);
}

// Lookup table for softClip(x, p) over the whole real line using u = x / (1 + |x|)
struct ShaperTable
{
    static constexpr int N = 16384;
    double p = -1;
    float t[N + 2];
    void build (double knee)
    {
        if (knee == p) return;
        p = knee;
        for (int i = 0; i <= N; ++i)
        {
            const double u = -1.0 + 2.0 * i / N;
            const double au = std::fabs (u);
            t[i] = au >= 1.0 ? (float) (u > 0 ? 1.0 : -1.0) : (float) softClip (u / (1.0 - au), knee);
        }
        t[N + 1] = t[N];
    }
    inline double operator() (double x) const
    {
        const double u = x / (1.0 + std::fabs (x));
        const double pos = (u + 1.0) * (0.5 * N);
        const int i = std::min (N - 1, std::max (0, (int) pos));
        const double fr = pos - i;
        return t[i] + (t[i + 1] - t[i]) * fr;
    }
};

inline double lerp (double a, double b, double t) { return a + (b - a) * t; }
inline double quad3 (double v0, double v50, double v100, double d)  // through (0,v0) (.5,v50) (1,v100)
{
    const double a = 2 * v100 - 4 * v50 + 2 * v0, b = 4 * v50 - v100 - 3 * v0;
    return v0 + b * d + a * d * d;
}

// ---------------------------------------------------------------------------
// One channel of one style, running at the oversampled rate.
class StyleChannel
{
public:
    void prepare (double sampleRate) { fs = sampleRate; dc.highpass (fs, 5.0, 0.5); dcPre.highpass (fs, 10.0, 0.5); setStyle (def, true); reset(); }
    void reset()
    {
        preHp.reset(); prePk.reset(); dcPre.reset(); postPk.reset(); dc.reset();
        for (auto& b : lp) b.reset();
        for (auto& b : crushLp) b.reset();
        hold = 0; holdPhase = 0; ff = 1; lastX = 0; smIdx = 0; smEnv = 0;
        for (auto& v : smBuf) v = 0;
        rng = 0x1234567u; for (auto& s : smPos) s = 0; for (auto& s : smVel) s = 0;
    }
    void setStyle (const StyleDef& d, bool force = false)
    {
        const bool changed = force || std::memcmp (&d, &def, sizeof (StyleDef)) != 0;
        def = d;
        if (! changed) return;
        if (def.preHp > 0) preHp.highpass (fs, def.preHp); else preHp.setIdentity();
        if (def.prePkF > 0 && def.prePkDb != 0) prePk.peak (fs, def.prePkF, def.prePkDb, std::max (0.2, def.prePkQ)); else prePk.setIdentity();
        if (def.postPkF > 0 && def.postPkDb != 0) postPk.peak (fs, def.postPkF, def.postPkDb, std::max (0.2, def.postPkQ)); else postPk.setIdentity();
        static const double qs[3][3] = { { 0.7071, 0, 0 }, { 0.5412, 1.3066, 0 }, { 0.5176, 0.7071, 1.9319 } };
        for (int i = 0; i < 3; ++i)
        {
            if (def.postLp > 0 && i < def.postLpOrder) lp[i].lowpass (fs, def.postLp, qs[def.postLpOrder - 1][i]);
            else lp[i].setIdentity();
        }
        shaper.build (std::max (0.5, def.knee));
        shaper2.build (std::max (0.5, def.knee2));
        lastCrushF = -1; crushF = -1; lastDrive = -1;
    }

    // drive 0..1
    inline double process (double x, double drive)
    {
        drive = std::clamp (drive, 0.0, 1.0);
        if (drive != lastDrive || lastCrushF < 0)
        {
            lastDrive = drive;
            const double gDb = def.g0 + (def.g100 - def.g0) * std::pow (drive, def.gamma);
            g = std::pow (10.0, gDb / 20.0);
            bias = lerp (def.b0, def.b100, drive);
            sp = def.kind == kDestroy ? lerp (def.s0, def.s100, std::pow (drive, def.gamma)) : lerp (def.s0, def.s100, drive);
            g2 = std::pow (10.0, sp / 20.0);
            mk = std::pow (10.0, quad3 (def.c0, def.c50, def.c100, drive) / 20.0);
            biasOut = softClip (bias, def.knee);
            if (lastCrushF < 0) lastCrushF = 0;
        }

        double v = prePk.process (preHp.process (x));
        double y;
        switch (def.kind)
        {
            case kFoldback:
                y = shaper (fold (v * g));
                break;
            case kRectify:
            {
                // full-wave rectify, then remove DC and soft-clip
                const double r = dcPre.process (std::fabs (v));
                y = shaper (r * g + bias) - biasOut;
                break;
            }
            case kDestroy:
            {
                // sample-rate reduction (with anti-alias low-pass), bit-crush, clip
                const double fr = std::pow (10.0, sp);           // target rate in Hz (log10 fitted)
                if (std::fabs (fr - crushF) > 1.0)
                {
                    crushF = fr;
                    crushLp[0].lowpass (fs, fr * 0.5, 0.5412); crushLp[1].lowpass (fs, fr * 0.5, 1.3066);
                    crushLp[2].lowpass (fs, fr * 0.5, 0.5412); crushLp[3].lowpass (fs, fr * 0.5, 1.3066);
                }
                double f = crushLp[1].process (crushLp[0].process (v));
                holdPhase += fr / fs;
                if (holdPhase >= 1.0) { holdPhase -= std::floor (holdPhase); hold = f; }
                const double bits = 16.0 - 13.0 * drive;
                const double q = std::pow (2.0, bits - 1);
                const double c = std::round (hold * q) / q;
                y = shaper (crushLp[3].process (crushLp[2].process (c)) * g);
                break;
            }
            case kBreakdown:
            {
                // octave-down divider (flip-flop on rising zero crossings) blended in, then heavy distortion
                if (lastX <= 0 && v > 0) ff = -ff;
                lastX = v;
                const double sub = std::fabs (v) * ff;
                const double m = std::clamp (sp, 0.0, 1.0);
                y = shaper ((v * (1 - m) + sub * m) * g + bias) - biasOut;
                break;
            }
            case kSmudge:
            {
                // a cloud of slowly wandering short delay taps: smears/stretches transients and pitch
                smBuf[smIdx] = v;
                const int maxD = (int) std::min<double> (kSmLen - 4, fs * 0.045 * sp);
                double acc = v * (1.0 - std::clamp (sp, 0.0, 1.0));
                for (int t = 0; t < kTaps; ++t)
                {
                    rng = rng * 1664525u + 1013904223u;
                    const double n = ((rng >> 9) * (1.0 / 8388608.0)) - 1.0;
                    smVel[t] = smVel[t] * 0.9995 + n * 0.0005;
                    smPos[t] = std::clamp (smPos[t] + smVel[t] * 0.02, 0.0, 1.0);
                    const double dly = 1.0 + smPos[t] * maxD;
                    const int i0 = (int) dly; const double fr = dly - i0;
                    const double a = smBuf[(smIdx - i0 + kSmLen) % kSmLen], b = smBuf[(smIdx - i0 - 1 + kSmLen) % kSmLen];
                    acc += (a + (b - a) * fr) * std::clamp (sp, 0.0, 1.0) / kTaps;
                }
                smIdx = (smIdx + 1) % kSmLen;
                y = shaper (acc * g);
                break;
            }
            case kTwoStage:
                // preamp (asymmetric) -> inter-stage EQ -> power amp (symmetric)
                y = shaper (v * g + bias) - biasOut;
                y = shaper2 (postPk.process (y) * g2);
                break;
            default:
                y = shaper (v * g + bias) - biasOut;
                break;
        }
        if (def.kind != kTwoStage) y = postPk.process (y);
        for (auto& b : lp) y = b.process (y);
        y = dc.process (y);
        return y * mk;
    }

private:
    StyleDef def { 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0.7, 0, 0, 0.7, 0, 0, 0, 0, 2, 0, 0, 0 };
    double fs = 48000;
    Biquad preHp, prePk, postPk, lp[3], dc, crushLp[4];
    Biquad dcPre;
    ShaperTable shaper, shaper2;
    double lastCrushF = -1, crushF = -1, lastDrive = -1, g = 1, g2 = 1, bias = 0, sp = 0, mk = 1, biasOut = 0, hold = 0, holdPhase = 0, lastX = 0;
    int ff = 1;
    static constexpr int kSmLen = 65536, kTaps = 6;
    double smBuf[kSmLen] {};
    int smIdx = 0; double smEnv = 0;
    double smPos[kTaps] {}, smVel[kTaps] {};
    uint32_t rng = 0x1234567u;
};
} // namespace nep
