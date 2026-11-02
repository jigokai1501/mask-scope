// mask::scope — enveloppe 1 ms, détection de coups (hits) et analyse ADSR
#pragma once
#include "Common.h"

namespace mask
{
struct HitSummary
{
    uint64_t startBin = 0;      // bin de 1 ms absolu où le coup commence
    float peakDb = -120.0f;
    float aMs = 0, dMs = 0, sMs = 0, rMs = 0;
    float sustainDb = 0;        // niveau (rel. crête) au début de S (ou de R s'il n'y a pas de S)
    float slopeD = 0, slopeS = 0, slopeR = 0;   // dB/s
    float t20 = 0, t40 = 0, t60 = 0;            // ms depuis la crête (0 = non atteint)
    float lengthMs = 0;
    float peakOffMs = 0;        // position de la crête depuis le début du coup
    uint8_t truncated = 0;      // coupé par le coup suivant
    uint8_t hasD = 0, hasS = 0, hasR = 0;
};

struct HitDetail
{
    HitSummary s;
    int n = 0;
    float stepMs = 1.0f;
    float envDb[256] {};        // relatif à la crête
    float tPeak = 0, tA0 = 0;                  // positions (ms depuis le début du coup)
    int nSeg = 0;                              // segments de la queue : rôle 'D', 'S' ou 'R'
    char segRole[3] {};
    float segT0[3] {}, segT1[3] {};
    uint32_t serial = 0;
};

inline int firstBelow (const std::vector<float>& y, int from, int to, float level)
{
    for (int j = from; j < to; ++j) if (y[(size_t) j] <= level) return j;
    return -1;
}

// y : enveloppe en dB absolue (1 point = 1 ms), p = indice de la crête
inline HitDetail analyzeHit (const std::vector<float>& y, int p, bool truncated, uint64_t startBin)
{
    HitDetail d;
    const int n = (int) y.size();
    const float peak = y[(size_t) p];
    d.s.startBin = startBin;
    d.s.peakDb = peak;
    d.s.truncated = truncated ? 1 : 0;
    d.s.lengthMs = (float) n;

    // attaque : du dernier point à -20 dB (10 % de l'amplitude) avant la crête jusqu'à la crête
    int s10 = 0;
    for (int j = p; j >= 0; --j) if (y[(size_t) j] <= peak - 20.0f) { s10 = j; break; }
    d.s.aMs = (float) std::max (1, p - s10);
    d.tA0 = (float) s10; d.tPeak = (float) p; d.s.peakOffMs = (float) p;

    // fin de la queue : dernier point > crête - 60 dB (ou fin de l'enregistrement si coupé)
    int e = n - 1;
    if (! truncated) { e = p; for (int j = n - 1; j > p; --j) if (y[(size_t) j] >= peak - 60.0f) { e = j; break; } }
    if (e <= p + 2) e = std::min (n - 1, p + 3);

    auto tReach = [&] (float drop) { const int j = firstBelow (y, p, n, peak - drop); return j >= 0 ? (float) (j - p) : 0.0f; };
    d.s.t20 = tReach (20.0f); d.s.t40 = tReach (40.0f); d.s.t60 = tReach (60.0f);

    // simplification polygonale (Douglas-Peucker limité à 2 coupures) de la queue
    const float eps = 2.5f;
    auto bestSplit = [&] (int a, int b, float& dev)
    {
        int bi = -1; dev = 0.0f;
        if (b - a < 4) return -1;
        const float ya = y[(size_t) a], yb = y[(size_t) b];
        for (int j = a + 1; j < b; ++j)
        {
            const float line = ya + (yb - ya) * (float) (j - a) / (float) (b - a);
            const float dd = std::abs (y[(size_t) j] - line);
            if (dd > dev) { dev = dd; bi = j; }
        }
        return bi;
    };
    std::vector<int> cuts;
    float dv = 0.0f;
    const int s0 = bestSplit (p, e, dv);
    if (s0 >= 0 && dv >= eps)
    {
        float dl, dr;
        const int sl = bestSplit (p, s0, dl), sr = bestSplit (s0, e, dr);
        if (sl >= 0 && dl >= eps && dl >= dr) { cuts = { sl, s0 }; }
        else if (sr >= 0 && dr >= eps) { cuts = { s0, sr }; }
        else cuts = { s0 };
    }
    std::vector<int> bounds { p };
    for (int c : cuts) bounds.push_back (c);
    bounds.push_back (e);
    const int ns = (int) bounds.size() - 1;
    struct Seg { float ms, slope, startDb; };
    std::vector<Seg> segs;
    for (int i = 0; i < ns; ++i)
    {
        const float ms = (float) (bounds[(size_t) i + 1] - bounds[(size_t) i]);
        const float dB = y[(size_t) bounds[(size_t) i + 1]] - y[(size_t) bounds[(size_t) i]];
        segs.push_back ({ ms, ms > 0 ? dB / (ms * 0.001f) : 0.0f, y[(size_t) bounds[(size_t) i]] - peak });
    }
    const float flat = 12.0f; // |pente| < 12 dB/s ⇒ plateau (sustain)
    auto setD = [&] (const Seg& g) { d.s.dMs = g.ms; d.s.slopeD = g.slope; d.s.hasD = 1; };
    auto setS = [&] (const Seg& g) { d.s.sMs = g.ms; d.s.slopeS = g.slope; d.s.hasS = 1; d.s.sustainDb = g.startDb; };
    auto setR = [&] (const Seg& g) { d.s.rMs = g.ms; d.s.slopeR = g.slope; d.s.hasR = 1; if (! d.s.hasS) d.s.sustainDb = g.startDb; };
    d.nSeg = ns;
    for (int i = 0; i < ns; ++i)
    {
        char role;
        if (ns == 1)      role = std::abs (segs[(size_t) i].slope) < flat ? 'S' : 'D';
        else if (ns == 2) role = i == 0 ? (std::abs (segs[0].slope) < flat ? 'S' : 'D') : 'R';
        else              role = i == 0 ? 'D' : (i == 1 ? 'S' : 'R');
        d.segRole[i] = role;
        d.segT0[i] = (float) bounds[(size_t) i];
        d.segT1[i] = (float) bounds[(size_t) i + 1];
        if (role == 'D') setD (segs[(size_t) i]); else if (role == 'S') setS (segs[(size_t) i]); else setR (segs[(size_t) i]);
    }

    // courbe décimée (≤ 256 points), relative à la crête, depuis le début
    const int len = std::min (n, e + 1 + std::max (20, (e - p) / 8));
    const int stride = std::max (1, (len + 255) / 256);
    d.stepMs = (float) stride;
    d.n = 0;
    for (int j = 0; j < len && d.n < 256; j += stride)
    {
        float m = -120.0f;
        for (int k = j; k < std::min (len, j + stride); ++k) m = std::max (m, y[(size_t) k]);
        d.envDb[d.n++] = std::max (-90.0f, m - peak);
    }
    return d;
}

class HitTracker
{
public:
    static constexpr int kMaxLen = 3000;
    static constexpr int kRingN = 64;

