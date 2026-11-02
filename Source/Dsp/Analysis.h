// mask::scope — thread d'analyse : spectrogramme, spectre, bandes, enveloppe/ADSR, simulation codec
#pragma once
#include <chrono>
#include <thread>
#include "Codec.h"
#include "Envelope.h"
#include "Fft.h"
#include "Meters.h"

namespace mask
{
constexpr int kSpecRows = 512;       // lignes du spectrogramme (20 Hz → 20 kHz, log)
constexpr int kSpecCols = 6144;      // 30,7 s à 200 colonnes/s
constexpr int kColsPerSec = 200;
constexpr int kSpecPts = 1024;       // points du spectre
constexpr int kModes = 5;            // ST, MID, SIDE, L, R
constexpr int kBands = 8;
constexpr int kCodecs = 3;
constexpr double kFMin = 20.0;

enum Mode { ModeST = 0, ModeMid, ModeSide, ModeL, ModeR };

inline const double* bandEdges()
{
    static const double e[kBands + 1] = { 20, 60, 150, 400, 1000, 2500, 5000, 9000, 20000 };
    return e;
}
inline const char* bandName (int b)
{
    static const char* n[kBands] = { "SUB", "BASS", "LOW MID", "MID", "HI MID", "PRESENCE", "BRILLIANCE", "AIR" };
    return n[b];
}

struct Settings
{
    std::atomic<int> specFft { 1 };        // 0:2048 1:4096 2:8192
    std::atomic<int> spectrumFft { 2 };    // 0:2048 … 4:32768
    std::atomic<int> smooth { 3 };         // 0:aucun 1:1/24 2:1/12 3:1/6 4:1/3 (fraction d'octave)
    std::atomic<int> speed { 1 };          // 0:rapide … 3:très lent
    std::atomic<bool> hold { false };      // gel du spectrogramme et de l'enveloppe
    std::atomic<float> hitThresh { -48.0f };
    std::atomic<bool> autoReset { true };
    std::atomic<int> resetSpectrum { 0 };  // incrémenté pour effacer moyenne / crête
    static int specFftSize (int i) { return 2048 << clampi (i, 0, 2); }
    static int spectrumFftSize (int i) { return 2048 << clampi (i, 0, 4); }
    static double smoothOct (int i) { static const double v[] = { 1.0 / 96.0, 1.0 / 24.0, 1.0 / 12.0, 1.0 / 6.0, 1.0 / 3.0 }; return v[clampi (i, 0, 4)]; }
    static double speedMs (int i) { static const double v[] = { 40.0, 120.0, 350.0, 1000.0 }; return v[clampi (i, 0, 3)]; }
};

struct SpectrogramStore
{
    std::vector<uint8_t> plane[3];                 // ST, MID, SIDE : (dB + 120) * 2
    std::atomic<uint64_t> written { 0 };
    SpectrogramStore() { for (auto& p : plane) p.assign ((size_t) kSpecRows * kSpecCols, 0); }
    void clear() { written.store (0); for (auto& p : plane) std::fill (p.begin(), p.end(), 0); }
    static uint8_t quant (float db) noexcept { const float v = (db + 120.0f) * 2.0f; return (uint8_t) (v < 0 ? 0 : (v > 254 ? 254 : (int) v)); }
    static float dequant (uint8_t q) noexcept { return (float) q * 0.5f - 120.0f; }
    const uint8_t* column (int pl, uint64_t c) const noexcept { return plane[pl].data() + (size_t) (c % kSpecCols) * kSpecRows; }
    uint8_t* columnW (int pl, uint64_t c) noexcept { return plane[pl].data() + (size_t) (c % kSpecCols) * kSpecRows; }
};

struct SpectrumBlock
{
    float live[kModes][kSpecPts];
    float avg[kModes][kSpecPts];
    float peak[kModes][kSpecPts];
    float fmax = 20000.0f;
    int fftN = 8192;
    uint32_t frames = 0;
};

struct AnalysisBlock
{
    float bandLiveDb[kBands] {}, bandSessDb[kBands] {};
    float bandShareLive[kBands] {}, bandShareSess[kBands] {};
    float bandCorr[kBands] {}, bandSideDb[kBands] {};   // Side relatif au Mid, en dB
    float subHz = 0, subDb = -140, subProm = 0;
    float sessFrames = 0;       // secondes de spectre cumulées
    float loadPct = 0;
    uint32_t dropped = 0;
    uint32_t hitCount = 0;
    uint8_t haveHit = 0;
    HitSummary lastHit;
    CodecResult codec[kCodecs];
};

class Analyzer
{
public:
    Analyzer (const StereoRing& r, Settings& s) : ring (r), set (s) { }
    ~Analyzer() { stop(); }

