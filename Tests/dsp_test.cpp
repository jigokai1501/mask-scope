// Tests du moteur d'analyse (sans JUCE) : calibrage du spectre, spectrogramme, bandes, ADSR, champ stéréo
#include "../Source/Dsp/Engine.h"
#include <cstdio>
#include <random>
using namespace mask;
static int fails = 0;
static void check (const char* name, double got, double want, double tol)
{
    const bool ok = std::abs (got - want) <= tol;
    if (! ok) ++fails;
    std::printf ("%-58s got %10.3f  want %10.3f  tol %.2f  %s\n", name, got, want, tol, ok ? "OK" : "FAIL");
}
static void checkTrue (const char* name, bool v) { if (! v) ++fails; std::printf ("%-58s %s\n", name, v ? "OK" : "FAIL"); }

static int paceMs() { static const int v = std::getenv ("DSP_PACE_MS") ? std::atoi (std::getenv ("DSP_PACE_MS")) : 40; return v; }
static void feed (Engine& e, const std::vector<float>& L, const std::vector<float>& R, double fs, bool pace = true)
{
    HostInfo h; h.haveHost = true; h.playing = true; h.bpm = 120;
    const int B = 480;
    for (size_t i = 0; i < L.size(); i += B)
    {
        const int n = (int) std::min<size_t> (B, L.size() - i);
        e.process (L.data() + i, R.data() + i, n, h);
        if (pace && (i / B) % 100 == 99) std::this_thread::sleep_for (std::chrono::milliseconds (paceMs())); // ≈ 1 s audio par pas
    }
    (void) fs;
    std::this_thread::sleep_for (std::chrono::milliseconds (400 + 6 * paceMs()));
}

