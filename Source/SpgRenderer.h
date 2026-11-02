// mask::scope — rendu incrémental du spectrogramme dans une image circulaire
#pragma once
#include "Look.h"

class SpgRenderer
{
public:
    static constexpr int W = 2048;
    static constexpr int H = mask::kSpecRows;

    struct View
    {
        int plane = 0;          // 0 ST · 1 MID · 2 SIDE
        float timeSec = 8.0f;
        float floorDb = -90.0f, ceilDb = -10.0f;
        float tilt = 0.0f;      // dB/octave
        int palette = 0;
        bool operator== (const View& o) const { return plane == o.plane && timeSec == o.timeSec && floorDb == o.floorDb && ceilDb == o.ceilDb && tilt == o.tilt && palette == o.palette; }
    };

    SpgRenderer() : img (juce::Image::ARGB, W, H, true) {}

    // dernier pixel écrit (pour la lecture au survol)
    int writeX() const noexcept { return wx; }
    const juce::Image& image() const noexcept { return img; }
    uint64_t newestColumn() const noexcept { return drawn; }

    void update (const mask::SpectrogramStore& st, const View& v, double fmax)
    {
        const uint64_t written = st.written.load (std::memory_order_acquire);
        const bool changed = ! (v == view) || written < drawn || fmaxCur != fmax;
        if (changed)
        {
            view = v; fmaxCur = fmax; rebuildTilt(); lut = &look::paletteLut (v.palette); clearAll();
            const uint64_t span = (uint64_t) (v.timeSec * mask::kColsPerSec);
            drawn = written > span + 2 ? written - span - 2 : 0;       // re-rendu de l'historique récent
        }
        if (written <= drawn) return;
        const uint64_t avail = std::min<uint64_t> (written - drawn, (uint64_t) (mask::kSpecCols - 16));
        if (written - drawn > (uint64_t) (mask::kSpecCols - 16)) drawn = written - avail;
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float pxPerCol = (float) W / (v.timeSec * (float) mask::kColsPerSec);
        for (; drawn < written; ++drawn)
        {
            const uint8_t* col = st.column (v.plane, drawn);
            for (int r = 0; r < H; ++r) pend[(size_t) r] = std::max (pend[(size_t) r], col[r]);
            const float nxt = acc + pxPerCol;
            const int n = (int) std::floor (nxt);
            for (int k = 0; k < n; ++k) writePixelColumn (bd);
            acc = nxt - (float) n;
            if (n > 0) pend.fill (0);      // si pxPerCol < 1, pend accumule jusqu'à franchir un pixel
        }
    }

    // colonne source la plus récente affichée à la position x01 (0 = ancien, 1 = récent)
    uint64_t columnAt (float x01) const noexcept
    {
        const float ago = (1.0f - x01) * view.timeSec * (float) mask::kColsPerSec;
        const uint64_t a = (uint64_t) std::max (0.0f, ago);
        return drawn > a + 1 ? drawn - 1 - a : 0;
    }
    const View& currentView() const noexcept { return view; }

private:
    void clearAll()
    {
        img.clear (img.getBounds());
        pend.fill (0);
        wx = 0; acc = 0.0f;
    }
    void rebuildTilt()
    {
        for (int r = 0; r < H; ++r)
        {
            const double f = mask::kFMin * std::pow (fmaxCur / mask::kFMin, (double) r / (H - 1));
            tiltDb[(size_t) r] = (float) (view.tilt * std::log2 (f / 1000.0));
        }
    }
    void writePixelColumn (juce::Image::BitmapData& bd)
    {
        const float inv = 1.0f / std::max (1.0f, view.ceilDb - view.floorDb);
        const int x = wx;
        for (int r = 0; r < H; ++r)
        {
            const float db = mask::SpectrogramStore::dequant (pend[(size_t) r]) + tiltDb[(size_t) r];
            const float t = juce::jlimit (0.0f, 1.0f, (db - view.floorDb) * inv);
            const uint32_t c = (*lut)[(size_t) (t * 1023.0f)];
            auto* p = reinterpret_cast<uint32_t*> (bd.getPixelPointer (x, H - 1 - r));
            *p = c;
        }
        wx = (wx + 1) % W;
    }

    juce::Image img;
    View view {};
    double fmaxCur = 20000.0;
    uint64_t drawn = 0;
    int wx = 0;
    float acc = 0.0f;
    std::array<uint8_t, H> pend {};
    std::array<float, H> tiltDb {};
    const std::array<uint32_t, 1024>* lut = &look::paletteLut (0);
};