    SpectrogramStore spectrogram;
    EnvStore envelope;
    HitTracker hits;
    SeqLock<SpectrumBlock> spectrumOut;
    SeqLock<AnalysisBlock> analysisOut;
    std::atomic<bool> active { true };

    void start (double sampleRate)
    {
        stop();
        fs = sampleRate;
        stopFlag = false;
        spectrogram.clear(); envelope.clear(); hits.reset();
        specFftCur = -1; spFftCur = -1; smoothCur = -1;
        resetAccum();
        int nc = 0; const auto* prof = codecProfiles (nc);
        for (int i = 0; i < kCodecs; ++i) codec[i].prepare (fs, prof[i]);
        binSamples = std::max (1, (int) std::lround (fs / 1000.0));
        pos = ring.written();
        th = std::thread ([this] { loop(); });
    }
    void stop()
    {
        stopFlag = true;
        if (th.joinable()) th.join();
    }
    void resetSession()
    {
        sessionResetReq.store (true);
    }

private:
    void resetAccum()
    {
        for (auto& v : sessSum) v = 0.0;
        sessFrames = 0;
        for (int m = 0; m < kModes; ++m) { for (auto& x : avgSum[m]) x = 0.0; }
        avgCount = 0;
        std::fill (&peakDb[0][0], &peakDb[0][0] + kModes * kSpecPts, -140.0f);
        std::fill (&holdLeft[0][0], &holdLeft[0][0] + kModes * kSpecPts, 0);
        liveInit = false;
        for (int i = 0; i < kCodecs; ++i) codec[i].reset();
    }

    // ---------- configuration dépendante de la fréquence d'échantillonnage / réglages ----------
    void configureSpec()
    {
        const int idx = set.specFft.load();
        if (idx == specFftCur) return;
        specFftCur = idx;
        specN = Settings::specFftSize (idx);
        sfft.init (specN);
        makeHann (swin, specN);
        double sw = 0; for (float v : swin) sw += v;
        sscale = (float) (2.0 / sw);
        sspec.init (specN);
        sre.assign ((size_t) specN, 0.0f); sim.assign ((size_t) specN, 0.0f);
        const double fmax = std::min (20000.0, fs * 0.49);
        smap.init (specN, fs, kSpecRows, kFMin, fmax, 1.0 / 24.0, windowEnbw (swin));
        spFmax = (float) fmax;
        hopCol = std::max (1, (int) std::lround (fs / kColsPerSec));
        nextCol = pos + (uint64_t) hopCol;
        tmpP.assign ((size_t) kSpecRows, 0.0f);
    }
    void configureSpectrum()
    {
        const int fi = set.spectrumFft.load(), sm = set.smooth.load();
        if (fi == spFftCur && sm == smoothCur) return;
        const bool fftChanged = fi != spFftCur;
        spFftCur = fi; smoothCur = sm;
        pN = Settings::spectrumFftSize (fi);
        if (fftChanged)
        {
            pfft.init (pN);
            makeBlackmanHarris (pwin, pN);
            double sw = 0; for (float v : pwin) sw += v;
            pscale = (float) (2.0 / sw);
            pspec.init (pN);
            pre.assign ((size_t) pN, 0.0f); pim.assign ((size_t) pN, 0.0f);
            pstSm.assign ((size_t) pN / 2 + 1, 0.0f);
            nextSpec = pos + 1024;
            // bornes des bandes en bins
            const double binHz = fs / pN;
            for (int b = 0; b < kBands; ++b)
            {
                bandK0[b] = std::max (1, (int) std::ceil (bandEdges()[b] / binHz));
                bandK1[b] = std::max (bandK0[b] + 1, (int) std::ceil (bandEdges()[b + 1] / binHz));
                bandK1[b] = std::min (bandK1[b], pN / 2);
            }
            liveInit = false;
        }
        pmap.init (pN, fs, kSpecPts, kFMin, std::min (20000.0, fs * 0.49), Settings::smoothOct (sm), windowEnbw (pwin));
        for (auto& v : pwBuf) v.assign (kSpecPts, 0.0f);
    }

