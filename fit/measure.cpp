#include "../Source/StyleEngine.h"
#include <vector>
#include <complex>
using namespace nep;
static StyleDef fromArr (const double* p)
{
    StyleDef d;
    d.kind=(int)p[0]; d.g0=p[1]; d.g100=p[2]; d.gamma=p[3]; d.knee=p[4]; d.b0=p[5]; d.b100=p[6];
    d.preHp=p[7]; d.prePkF=p[8]; d.prePkDb=p[9]; d.prePkQ=p[10]; d.postPkF=p[11]; d.postPkDb=p[12]; d.postPkQ=p[13];
    d.postLp=p[14]; d.postLpOrder=(int)p[15]; d.s0=p[16]; d.s100=p[17]; d.knee2=p[18]; d.c0=p[19]; d.c50=p[20]; d.c100=p[21];
    return d;
}
static StyleChannel ch;
// out[0] = fundamental gain dB, out[1..14] = H2..H15 dB re fundamental (-240 if absent/above 24 kHz), out[15] = THD %
static double warmSec = 0.25;
extern "C" void setWarm (double s) { warmSec = s; }
extern "C" void measure (const double* p, double drive, double freq, double levelDb, double fs, double* out)
{
    ch.setStyle (fromArr (p), true); ch.prepare (fs);
    const double A = std::pow (10.0, levelDb / 20.0);
    const int warm = (int) (warmSec * fs), N = (int) (0.05 * fs);
    std::vector<double> y (N);
    for (int n = 0; n < warm + N; ++n)
    {
        const double v = ch.process (A * std::sin (2 * M_PI * freq * n / fs), drive);
        if (n >= warm) y[n - warm] = v;
    }
    auto amp = [&] (double f) { double re = 0, im = 0; for (int n = 0; n < N; ++n) { double ph = 2 * M_PI * f * (n + warm) / fs; re += y[n] * std::cos (ph); im += y[n] * std::sin (ph); } return 2 * std::sqrt (re * re + im * im) / N; };
    const double a1 = amp (freq);
    out[0] = 20 * std::log10 (std::max (a1, 1e-12) / A);
    double s = 0;
    for (int h = 2; h <= 15; ++h)
    {
        if (h * freq >= 24000) { out[h - 1] = -240; continue; }
        const double ah = amp (h * freq); s += ah * ah;
        out[h - 1] = std::max (-240.0, 20 * std::log10 (std::max (ah, 1e-15) / std::max (a1, 1e-15)));
    }
    out[15] = 100 * std::sqrt (s) / std::max (a1, 1e-15);
}
