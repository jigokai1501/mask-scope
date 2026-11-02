// mask::scope — mesures côté thread audio : sonie (M/S/I/LRA), true peak, niveaux, corrélation, historique
#pragma once
#include "Loudness.h"

namespace mask
{
constexpr int kHistBins = 100;          // distribution court terme : -50 … 0 LUFS, pas de 0,5 LU
constexpr double kHistLo = -50.0, kHistStep = 0.5;

struct MeterBlock
{
    float lufsM = -120, lufsS = -120, lufsI = -120, lra = 0;
    float lufsMmax = -120, lufsSmax = -120;
    float tpL = -120, tpR = -120, tpMax = -120, tp3s = -120;
    float peakL = -120, peakR = -120, peakMax = -120;
    float rmsL = -120, rmsR = -120, rms3s = -120, crest3s = 0;
    float plr = 0, psr = 0;
    float corr = 0, balanceDb = 0, midDb = -120, sideDb = -120, monoFoldDb = 0;
    float dcDb = -140;
    float gateLevel = -70;      // seuil relatif courant (LUFS)
    float measuredSec = 0;
    float sessionSec = 0;
    uint32_t clipCount = 0, tpOverCount = 0;
    uint32_t resetCount = 0;
    uint8_t playing = 0, active = 0, haveHost = 0, mono = 0;
    float hist[kHistBins] {};
};

struct HistPoint { float m, s, tp, rms, corr, crest; };

class HistoryStore
{
public:
    static constexpr int kCap = 72000; // 2 h à 10 Hz
    HistoryStore() : pts ((size_t) kCap) {}
    void clear() noexcept { count.store (0, std::memory_order_release); }
    void push (const HistPoint& p) noexcept
    {
        const uint64_t c = count.load (std::memory_order_relaxed);
        pts[(size_t) (c % kCap)] = p;
        count.store (c + 1, std::memory_order_release);
    }
    uint64_t size() const noexcept { return count.load (std::memory_order_acquire); }
    HistPoint at (uint64_t i) const noexcept { return pts[(size_t) (i % kCap)]; }
    uint64_t first() const noexcept { const auto c = size(); return c > kCap ? c - kCap + 64 : 0; }
private:
    std::vector<HistPoint> pts;
    std::atomic<uint64_t> count { 0 };
};

class MeterEngine
{
public:
    void prepare (double sampleRate, bool isMono)
    {
        fs = sampleRate;
        mono = isMono;
        subLen = std::max (1, (int) std::lround (fs / 100.0));
        kw.prepare (fs);
        tpL.prepare (fs);
        tpR.prepare (fs);
        resetStats();
        std::fill (std::begin (ring), std::end (ring), Sub {});
        wp = 0; filled = 0; subN = 0; blockCounter = 0;
        sLL = sRR = sLR = 0;
    }
    void requestReset() noexcept { resetReq.store (true); }
    HistoryStore history;
    SeqLock<MeterBlock> out;

    void process (const float* L, const float* R, int n, bool playing, bool haveHost) noexcept
    {
        if (resetReq.exchange (false)) resetStats();
        isPlaying = playing; hostKnown = haveHost;
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i];
            const double r = mono ? 0.0 : (double) R[i];
            const double zl = kw.process (0, l);
            const double zr = mono ? 0.0 : kw.process (1, r);
            cur.e += zl * zl + zr * zr;
            cur.ll += l * l; cur.rr += r * r; cur.lr += l * r;
            cur.dcl += l; cur.dcr += r;
            const float al = (float) std::abs (l), ar = (float) std::abs (r);
            cur.pkL = std::max (cur.pkL, al); cur.pkR = std::max (cur.pkR, ar);
            const float tl = tpL.process ((float) l), tr = mono ? 0.0f : tpR.process ((float) r);
            cur.tpL = std::max (cur.tpL, tl); cur.tpR = std::max (cur.tpR, tr);
            if (al >= 0.9999f || ar >= 0.9999f) cur.clip = true;
            if (++subN >= subLen) endSub();
        }
        publish();
    }