    // ---------- boucle ----------
    void loop()
    {
        using clk = std::chrono::steady_clock;
        auto lastPub = clk::now();
        double busy = 0.0, wall = 0.0;
        auto lastLoop = clk::now();
        std::vector<float> tl, tr;
        while (! stopFlag.load())
        {
            if (sessionResetReq.exchange (false)) { resetAccum(); }
            const int rs = set.resetSpectrum.load();
            if (rs != lastResetSpectrum) { lastResetSpectrum = rs; resetAccum(); }
            const auto t0 = clk::now();
            configureSpec();
            configureSpectrum();
            hits.setThreshold (set.hitThresh.load());
            uint64_t w = ring.written();
            if (w < pos) { pos = w; nextCol = pos + (uint64_t) hopCol; nextSpec = pos + 1024; }
            if (w - pos > StereoRing::kCap * 3 / 4)
            {
                pos = w - (uint64_t) std::max (specN, pN) - 4096;
                nextCol = pos + (uint64_t) hopCol; nextSpec = pos + 1024;
                ++dropped;
            }
            bool did = false;
            if (w > pos)
            {
                const uint64_t target = std::min<uint64_t> (w, pos + 2048);
                const int n = (int) (target - pos);
                tl.resize ((size_t) n); tr.resize ((size_t) n);
                for (int i = 0; i < n; ++i) { tl[(size_t) i] = ring.L (pos + (uint64_t) i); tr[(size_t) i] = ring.R (pos + (uint64_t) i); }
                const bool act = active.load();
                feedEnvelope (tl.data(), tr.data(), n, act);
                for (int c = 0; c < kCodecs; ++c) if (act) codec[c].process (tl.data(), tr.data(), n);
                pos = target;
                while (nextCol <= pos) { if (act && ! set.hold.load()) specColumn (nextCol); nextCol += (uint64_t) hopCol; }
                while (nextSpec <= pos) { spectrumFrame (nextSpec, act); nextSpec += 1024; }
                did = true;
            }
            const auto t1 = clk::now();
            busy += std::chrono::duration<double> (t1 - t0).count();
            if (t1 - lastPub > std::chrono::milliseconds (60))
            {
                wall = std::chrono::duration<double> (t1 - lastLoop).count();
                lastLoop = t1;
                load = 0.7f * load + 0.3f * (float) (100.0 * busy / std::max (1e-6, wall));
                busy = 0.0;
                publish();
                lastPub = t1;
            }
            if (! did) std::this_thread::sleep_for (std::chrono::milliseconds (2));
        }
    }

    // ---------- enveloppe 1 ms ----------
    void feedEnvelope (const float* L, const float* R, int n, bool act)
    {
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            binMin = std::min (binMin, m); binMax = std::max (binMax, m);
            if (++binCount >= binSamples)
            {
                const float pk = std::max (std::abs (binMin), std::abs (binMax));
                recentPk[(size_t) (binPos++ & 15)] = pk;
                float mxp = 0; for (int k = 0; k < 11; ++k) mxp = std::max (mxp, recentPk[(size_t) ((binPos - 1 - (uint64_t) k) & 15)]);
                const float e = toDbAmp (mxp);
                if (act && ! set.hold.load()) { envelope.push (binMin, binMax, e); hits.onBin (e); }
                binMin = 1e9f; binMax = -1e9f; binCount = 0;
            }
        }
    }

    // ---------- spectrogramme ----------
    void specColumn (uint64_t end)
    {
        const uint64_t start = end - (uint64_t) specN;
        for (int i = 0; i < specN; ++i)
        {
            sre[(size_t) i] = ring.L (start + (uint64_t) i) * swin[(size_t) i];
            sim[(size_t) i] = ring.R (start + (uint64_t) i) * swin[(size_t) i];
        }
        sfft.forward (sre.data(), sim.data());
        sspec.compute (sre.data(), sim.data(), specN, sscale);
        const uint64_t c = spectrogram.written.load (std::memory_order_relaxed);
        const float* src[3] = { sspec.pst.data(), sspec.pm.data(), sspec.ps.data() };
        for (int p = 0; p < 3; ++p)
        {
            smap.mapPower (src[p], tmpP.data());
            uint8_t* col = spectrogram.columnW (p, c);
            for (int r = 0; r < kSpecRows; ++r) col[r] = SpectrogramStore::quant (tmpP[(size_t) r] > 1e-14f ? 10.0f * std::log10 (tmpP[(size_t) r]) : -140.0f);
        }
        spectrogram.written.store (c + 1, std::memory_order_release);
    }

