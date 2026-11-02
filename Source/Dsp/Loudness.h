// mask::scope — pondération K (ITU-R BS.1770-4), true peak, mesure gated (EBU R128 / Tech 3341-3342)
#pragma once
#include "Common.h"

namespace mask
{
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

class KWeighting
{
public:
    void prepare (double fs)
    {
        // Étage 1 : plateau d'aigus ; étage 2 : passe-haut RLB (formules ITU, valables à toute fréquence d'échantillonnage)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& s : stage1)
            {
                s.b0 = (Vh + Vb * K / Q + K * K) / a0;
                s.b1 = 2.0 * (K * K - Vh) / a0;
                s.b2 = (Vh - Vb * K / Q + K * K) / a0;
                s.a1 = 2.0 * (K * K - 1.0) / a0;
                s.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& s : stage2)
            {
                s.b0 = 1.0; s.b1 = -2.0; s.b2 = 1.0;
                s.a1 = 2.0 * (K * K - 1.0) / a0;
                s.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        reset();
    }
    void reset() noexcept { for (auto& s : stage1) s.reset(); for (auto& s : stage2) s.reset(); }
    inline double process (int ch, double x) noexcept { return stage2[ch].process (stage1[ch].process (x)); }
private:
    Biquad stage1[2], stage2[2];
};

// True peak : suréchantillonnage polyphasé (×4 <96 kHz, ×2 <192 kHz) — noyau sinc fenêtré Kaiser
class TruePeak
{
public:
    void prepare (double fs)
    {
        factor = fs < 96000.0 ? 4 : (fs < 192000.0 ? 2 : 1);
        const int len = factor * 12 + 1, c = len / 2;
        h.assign ((size_t) len, 0.0f);
        const double beta = 8.0;
        auto i0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        double sum = 0;
        for (int n = 0; n < len; ++n)
        {
            const double x = (double) (n - c) / factor;
            const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (kPi * x) / (kPi * x);
            const double r = (double) (n - c) / (double) c;
            const double win = i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
            h[(size_t) n] = (float) (sinc * win);
            sum += h[(size_t) n];
        }
        for (auto& v : h) v = (float) (v * factor / sum);
        reset();
    }
    void reset() noexcept { std::fill (std::begin (hist), std::end (hist), 0.0f); pos = 0; }
    // Retourne le maximum de |échantillon interpolé| autour de cet échantillon d'entrée.
    inline float process (float x) noexcept
    {
        pos = (pos + 1) & 15;
        hist[pos] = x;
        if (factor == 1) return std::abs (x);
        float m = 0.0f;
        const int len = (int) h.size();
        for (int p = 0; p < factor; ++p)
        {
            float acc = 0.0f;
            for (int k = 0; k < 13; ++k)
            {
                const int idx = factor * k + p;
                if (idx >= len) break;
                acc += h[(size_t) idx] * hist[(pos - k) & 15];
            }
            m = std::max (m, std::abs (acc));
        }
        return m;
    }
private:
    int factor = 4, pos = 0;
    std::vector<float> h;
    float hist[16] {};
};

// Histogramme de blocs de sonie (0,05 LU) pour la mesure gated.
class LoudnessHistogram
{
public:
    static constexpr double kLo = -70.0, kHi = 10.0, kStep = 0.05;
    static constexpr int kBins = (int) ((kHi - kLo) / kStep + 0.5);
    LoudnessHistogram() : cnt (kBins, 0), en (kBins, 0.0) {}
    void clear() { std::fill (cnt.begin(), cnt.end(), 0u); std::fill (en.begin(), en.end(), 0.0); total = 0; }
    // l = sonie du bloc (LUFS), energy = énergie moyenne pondérée K du bloc
    void add (double l, double energy) noexcept
    {
        if (l <= kLo) return;
        int b = (int) ((l - kLo) / kStep);
        b = clampi (b, 0, kBins - 1);
        cnt[(size_t) b]++;
        en[(size_t) b] += energy;
        ++total;
    }
    uint32_t count() const noexcept { return total; }

    // Sonie intégrée selon BS.1770-4 : porte relative à -10 LU (gate=10) ou -20 LU pour la LRA.
    double gatedLoudness (double relGateLu) const noexcept
    {
        if (total == 0) return -120.0;
        double e = 0; uint64_t n = 0;
        for (int b = 0; b < kBins; ++b) { e += en[(size_t) b]; n += cnt[(size_t) b]; }
        if (n == 0) return -120.0;
        const double gate = 10.0 * std::log10 (e / (double) n) - 0.691 - relGateLu;
        double e2 = 0; uint64_t n2 = 0;
        for (int b = 0; b < kBins; ++b)
            if (kLo + b * kStep >= gate) { e2 += en[(size_t) b]; n2 += cnt[(size_t) b]; }
        if (n2 == 0) return -120.0;
        return 10.0 * std::log10 (e2 / (double) n2) - 0.691;
    }
    double gateLevel (double relGateLu) const noexcept
    {
        double e = 0; uint64_t n = 0;
        for (int b = 0; b < kBins; ++b) { e += en[(size_t) b]; n += cnt[(size_t) b]; }
        return n ? 10.0 * std::log10 (e / (double) n) - 0.691 - relGateLu : -70.0;
    }
    // LRA : écart entre les centiles 10 et 95 des valeurs court terme au-dessus de la porte (-20 LU)
    double range() const noexcept
    {
        if (total == 0) return 0.0;
        const double gate = gateLevel (20.0);
        uint64_t n = 0;
        for (int b = 0; b < kBins; ++b) if (kLo + b * kStep >= gate) n += cnt[(size_t) b];
        if (n < 2) return 0.0;
        auto pct = [&] (double p)
        {
            const double target = p * (double) (n - 1);
            uint64_t acc = 0;
            for (int b = 0; b < kBins; ++b)
            {
                if (kLo + b * kStep < gate) continue;
                acc += cnt[(size_t) b];
                if ((double) acc > target) return kLo + (b + 0.5) * kStep;
            }
            return kHi;
        };
        return std::max (0.0, pct (0.95) - pct (0.10));
    }
    // distribution (nb de blocs par intervalle) sur [lo, lo+step*nOut]
    void coarse (float* out, int nOut, double lo, double step) const noexcept
    {
        std::fill (out, out + nOut, 0.0f);
        for (int b = 0; b < kBins; ++b)
        {
            if (! cnt[(size_t) b]) continue;
            const double l = kLo + (b + 0.5) * kStep;
            int i = (int) std::floor ((l - lo) / step);
            i = clampi (i, 0, nOut - 1);
            out[i] += (float) cnt[(size_t) b];
        }
    }
private:
    std::vector<uint32_t> cnt;
    std::vector<double> en;
    uint32_t total = 0;
};

} // namespace mask