private:
    struct Sub { double e = 0, ll = 0, rr = 0, lr = 0; float pkL = 0, pkR = 0, tpL = 0, tpR = 0; double dcl = 0, dcr = 0; bool clip = false; };

    void resetStats() noexcept
    {
        histInt.clear(); histLra.clear();
        lufsMmax = lufsSmax = -120; peakMax = tpMax = 0.0f; peakMaxDb = tpMaxDb = -120.0f;
        clipCount = tpOverCount = 0; prevClip = prevTpOver = false;
        measuredBlocks = 0; sessionBlocks = 0;
        history.clear();
        ++resetCount;
    }

    static double lufs (double meanEnergy) noexcept { return meanEnergy > 1e-12 ? 10.0 * std::log10 (meanEnergy) - 0.691 : -120.0; }

    void endSub() noexcept
    {
        subN = 0;
        const double n = (double) subLen;
        ring[wp] = cur;
        wp = (wp + 1) % kRing;
        filled = std::min (filled + 1, kRing);
        cur = Sub {};

        // fenêtres glissantes calculées par somme directe (300 additions / 10 ms)
        auto sumLast = [&] (int cnt, double Sub::*f) { double s = 0; for (int k = 1; k <= cnt; ++k) s += ring[(wp + kRing - k) % kRing].*f; return s; };
        auto maxLast = [&] (int cnt, float Sub::*f) { float m = 0; for (int k = 1; k <= cnt; ++k) m = std::max (m, ring[(wp + kRing - k) % kRing].*f); return m; };

        const double eM = sumLast (40, &Sub::e) / (40.0 * n);
        const double eS = sumLast (300, &Sub::e) / (300.0 * n);
        lufsMnow = (float) lufs (eM);
        lufsSnow = filled >= 300 ? (float) lufs (eS) : (float) lufs (eS * 300.0 / std::max (1, filled));
        const bool rec = isPlaying || ! hostKnown || lufsMnow > -70.0f;

        // stéréo lissée (τ ≈ 400 ms)
        const Sub& s = ring[(wp + kRing - 1) % kRing];
        const double a = std::exp (-0.01 / 0.4);
        sLL = a * sLL + (1 - a) * s.ll / n; sRR = a * sRR + (1 - a) * s.rr / n; sLR = a * sLR + (1 - a) * s.lr / n;
        const double ad = std::exp (-0.01 / 2.0);
        dcL = ad * dcL + (1 - ad) * s.dcl / n; dcR = ad * dcR + (1 - ad) * s.dcr / n;

        if (s.clip && ! prevClip && rec) ++clipCount;
        prevClip = s.clip;
        const bool tpOver = std::max (s.tpL, s.tpR) > 1.0f;
        if (tpOver && ! prevTpOver && rec) ++tpOverCount;
        prevTpOver = tpOver;

        if (rec)
        {
            peakMax = std::max (peakMax, std::max (s.pkL, s.pkR));
            tpMax = std::max (tpMax, std::max (s.tpL, s.tpR));
        }

        if (++blockCounter >= 10)
        {
            blockCounter = 0;
            if (rec)
            {
                ++sessionBlocks;
                if (filled >= 40) { histInt.add (lufs (eM), eM); ++measuredBlocks; lufsMmax = std::max (lufsMmax, lufsMnow); }
                if (filled >= 300)
                {
                    histLra.add (lufs (eS), eS);
                    lufsSmax = std::max (lufsSmax, lufsSnow);
                    histLra.coarse (coarse, kHistBins, kHistLo, kHistStep);
                }
                const double rmsAll = (sumLast (300, &Sub::ll) + sumLast (300, &Sub::rr)) / (300.0 * n * (mono ? 1.0 : 2.0));
                const float tp3 = std::max (maxLast (300, &Sub::tpL), maxLast (300, &Sub::tpR));
                const float pk3 = std::max (maxLast (300, &Sub::pkL), maxLast (300, &Sub::pkR));
                const float rms3 = toDbPow (rmsAll);
                history.push ({ lufsMnow, lufsSnow, toDbAmp (tp3), rms3, currentCorr(), toDbAmp (pk3) - rms3 });
            }
        }
    }

    float currentCorr() const noexcept
    {
        if (mono) return 1.0f;
        const double d = std::sqrt (sLL * sRR);
        return d > 1e-12 ? (float) clampf ((float) (sLR / d), -1.0f, 1.0f) : 0.0f;
    }

    void publish() noexcept
    {
        MeterBlock b;
        const double n = (double) subLen;
        b.lufsM = lufsMnow; b.lufsS = lufsSnow;
        b.lufsI = (float) histInt.gatedLoudness (10.0);
        b.gateLevel = (float) std::max (-70.0, histInt.gateLevel (10.0));
        b.lra = (float) histLra.range();
        b.lufsMmax = lufsMmax; b.lufsSmax = lufsSmax;
        auto maxLast = [&] (int cnt, float Sub::*f) { float m = 0; for (int k = 1; k <= cnt && k <= filled; ++k) m = std::max (m, ring[(wp + kRing - k) % kRing].*f); return m; };
        auto sumLast = [&] (int cnt, double Sub::*f) { double s = 0; for (int k = 1; k <= cnt && k <= filled; ++k) s += ring[(wp + kRing - k) % kRing].*f; return s; };
        b.tpL = toDbAmp (maxLast (30, &Sub::tpL)); b.tpR = toDbAmp (maxLast (30, &Sub::tpR));
        b.tp3s = toDbAmp (std::max (maxLast (300, &Sub::tpL), maxLast (300, &Sub::tpR)));
        b.tpMax = toDbAmp (tpMax);
        b.peakL = toDbAmp (maxLast (10, &Sub::pkL)); b.peakR = toDbAmp (maxLast (10, &Sub::pkR));
        b.peakMax = toDbAmp (peakMax);
        b.rmsL = toDbPow (sumLast (30, &Sub::ll) / (30.0 * n)); b.rmsR = toDbPow (sumLast (30, &Sub::rr) / (30.0 * n));
        const double rmsAll = (sumLast (300, &Sub::ll) + sumLast (300, &Sub::rr)) / (300.0 * n * (mono ? 1.0 : 2.0));
        b.rms3s = toDbPow (rmsAll);
        const float pk3 = toDbAmp (std::max (maxLast (300, &Sub::pkL), maxLast (300, &Sub::pkR)));
        b.crest3s = b.rms3s > -100.0f ? pk3 - b.rms3s : 0.0f;
        b.plr = (b.lufsI > -100.0f && b.tpMax > -100.0f) ? b.tpMax - b.lufsI : 0.0f;
        b.psr = (b.lufsS > -100.0f && b.tp3s > -100.0f) ? b.tp3s - b.lufsS : 0.0f;
        b.corr = currentCorr();
        b.balanceDb = (sLL > 1e-12 && sRR > 1e-12) ? (float) (10.0 * std::log10 (sLL / sRR)) : 0.0f;
        const double mE = 0.25 * (sLL + sRR + 2 * sLR), sE = 0.25 * (sLL + sRR - 2 * sLR);
        b.midDb = toDbPow (mE); b.sideDb = toDbPow (std::max (0.0, sE));
        b.monoFoldDb = (sLL + sRR) > 1e-12 ? (float) (10.0 * std::log10 (std::max (1e-9, mE / (0.5 * (sLL + sRR))))) : 0.0f;
        b.dcDb = toDbAmp ((float) std::max (std::abs (dcL), std::abs (dcR)));
        b.measuredSec = (float) measuredBlocks * 0.1f;
        b.sessionSec = (float) sessionBlocks * 0.1f;
        b.clipCount = clipCount; b.tpOverCount = tpOverCount; b.resetCount = resetCount;
        b.playing = isPlaying; b.haveHost = hostKnown; b.mono = mono; b.active = (lufsMnow > -70.0f || isPlaying);
        std::memcpy (b.hist, coarse, sizeof (coarse));
        out.write (b);
    }

    static constexpr int kRing = 300;
    double fs = 48000.0;
    int subLen = 480, subN = 0, wp = 0, filled = 0, blockCounter = 0;
    bool mono = false, isPlaying = false, hostKnown = false;
    KWeighting kw;
    TruePeak tpL, tpR;
    Sub cur, ring[kRing];
    LoudnessHistogram histInt, histLra;
    float coarse[kHistBins] {};
    double sLL = 0, sRR = 0, sLR = 0, dcL = 0, dcR = 0;
    float lufsMnow = -120, lufsSnow = -120, lufsMmax = -120, lufsSmax = -120;
    float peakMax = 0, tpMax = 0, peakMaxDb = -120, tpMaxDb = -120;
    uint32_t clipCount = 0, tpOverCount = 0, resetCount = 0;
    int measuredBlocks = 0, sessionBlocks = 0;
    bool prevClip = false, prevTpOver = false;
    std::atomic<bool> resetReq { false };
};

} // namespace mask