    void reset()
    {
        state = 0; i = 0; sinceOnset = 1000; cur.clear();
        std::fill (std::begin (recent), std::end (recent), -120.0f);
        count.store (0);
    }
    void setThreshold (float db) noexcept { thresh = db; }

    void onBin (float e)
    {
        recent[i & 63] = e;
        float fl = 1000.0f; int flIdx = 0;
        for (int k = 1; k <= 30; ++k) { const float v = recent[(i - (uint64_t) k) & 63]; if (v < fl) { fl = v; flIdx = k; } }
        const bool rise = e > thresh && (e - fl) > 9.0f && sinceOnset > 40;
        if (state == 0)
        {
            if (rise) begin (fl, flIdx);
        }
        else
        {
            cur.push_back (e);
            if ((int) cur.size() - startOff < 120 && e > peakDb) { peakDb = e; peakIdx = (int) cur.size() - 1; }
            if (rise)
            {
                // le nouveau coup démarre au point bas ; le précédent s'arrête là
                const int cut = (int) cur.size() - 1 - flIdx;
                if (cut > 15) finalize (cut, true);
                else { cur.clear(); state = 0; }
                begin (fl, flIdx);
            }
            else if (e < peakDb - 60.0f || e < -85.0f || (int) cur.size() >= kMaxLen) finalize ((int) cur.size(), false);
        }
        ++i; ++sinceOnset;
    }

    std::atomic<uint32_t> count { 0 };
    HitSummary ring[kRingN];
    SeqLock<HitDetail> latest;
    uint32_t serial = 0;

private:
    void begin (float fl, int flIdx)
    {
        // reconstruit le début depuis les derniers bins (retour au point bas)
        cur.clear();
        for (int k = flIdx; k >= 1; --k) cur.push_back (recent[(i - (uint64_t) k) & 63]);
        cur.push_back (recent[i & 63]);
        startBin = i - (uint64_t) flIdx;
        peakDb = -200.0f; peakIdx = 0; startOff = 0;
        for (int k = 0; k < (int) cur.size(); ++k) if (cur[(size_t) k] > peakDb) { peakDb = cur[(size_t) k]; peakIdx = k; }
        (void) fl;
        state = 1; sinceOnset = 0;
    }
    void finalize (int len, bool truncated)
    {
        state = 0;
        if (len < 15) { cur.clear(); return; }
        std::vector<float> y (cur.begin(), cur.begin() + len);
        int p = std::min (peakIdx, len - 1);
        HitDetail d = analyzeHit (y, p, truncated, startBin);
        d.serial = ++serial;
        const uint32_t c = count.load (std::memory_order_relaxed);
        ring[c % kRingN] = d.s;
        count.store (c + 1, std::memory_order_release);
        latest.write (d);
        cur.clear();
    }

    int state = 0, peakIdx = 0, startOff = 0;
    uint64_t i = 0, startBin = 0;
    int sinceOnset = 1000;
    float peakDb = -200.0f, thresh = -48.0f;
    float recent[64] {};
    std::vector<float> cur;
};

// Enveloppe : min/max de l'échantillon mid par ms + enveloppe de crête (max glissant sur 11 ms)
class EnvStore
{
public:
    static constexpr int kCap = 65536;
    EnvStore() : mn ((size_t) kCap, 0.0f), mx ((size_t) kCap, 0.0f), env ((size_t) kCap, -120.0f) {}
    void clear() { written.store (0); std::fill (mn.begin(), mn.end(), 0.0f); std::fill (mx.begin(), mx.end(), 0.0f); std::fill (env.begin(), env.end(), -120.0f); }
    uint64_t size() const noexcept { return written.load (std::memory_order_acquire); }
    void push (float lo, float hi, float envDb) noexcept
    {
        const uint64_t c = written.load (std::memory_order_relaxed);
        const size_t k = (size_t) (c % kCap);
        mn[k] = lo; mx[k] = hi; env[k] = envDb;
        written.store (c + 1, std::memory_order_release);
    }
    float minAt (uint64_t i) const noexcept { return mn[(size_t) (i % kCap)]; }
    float maxAt (uint64_t i) const noexcept { return mx[(size_t) (i % kCap)]; }
    float envAt (uint64_t i) const noexcept { return env[(size_t) (i % kCap)]; }
private:
    std::vector<float> mn, mx, env;
    std::atomic<uint64_t> written { 0 };
};

} // namespace mask
