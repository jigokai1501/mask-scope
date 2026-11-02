#include "../Source/Dsp/Meters.h"
#include <cstdio>
using namespace mask;
static int fails = 0;
static void check (const char* name, double got, double want, double tol)
{
    const bool ok = std::abs (got - want) <= tol;
    if (! ok) ++fails;
    std::printf ("%-52s got %9.3f  want %9.3f  tol %.2f  %s\n", name, got, want, tol, ok ? "OK" : "FAIL");
}
struct Gen { double ph = 0; };
static void run (MeterEngine& m, double fs, double sec, double f, double ampL, double ampR, Gen& g, double phaseOffR = 0.0)
{
    const int n = (int) (sec * fs);
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        L[(size_t) i] = (float) (ampL * std::sin (g.ph));
        R[(size_t) i] = (float) (ampR * std::sin (g.ph + phaseOffR));
        g.ph += 2 * kPi * f / fs;
    }
    for (int i = 0; i < n; i += 480)
        m.process (L.data() + i, R.data() + i, std::min (480, n - i), true, true);
}
static MeterBlock get (MeterEngine& m) { MeterBlock b; m.out.read (b); return b; }
int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("--- fs = %.0f\n", fs);
        {
            MeterEngine m; m.prepare (fs, false); Gen g;
            run (m, fs, 20, 997, std::pow (10, -23 / 20.0), std::pow (10, -23 / 20.0), g);
            auto b = get (m);
            check ("997 Hz stereo -23 dBFS: M", b.lufsM, -23.0, 0.1);
            check ("997 Hz stereo -23 dBFS: S", b.lufsS, -23.0, 0.1);
            check ("997 Hz stereo -23 dBFS: I", b.lufsI, -23.0, 0.1);
            check ("  sample peak", b.peakL, -23.0, 0.1);
            check ("  true peak", b.tpL, -23.0, 0.3);
            check ("  correlation", b.corr, 1.0, 0.01);
            check ("  mono fold", b.monoFoldDb, 0.0, 0.05);
        }
        {
            MeterEngine m; m.prepare (fs, false); Gen g;
            run (m, fs, 20, 997, 1.0, 0.0, g);
            auto b = get (m);
            check ("997 Hz 0 dBFS left only: I (-3.01)", b.lufsI, -3.01, 0.1);
        }
        {
            MeterEngine m; m.prepare (fs, false); Gen g;
            run (m, fs, 20, 997, std::pow (10, -20 / 20.0), std::pow (10, -20 / 20.0), g);
            run (m, fs, 20, 997, std::pow (10, -40 / 20.0), std::pow (10, -40 / 20.0), g);
            auto b = get (m);
            check ("gating: 20 s @-20 + 20 s @-40  => I = -20", b.lufsI, -20.0, 0.1);
        }
        {
            MeterEngine m; m.prepare (fs, false); Gen g;
            run (m, fs, 20, 997, std::pow (10, -20 / 20.0), std::pow (10, -20 / 20.0), g);
            run (m, fs, 20, 997, std::pow (10, -30 / 20.0), std::pow (10, -30 / 20.0), g);
            auto b = get (m);
            check ("LRA: 20 s @-20 then 20 s @-30 => 10 LU", b.lra, 10.0, 1.0);
            check ("  integrated of the two (gate keeps both)", b.lufsI, -22.6, 0.4);
        }
        {
            MeterEngine m; m.prepare (fs, false); Gen g;
            run (m, fs, 5, 997, 0.5, 0.5, g, kPi); // L et R en opposition de phase
            auto b = get (m);
            check ("anti-phase: correlation = -1", b.corr, -1.0, 0.02);
            check ("anti-phase: mono fold very low (< -40 dB)", b.monoFoldDb < -40.0 ? 1 : 0, 1, 0);
        }
        {   // true peak : sinus à fs/4 déphasé de 45° : échantillons ±0,707 mais crête réelle 1,0
            MeterEngine m; m.prepare (fs, false);
            const int n = (int) (fs * 2);
            std::vector<float> L ((size_t) n);
            for (int i = 0; i < n; ++i) L[(size_t) i] = (float) std::sin (2 * kPi * 0.25 * i + kPi / 4);
            for (int i = 0; i < n; i += 480) m.process (L.data() + i, L.data() + i, std::min (480, n - i), true, true);
            auto b = get (m);
            check ("fs/4 sine @45°: sample peak = -3.01", b.peakL, -3.01, 0.05);
            if (fs < 96000) check ("fs/4 sine @45°: true peak = 0 dBTP", b.tpL, 0.0, 0.25);
        }
    }
    std::printf ("\n%s (%d failure%s)\n", fails ? "ECHEC" : "TOUT OK", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