    // ---------- spectre temps réel + bandes ----------
    void spectrumFrame (uint64_t end, bool act)
    {
        const uint64_t start = end - (uint64_t) pN;
        for (int i = 0; i < pN; ++i)
        {
            pre[(size_t) i] = ring.L (start + (uint64_t) i) * pwin[(size_t) i];
            pim[(size_t) i] = ring.R (start + (uint64_t) i) * pwin[(size_t) i];
        }
        pfft.forward (pre.data(), pim.data());
        pspec.compute (pre.data(), pim.data(), pN, pscale);
        const float* src[kModes] = { pspec.pst.data(), pspec.pm.data(), pspec.ps.data(), pspec.pl.data(), pspec.pr.data() };
        const double tau = Settings::speedMs (set.speed.load()) * 0.001;
        const float alpha = (float) (1.0 - std::exp (-1024.0 / (fs * tau)));
        for (int m = 0; m < kModes; ++m)
        {
            pmap.mapPower (src[m], pwBuf[m].data());
            for (int i = 0; i < kSpecPts; ++i)
            {
                const float p = pwBuf[m][(size_t) i];
                if (! liveInit) liveP[m][(size_t) i] = p; else liveP[m][(size_t) i] += alpha * (p - liveP[m][(size_t) i]);
                if (act) avgSum[m][(size_t) i] += p;
            }
        }
        if (act) ++avgCount;
        liveInit = true;
        // pic de maintien
        const float decay = 12.0f * 1024.0f / (float) fs;
        for (int m = 0; m < kModes; ++m)
            for (int i = 0; i < kSpecPts; ++i)
            {
                const float p = liveP[m][(size_t) i];
                const float db = p > 1e-14f ? 10.0f * std::log10 (p) : -140.0f;
                float& pk = peakDb[m][i];
                if (db >= pk) { pk = db; holdLeft[m][i] = 70; }
                else if (holdLeft[m][i] > 0) --holdLeft[m][i];
                else pk = std::max (db, pk - decay);
            }
        // bandes
        double sPst[kBands], sPm[kBands], sPs[kBands], sCr[kBands], sPl[kBands], sPr[kBands];
        for (int b = 0; b < kBands; ++b)
        {
            double a = 0, bm = 0, bs = 0, c = 0, l = 0, r = 0;
            for (int k = bandK0[b]; k < bandK1[b]; ++k) { a += pspec.pst[(size_t) k]; bm += pspec.pm[(size_t) k]; bs += pspec.ps[(size_t) k]; c += pspec.cr[(size_t) k]; l += pspec.pl[(size_t) k]; r += pspec.pr[(size_t) k]; }
            sPst[b] = a; sPm[b] = bm; sPs[b] = bs; sCr[b] = c; sPl[b] = l; sPr[b] = r;
        }
        for (int b = 0; b < kBands; ++b)
        {
            if (! bandInit) { bPst[b] = sPst[b]; bPm[b] = sPm[b]; bPs[b] = sPs[b]; bCr[b] = sCr[b]; bPl[b] = sPl[b]; bPr[b] = sPr[b]; }
            else
            {
                bPst[b] += alpha * (sPst[b] - bPst[b]); bPm[b] += alpha * (sPm[b] - bPm[b]); bPs[b] += alpha * (sPs[b] - bPs[b]);
                bCr[b] += alpha * (sCr[b] - bCr[b]); bPl[b] += alpha * (sPl[b] - bPl[b]); bPr[b] += alpha * (sPr[b] - bPr[b]);
            }
            if (act) sessSum[b] += sPst[b];
        }
        bandInit = true;
        if (act) ++sessFrames;
        // pic grave (fondamentale kick / 808) sur le spectre lissé par bin
        const double binHz = fs / pN;
        const int k0 = std::max (2, (int) std::ceil (25.0 / binHz)), k1 = std::min (pN / 2 - 2, (int) std::floor (160.0 / binHz));
        for (int k = 0; k <= pN / 2; ++k) pstSm[(size_t) k] += alpha * (pspec.pst[(size_t) k] - pstSm[(size_t) k]);
        int kb = k0; float best = 0; int cnt = 0;
        medBuf.clear();
        for (int k = k0; k <= k1; ++k) { if (pstSm[(size_t) k] > best) { best = pstSm[(size_t) k]; kb = k; } medBuf.push_back (pstSm[(size_t) k]); ++cnt; }
        float medP = 1e-20f;
        if (cnt > 0) { std::nth_element (medBuf.begin(), medBuf.begin() + cnt / 2, medBuf.end()); medP = medBuf[(size_t) (cnt / 2)] + 1e-20f; }
        if (cnt > 0 && best > 1e-12f)
        {
            const float a = 10.0f * std::log10 (pstSm[(size_t) kb - 1] + 1e-20f), b = 10.0f * std::log10 (best), c = 10.0f * std::log10 (pstSm[(size_t) kb + 1] + 1e-20f);
            const float den = a - 2 * b + c;
            const float delta = std::abs (den) > 1e-6f ? clampf (0.5f * (a - c) / den, -0.5f, 0.5f) : 0.0f;
            subHz = (float) ((kb + delta) * binHz);
            subDb = b;
            subProm = b - 10.0f * std::log10 (medP);
        }
        // publication du spectre
        SpectrumBlock sb;
        sb.fmax = spFmax; sb.fftN = pN; sb.frames = ++spFrames;
        for (int m = 0; m < kModes; ++m)
            for (int i = 0; i < kSpecPts; ++i)
            {
                const float p = liveP[m][(size_t) i];
                sb.live[m][i] = p > 1e-14f ? 10.0f * std::log10 (p) : -140.0f;
                const double a = avgCount > 0 ? avgSum[m][(size_t) i] / avgCount : 0.0;
                sb.avg[m][i] = a > 1e-14 ? (float) (10.0 * std::log10 (a)) : -140.0f;
                sb.peak[m][i] = peakDb[m][i];
            }
        spectrumOut.write (sb);
    }

