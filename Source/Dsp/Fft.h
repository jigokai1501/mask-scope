// mask::scope — FFT complexe radix-2 + fenêtres + lissage en fractions d'octave
#pragma once
#include "Common.h"

namespace mask
{
class Fft
{
public:
    void init (int n)
    {
        size = n;
        logn = 0;
        while ((1 << logn) < n) ++logn;
        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
        {
            cosT[(size_t) i] = (float) std::cos (2.0 * kPi * i / n);
            sinT[(size_t) i] = (float) std::sin (2.0 * kPi * i / n);
        }
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < logn; ++b)
                if (i & (1 << b)) r |= 1 << (logn - 1 - b);
            rev[(size_t) i] = (uint32_t) r;
        }
    }
    int getSize() const noexcept { return size; }

    // sens direct : X[k] = Σ x[n] e^{-2πikn/N}
    void forward (float* re, float* im) const noexcept { run (re, im, -1.0f); }
    // sens inverse (sans 1/N)
    void inverse (float* re, float* im) const noexcept { run (re, im, +1.0f); }

private:
    void run (float* re, float* im, float sgn) const noexcept
    {
        for (int i = 0; i < size; ++i)
        {
            const int j = (int) rev[(size_t) i];
            if (j > i) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        for (int len = 2; len <= size; len <<= 1)
        {
            const int half = len >> 1;
            const int step = size / len;
            for (int i = 0; i < size; i += len)
            {
                for (int k = 0, t = 0; k < half; ++k, t += step)
                {
                    const float wr = cosT[(size_t) t];
                    const float wi = sgn * sinT[(size_t) t];
                    const int a = i + k, b = a + half;
                    const float xr = re[b] * wr - im[b] * wi;
                    const float xi = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - xr; im[b] = im[a] - xi;
                    re[a] += xr;        im[a] += xi;
                }
            }
        }
    }
    int size = 0, logn = 0;
    std::vector<float> cosT, sinT;
    std::vector<uint32_t> rev;
};

inline void makeHann (std::vector<float>& w, int n)
{
    w.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
        w[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / n)); // périodique (analyse spectrale)
}
inline void makeBlackmanHarris (std::vector<float>& w, int n)
{
    w.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double x = 2.0 * kPi * i / n;
        w[(size_t) i] = (float) (0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2 * x) - 0.01168 * std::cos (3 * x));
    }
}

// Spectres de puissance L, R, Mid, Side, Stéréo (moyenne L/R) depuis un seul FFT complexe (L + iR).
// Les puissances sont calibrées : sinusoïde pleine échelle (crête = 1) ⇒ 1,0 sur le bin (hors pertes de scalloping).
struct StereoSpectra
{
    int bins = 0;
    std::vector<float> pl, pr, pm, ps, pst, cr; // cr = Re(XL·conj(XR))
    void init (int fftN)
    {
        bins = fftN / 2 + 1;
        for (auto* v : { &pl, &pr, &pm, &ps, &pst, &cr }) v->assign ((size_t) bins, 0.0f);
    }
    // re/im : sortie FFT de (L·w) + i(R·w). scale = 2 / Σw
    void compute (const float* re, const float* im, int n, float scale)
    {
        const float sc2 = scale * scale;
        for (int k = 0; k < bins; ++k)
        {
            const int k2 = (n - k) & (n - 1);
            const float zr = re[k], zi = im[k];
            const float cr_ = re[k2], ci_ = -im[k2]; // conj(Z[N-k])
            const float lr = 0.5f * (zr + cr_), li = 0.5f * (zi + ci_);
            // (Z - conj(Z[N-k])) / (2i)
            const float dr = zr - cr_, di = zi - ci_;
            const float rr = 0.5f * di, ri = -0.5f * dr;
            const float L2 = lr * lr + li * li, R2 = rr * rr + ri * ri;
            const float mr = 0.5f * (lr + rr), mi = 0.5f * (li + ri);
            const float sr = 0.5f * (lr - rr), si = 0.5f * (li - ri);
            pl[(size_t) k] = L2 * sc2;
            pr[(size_t) k] = R2 * sc2;
            pm[(size_t) k] = (mr * mr + mi * mi) * sc2;
            ps[(size_t) k] = (sr * sr + si * si) * sc2;
            pst[(size_t) k] = 0.5f * (L2 + R2) * sc2;
            cr[(size_t) k] = (lr * rr + li * ri) * sc2;
        }
    }
};

// Largeur de bande équivalente de bruit d'une fenêtre, en bins : N·Σw² / (Σw)²
inline double windowEnbw (const std::vector<float>& w)
{
    double s1 = 0, s2 = 0;
    for (float v : w) { s1 += v; s2 += (double) v * v; }
    return (double) w.size() * s2 / (s1 * s1);
}

// Cartographie bins → points log-espacés : niveau de bande (puissance dans la fenêtre / ENBW).
// Une sinusoïde est lue à son niveau réel quel que soit le lissage ; le bruit rose apparaît plat.
class OctaveMapper
{
public:
    void init (int fftN, double fs, int nPts, double fmin, double fmax, double smoothOct, double enbwBins)
    {
        enbw = enbwBins;
        n = nPts;
        bins = fftN / 2 + 1;
        const double binHz = fs / fftN;
        freq.resize ((size_t) n);
        lo.resize ((size_t) n);
        hi.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double f = fmin * std::pow (fmax / fmin, (double) i / (n - 1));
            freq[(size_t) i] = (float) f;
            const double half = 0.5 * smoothOct;
            double a = f * std::pow (2.0, -half) / binHz, b = f * std::pow (2.0, half) / binHz;
            const double c = 0.5 * (a + b), wd = std::max (b - a, enbw);
            lo[(size_t) i] = (float) (c - 0.5 * wd);
            hi[(size_t) i] = (float) (c + 0.5 * wd);
        }
        cum.assign ((size_t) bins + 1, 0.0);
    }
    int count() const noexcept { return n; }
    float freqAt (int i) const noexcept { return freq[(size_t) i]; }

    // power : bins puissances (calibrées crête²) → out : puissance de bande lissée (pas en dB)
    void mapPower (const float* power, float* out)
    {
        cum[0] = 0.0;
        for (int k = 0; k < bins; ++k) cum[(size_t) k + 1] = cum[(size_t) k] + power[k];
        for (int i = 0; i < n; ++i)
            out[i] = (float) ((cumAt (hi[(size_t) i]) - cumAt (lo[(size_t) i])) / enbw);
    }
    static void toDb (const float* p, float* db, int n)
    {
        for (int i = 0; i < n; ++i) db[i] = p[i] > 1.0e-14f ? 10.0f * std::log10 (p[i]) : kFloorDb;
    }
private:
    // somme de la densité constante par bin (bin k couvre [k-0,5 ; k+0,5[)
    double cumAt (double x) const noexcept
    {
        x = std::max (-0.5, std::min ((double) bins - 0.5, x));
        const double xs = x + 0.5;
        const int k = (int) std::floor (xs);
        const double fr = xs - k;
        const size_t kk = (size_t) std::min (k, bins - 1);
        const double dens = (cum[kk + 1] - cum[kk]);
        return cum[kk] + fr * dens;
    }
    int n = 0, bins = 0;
    double enbw = 1.0;
    std::vector<float> freq, lo, hi;
    std::vector<double> cum;
};

} // namespace mask
