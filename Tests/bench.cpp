// Mesure de coût : thread audio (process) et thread d'analyse (charge pendant 10 s en temps réel)
#include "../Source/Dsp/Engine.h"
#include <cstdio>
#include <random>
using namespace mask;
int main()
{
    const double fs = 48000.0;
    const int n = (int) (fs * 10);
    std::vector<float> L ((size_t) n), R ((size_t) n);
    std::mt19937 rng (5); std::normal_distribution<float> nd (0.0f, 0.2f);
    for (int i = 0; i < n; ++i) { L[(size_t) i] = nd (rng); R[(size_t) i] = nd (rng); }
    Engine e; e.start (fs, false);
    HostInfo h; h.haveHost = true; h.playing = true; h.bpm = 120;
    // 1) coût du thread audio : 10 s de signal, blocs de 512, mesurés individuellement
    double worst = 0, total = 0; int cnt = 0;
    for (int i = 0; i + 512 <= n; i += 512)
    {
        const auto t0 = std::chrono::steady_clock::now();
        e.process (L.data() + i, R.data() + i, 512, h);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        total += ms; worst = std::max (worst, ms); ++cnt;
        if (cnt % 100 == 0) std::this_thread::sleep_for (std::chrono::milliseconds (30));
    }
    const double budget = 512.0 / fs * 1000.0;
    std::printf ("thread audio : moyenne %.3f ms / bloc de 512 (budget %.2f ms) = %.2f %% CPU ; pire bloc %.3f ms\n", total / cnt, budget, 100.0 * total / cnt / budget, worst);
    // 2) charge du thread d'analyse en temps réel : 10 s de signal alimentées à vitesse réelle
    e.reset();
    for (int i = 0; i + 512 <= n; i += 512)
    {
        e.process (L.data() + i, R.data() + i, 512, h);
        std::this_thread::sleep_for (std::chrono::microseconds ((long) (512.0 / fs * 1e6)));
    }
    AnalysisBlock a; e.analyzer.analysisOut.read (a);
    std::printf ("thread d'analyse (FFT 4096 spectrogramme + FFT 8192 spectre + codec x3 + enveloppe) : %.1f %% d'un cœur ; blocs perdus : %u\n", a.loadPct, a.dropped);
    e.stop();
    return 0;
}