    void publish()
    {
        AnalysisBlock a;
        double totL = 0, totS = 0;
        for (int b = 0; b < kBands; ++b) { totL += bPst[b]; totS += sessSum[b]; }
        for (int b = 0; b < kBands; ++b)
        {
            const double nb = std::max (1, bandK1[b] - bandK0[b]);
            a.bandLiveDb[b] = toDbPow (bPst[b] / nb);
            a.bandShareLive[b] = totL > 1e-18 ? (float) (bPst[b] / totL) : 0.0f;
            a.bandSessDb[b] = sessFrames > 0 ? toDbPow (sessSum[b] / sessFrames / nb) : kFloorDb;
            a.bandShareSess[b] = totS > 1e-18 ? (float) (sessSum[b] / totS) : 0.0f;
            const double d = std::sqrt (bPl[b] * bPr[b]);
            a.bandCorr[b] = d > 1e-18 ? (float) clampf ((float) (bCr[b] / d), -1.0f, 1.0f) : 0.0f;
            a.bandSideDb[b] = (bPm[b] > 1e-18 && bPs[b] > 1e-18) ? (float) (10.0 * std::log10 (bPs[b] / bPm[b])) : -60.0f;
        }
        a.subHz = subHz; a.subDb = subDb; a.subProm = subProm;
        a.sessFrames = (float) (sessFrames * 1024.0 / fs);
        a.loadPct = load; a.dropped = dropped;
        const uint32_t hc = hits.count.load (std::memory_order_acquire);
        a.hitCount = hc;
        if (hc > 0) { a.haveHit = 1; a.lastHit = hits.ring[(hc - 1) % HitTracker::kRingN]; }
        for (int i = 0; i < kCodecs; ++i) a.codec[i] = codec[i].result();
        analysisOut.write (a);
    }

    // ---------- état ----------
    const StereoRing& ring;
    Settings& set;
    std::thread th;
    std::atomic<bool> stopFlag { false }, sessionResetReq { false };
    double fs = 48000.0;
    uint64_t pos = 0, nextCol = 0, nextSpec = 0;
    int hopCol = 240, binSamples = 48, lastResetSpectrum = 0;
    uint32_t dropped = 0, spFrames = 0;
    float load = 0;
    // spectrogramme
    int specFftCur = -1, specN = 4096;
    Fft sfft; std::vector<float> swin, sre, sim, tmpP; float sscale = 1.0f;
    StereoSpectra sspec; OctaveMapper smap;
    // spectre
    int spFftCur = -1, smoothCur = -1, pN = 8192;
    Fft pfft; std::vector<float> pwin, pre, pim, pstSm; float pscale = 1.0f, spFmax = 20000.0f;
    StereoSpectra pspec; OctaveMapper pmap;
    std::vector<float> pwBuf[kModes];
    float liveP[kModes][kSpecPts] {};
    double avgSum[kModes][kSpecPts] {};
    double avgCount = 0;
    float peakDb[kModes][kSpecPts] {};
    int holdLeft[kModes][kSpecPts] {};
    bool liveInit = false, bandInit = false;
    int bandK0[kBands] {}, bandK1[kBands] {};
    double sessSum[kBands] {}, sessFrames = 0;
    double bPst[kBands] {}, bPm[kBands] {}, bPs[kBands] {}, bCr[kBands] {}, bPl[kBands] {}, bPr[kBands] {};
    float subHz = 0, subDb = -140, subProm = 0;
    std::vector<float> medBuf;
    // enveloppe
    float binMin = 1e9f, binMax = -1e9f;
    int binCount = 0;
    uint64_t binPos = 0;
    float recentPk[16] {};
    // codec
    CodecSim codec[kCodecs];
};

} // namespace mask
