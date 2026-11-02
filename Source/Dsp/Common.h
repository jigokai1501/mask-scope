// mask::scope — briques communes (sans JUCE)
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace mask
{
constexpr double kPi = 3.14159265358979323846;
constexpr float kFloorDb = -140.0f;

inline float toDbAmp (float a) noexcept { return a > 1.0e-7f ? 20.0f * std::log10 (a) : kFloorDb; }
inline float toDbPow (double p) noexcept { return p > 1.0e-14 ? (float) (10.0 * std::log10 (p)) : kFloorDb; }
inline float fromDb (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float clampf (float v, float lo, float hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi (int v, int lo, int hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

// Copie cohérente d'une structure POD entre deux threads (un seul écrivain).
template <typename T>
class SeqLock
{
public:
    void write (const T& v) noexcept
    {
        seq.fetch_add (1, std::memory_order_acq_rel);
        std::memcpy ((void*) &data, (const void*) &v, sizeof (T));
        seq.fetch_add (1, std::memory_order_release);
    }
    bool read (T& out) const noexcept
    {
        for (int tries = 0; tries < 8; ++tries)
        {
            const auto s1 = seq.load (std::memory_order_acquire);
            if (s1 & 1u) continue;
            std::memcpy ((void*) &out, (const void*) &data, sizeof (T));
            std::atomic_thread_fence (std::memory_order_acquire);
            if (seq.load (std::memory_order_relaxed) == s1) return true;
        }
        return false;
    }
    uint32_t version() const noexcept { return seq.load (std::memory_order_acquire) >> 1; }
private:
    std::atomic<uint32_t> seq { 0 };
    T data {};
};

// Anneau stéréo : un écrivain (thread audio), un lecteur (thread d'analyse) + lecture d'affichage.
class StereoRing
{
public:
    static constexpr size_t kCap = 1u << 18; // ≈ 5,5 s à 48 kHz
    static constexpr size_t kMask = kCap - 1;

    StereoRing() : l (kCap, 0.0f), r (kCap, 0.0f) {}
    void clear() { std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f); w.store (0); }

    void push (const float* L, const float* R, int n) noexcept
    {
        uint64_t p = w.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            l[(p + (uint64_t) i) & kMask] = L[i];
            r[(p + (uint64_t) i) & kMask] = R[i];
        }
        w.store (p + (uint64_t) n, std::memory_order_release);
    }
    uint64_t written() const noexcept { return w.load (std::memory_order_acquire); }
    float L (uint64_t idx) const noexcept { return l[idx & kMask]; }
    float R (uint64_t idx) const noexcept { return r[idx & kMask]; }

private:
    std::vector<float> l, r;
    std::atomic<uint64_t> w { 0 };
};

// Durée en valeur de note (texte) selon le tempo.
inline const char* noteName (double ms, double bpm) noexcept
{
    static const struct { double beats; const char* n; } t[] = {
        { 1.0 / 16.0, "1/64" }, { 1.0 / 8.0, "1/32" }, { 1.0 / 6.0, "1/16T" }, { 0.25, "1/16" }, { 1.0 / 3.0, "1/8T" },
        { 0.375, "1/16." }, { 0.5, "1/8" }, { 2.0 / 3.0, "1/4T" }, { 0.75, "1/8." }, { 1.0, "1/4" },
        { 4.0 / 3.0, "1/2T" }, { 1.5, "1/4." }, { 2.0, "1/2" }, { 3.0, "1/2." }, { 4.0, "1 mesure" },
        { 6.0, "1,5 mesure" }, { 8.0, "2 mesures" }, { 16.0, "4 mesures" } };
    if (bpm <= 1.0 || ms <= 0.0) return "";
    const double beats = ms / (60000.0 / bpm);
    const char* best = t[0].n;
    double bestErr = 1.0e9;
    for (auto& e : t)
    {
        const double err = std::abs (std::log (beats / e.beats));
        if (err < bestErr) { bestErr = err; best = e.n; }
    }
    return best;
}

inline double beatMs (double bpm) noexcept { return bpm > 1.0 ? 60000.0 / bpm : 0.0; }

// Nom de note (A4 = 440 Hz) et écart en cents.
inline int noteFromHz (double hz, int& cents) noexcept
{
    if (hz < 8.0) { cents = 0; return 0; }
    const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
    const int m = (int) std::lround (midi);
    cents = (int) std::lround ((midi - m) * 100.0);
    return m;
}
inline const char* midiName (int m) noexcept
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((m % 12) + 12) % 12];
}
inline int midiOctave (int m) noexcept { return (m / 12) - 1 - (m < 0 && m % 12 != 0 ? 1 : 0); }

} // namespace mask