int main()
{
    const double fs = 48000.0;
    // ---- 1. calibrage : sinus 1 kHz à -6 dBFS (L = R)
    {
        Engine e; e.start (fs, false);
        const int n = (int) (fs * 3);
        std::vector<float> L ((size_t) n), R ((size_t) n);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.5 * std::sin (2 * kPi * 1000.0 * i / fs));
        feed (e, L, R, fs);
        SpectrumBlock sb; e.analyzer.spectrumOut.read (sb);
        // trouver le point max du spectre ST
        int im = 0; for (int i = 0; i < kSpecPts; ++i) if (sb.live[ModeST][i] > sb.live[ModeST][im]) im = i;
        const double f = 20.0 * std::pow (std::min (20000.0, fs * 0.49) / 20.0, (double) im / (kSpecPts - 1));
        check ("sinus 1 kHz -6 dBFS : fréquence du pic (Hz)", f, 1000.0, 60.0);
        check ("  niveau du pic ST (dB, scalloping ≤ 1,5 dB)", sb.live[ModeST][im], -6.0, 2.0);
        check ("  Mid = ST (signal mono)", sb.live[ModeMid][im], sb.live[ModeST][im], 0.2);
        checkTrue ("  Side très bas (< -100 dB)", sb.live[ModeSide][im] < -100.0f);
        SpectrogramStore& sp = e.analyzer.spectrogram;
        check ("spectrogramme : colonnes/s (≈200)", (double) sp.written.load() / 3.0, 200.0, 12.0);
        const uint64_t c = sp.written.load() - 5;
        const uint8_t* col = sp.column (0, c);
        int rm = 0; for (int r = 0; r < kSpecRows; ++r) if (col[r] > col[rm]) rm = r;
        const double fr = 20.0 * std::pow (std::min (20000.0, fs * 0.49) / 20.0, (double) rm / (kSpecRows - 1));
        check ("  ligne du pic -> fréquence (Hz)", fr, 1000.0, 40.0);
        check ("  niveau du pic spectrogramme (dB)", SpectrogramStore::dequant (col[rm]), -6.0, 2.5);
        AnalysisBlock ab; e.analyzer.analysisOut.read (ab);
        (void) ab;
        e.stop();
    }
    // ---- 2. champ stéréo : sinus 2 kHz en opposition de phase (Side pur)
    {
        Engine e; e.start (fs, false);
        const int n = (int) (fs * 3);
        std::vector<float> L ((size_t) n), R ((size_t) n);
        for (int i = 0; i < n; ++i) { const float v = (float) (0.4 * std::sin (2 * kPi * 2000.0 * i / fs)); L[(size_t) i] = v; R[(size_t) i] = -v; }
        feed (e, L, R, fs);
        SpectrumBlock sb; e.analyzer.spectrumOut.read (sb);
        int im = 0; for (int i = 0; i < kSpecPts; ++i) if (sb.live[ModeSide][i] > sb.live[ModeSide][im]) im = i;
        checkTrue ("opposition de phase : Side >> Mid", sb.live[ModeSide][im] - sb.live[ModeMid][im] > 60.0f);
        AnalysisBlock ab; e.analyzer.analysisOut.read (ab);
        check ("  corrélation de la bande 1-2,5k = -1", ab.bandCorr[4], -1.0, 0.05);
        MeterBlock mb; e.meters.out.read (mb);
        check ("  corrélation globale", mb.corr, -1.0, 0.02);
        e.stop();
    }
    // ---- 3. fondamentale grave : sinus 52 Hz
    {
        Engine e; e.start (fs, false);
        const int n = (int) (fs * 4);
        std::vector<float> L ((size_t) n), R ((size_t) n);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.5 * std::sin (2 * kPi * 52.0 * i / fs));
        feed (e, L, R, fs);
        AnalysisBlock ab; e.analyzer.analysisOut.read (ab);
        check ("sinus 52 Hz : pic grave détecté (Hz)", ab.subHz, 52.0, 1.5);
        checkTrue ("  prominence > 20 dB", ab.subProm > 20.0f);
        e.stop();
    }
    // ---- 4. kicks (A court, queue exponentielle) toutes les 500 ms
    {
        Engine e; e.start (fs, false);
        const int n = (int) (fs * 6.2);
        std::vector<float> L ((size_t) n, 0.0f), R;
        const double tauMs = 400.0 / 6.9078;       // -60 dB à 400 ms  (ln(1000) = 6.9078)
        for (int k = 0; k < 12; ++k)
        {
            const int s0 = (int) (fs * (0.2 + 0.5 * k));
            double ph = 0;
            for (int i = 0; i < (int) (fs * 0.5) && s0 + i < n; ++i)
            {
                const double t = i / fs;                               // s
                const double att = std::min (1.0, t / 0.003);           // attaque 3 ms
                const double env = att * std::exp (-t * 1000.0 / tauMs);
                const double f = 50.0 + 90.0 * std::exp (-t / 0.03);   // chute de hauteur
                ph += 2 * kPi * f / fs;
                L[(size_t) (s0 + i)] = (float) (0.8 * env * std::sin (ph));
            }
        }
        R = L;
        feed (e, L, R, fs);
        AnalysisBlock ab; e.analyzer.analysisOut.read (ab);
        std::printf ("  kicks : %u coups détectés\n", ab.hitCount);
        checkTrue ("kick : ≥ 8 coups détectés sur 12", ab.hitCount >= 8);
        const HitSummary& h = ab.lastHit;
        std::printf ("  dernier coup : A=%.0f D=%.0f S=%.0f R=%.0f ms  t20=%.0f t40=%.0f t60=%.0f  pente D=%.0f dB/s tronqué=%d\n",
                     h.aMs, h.dMs, h.sMs, h.rMs, h.t20, h.t40, h.t60, h.slopeD, h.truncated);
        check ("  pic ≈ -2 dBFS", h.peakDb, 20 * std::log10 (0.8), 1.5);
        check ("  attaque (ms) ≤ ~6", h.aMs, 3.0, 4.0);
        check ("  T20 (≈ 133 ms + maintien ≤ 11 ms)", h.t20, 400.0 / 3.0, 25.0);
        check ("  T40 (≈ 267 ms)", h.t40, 800.0 / 3.0, 25.0);
        checkTrue ("  coupé par le coup suivant (500 ms < T60)", h.truncated == 1 || h.t60 > 0);
        e.stop();
    }
    // ---- 5. nappe ADSR : A 80 ms, D 100 ms → -6 dB, S 600 ms, R 300 ms
    {
        Engine e; e.start (fs, false);
        const int n = (int) (fs * 3.0);
        std::vector<float> L ((size_t) n, 0.0f), R;
        const int s0 = (int) (fs * 0.5);
        const double A = 0.08, D = 0.10, S = 0.60, Rl = 0.30;
        double ph = 0;
        for (int i = 0; i < (int) (fs * (A + D + S + Rl + 0.5)); ++i)
        {
            const double t = i / fs; double env;
            if (t < A) env = t / A;
            else if (t < A + D) env = 1.0 - 0.5 * (t - A) / D;
            else if (t < A + D + S) env = 0.5;
            else env = 0.5 * std::pow (10.0, -60.0 * (t - A - D - S) / Rl / 20.0);
            ph += 2 * kPi * 220.0 / fs;
            if (s0 + i < n) L[(size_t) (s0 + i)] = (float) (0.7 * env * std::sin (ph));
        }
        R = L;
        feed (e, L, R, fs);
        AnalysisBlock ab; e.analyzer.analysisOut.read (ab);
        const HitSummary& h = ab.lastHit;
        std::printf ("  nappe : A=%.0f D=%.0f S=%.0f R=%.0f ms  sustain=%.1f dB  pentes D=%.0f S=%.0f R=%.0f dB/s\n",
                     h.aMs, h.dMs, h.sMs, h.rMs, h.sustainDb, h.slopeD, h.slopeS, h.slopeR);
        checkTrue ("nappe : un coup détecté", ab.hitCount >= 1);
        check ("  attaque (10→100 %) ≈ 72 ms", h.aMs, 72.0, 15.0);
        checkTrue ("  un plateau S est trouvé (pente ≈ 0)", h.hasS && std::abs (h.slopeS) < 15.0f);
        check ("  durée de S ≈ 600 ms", h.sMs, 600.0, 120.0);
        check ("  niveau de S ≈ -6 dB", h.sustainDb, -6.0, 2.5);
        checkTrue ("  une release R est trouvée, pente < -60 dB/s", h.hasR && h.slopeR < -60.0f);
        e.stop();
    }
    // ---- 6. codec : bruit écrêté → dépassement mesuré
    {
        Engine e; e.start (fs, false);
        const int n = (int) (fs * 6.0);
        std::vector<float> L ((size_t) n), R ((size_t) n);
        std::mt19937 rng (1); std::normal_distribution<float> nd (0.0f, 1.0f);
        float lp1 = 0, lp2 = 0;
        for (int i = 0; i < n; ++i) { lp1 += 0.1f * (nd (rng) - lp1); lp2 += 0.4f * (nd (rng) - lp2); const float v = std::max (-0.89f, std::min (0.89f, 3.0f * (lp1 + 0.3f * lp2))); L[(size_t) i] = v; R[(size_t) i] = v * 0.9f; }
        feed (e, L, R, fs);
        AnalysisBlock ab; e.analyzer.analysisOut.read (ab);
        for (int c = 0; c < kCodecs; ++c)
            std::printf ("  codec %d : valid=%d dépassement max %.2f dB moyen %.2f dB  HF perdu %.1f dB (%u s)\n", c, ab.codec[c].valid, ab.codec[c].overshootMaxDb, ab.codec[c].overshootAvgDb, ab.codec[c].hfLossDb, ab.codec[c].seconds);
        checkTrue ("codec : mesure valide", ab.codec[0].valid != 0);
        checkTrue ("  96 kbps dépasse plus que 320 kbps", ab.codec[0].overshootMaxDb >= ab.codec[2].overshootMaxDb - 0.05f);
        e.stop();
    }
    std::printf ("\n%s (%d échec%s)\n", fails ? "ECHEC" : "TOUT OK", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
