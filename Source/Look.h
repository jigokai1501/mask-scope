// mask::scope — couleurs, palettes de spectrogramme, petits outils de dessin
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdarg>
#include <cstdio>
#include "Dsp/Analysis.h"

namespace col
{
using C = juce::Colour;
inline const C bg        { 0xff08090c };
inline const C panel     { 0xff12141a };
inline const C panel2    { 0xff1a1d25 };
inline const C line      { 0xff272b36 };
inline const C text      { 0xffe8eaf0 };
inline const C dim       { 0xff9aa0b0 };
inline const C faint     { 0xff5d6475 };
inline const C white     { 0xfff4f6fb };     // « mask:: »
inline const C mint      { 0xff35f0b2 };     // dominante de mask::scope
inline const C amber     { 0xffffb020 };
inline const C pink      { 0xffff4fb8 };
inline const C violet    { 0xffa97bff };
inline const C cyan      { 0xff19e3ff };
inline const C blue      { 0xff4d8dff };
inline const C yellow    { 0xffffe04a };
inline const C bad       { 0xffff5a4d };
inline const C acid      { 0xffb6ff00 };
}

namespace look
{
struct Stop { float t; uint32_t rgb; };

inline const std::vector<std::vector<Stop>>& paletteDefs()
{
    static const std::vector<std::vector<Stop>> p = {
        { { 0.00f, 0x05060c }, { 0.15f, 0x14104a }, { 0.32f, 0x4a1a8c }, { 0.50f, 0xb0278c }, { 0.68f, 0xff6a3d }, { 0.85f, 0xffd23f }, { 1.00f, 0xfffbe6 } },
        { { 0.00f, 0x05070d }, { 0.25f, 0x0b2a5b }, { 0.50f, 0x1f7fd1 }, { 0.75f, 0x62e6ff }, { 1.00f, 0xffffff } },
        { { 0.00f, 0x04090a }, { 0.30f, 0x0b3b33 }, { 0.60f, 0x1fbf8f }, { 0.85f, 0x7cffd4 }, { 1.00f, 0xffffff } },
        { { 0.00f, 0x000000 }, { 1.00f, 0xffffff } } };
    return p;
}
inline const char* paletteName (int i) { static const char* n[] = { "MASK", "ICE", "MINT", "GRAY" }; return n[juce::jlimit (0, 3, i)]; }

// 1024 couleurs ARGB opaques
inline const std::array<uint32_t, 1024>& paletteLut (int idx)
{
    static std::array<uint32_t, 1024> luts[4];
    static bool built = false;
    if (! built)
    {
        const auto& defs = paletteDefs();
        for (int p = 0; p < 4; ++p)
            for (int i = 0; i < 1024; ++i)
            {
                const float t = (float) i / 1023.0f;
                size_t k = 0;
                while (k + 2 < defs[(size_t) p].size() && t > defs[(size_t) p][k + 1].t) ++k;
                const auto& a = defs[(size_t) p][k]; const auto& b = defs[(size_t) p][k + 1];
                const float u = juce::jlimit (0.0f, 1.0f, (t - a.t) / (b.t - a.t));
                auto ch = [&] (int sh) { return (uint32_t) std::lround (((a.rgb >> sh) & 255) * (1 - u) + ((b.rgb >> sh) & 255) * u); };
                luts[p][(size_t) i] = 0xff000000u | (ch (16) << 16) | (ch (8) << 8) | ch (0);
            }
        built = true;
    }
    return luts[juce::jlimit (0, 3, idx)];
}

inline juce::Colour sevColour (int sev)
{
    switch (sev) { case 3: return col::bad; case 2: return col::amber; case 1: return col::blue; default: return col::dim; }
}
inline juce::Colour modeColour (int mode)
{
    switch (mode) { case mask::ModeMid: return col::mint; case mask::ModeSide: return col::violet; case mask::ModeL: return col::cyan; case mask::ModeR: return col::pink; default: return col::white; }
}
inline const char* modeName (int mode) { static const char* n[] = { "ST", "MID", "SIDE", "L", "R" }; return n[juce::jlimit (0, 4, mode)]; }

inline juce::String fmtDb (float v, int dec = 1)
{
    if (v < -119.0f) return juce::String ("-inf");
    return dec == 0 ? juce::String ((int) std::lround (v)) : juce::String (v, dec);
}
inline juce::String fromStd (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
inline juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
inline juce::String fmtU (const char* f, ...)
{
    char buf[512];
    va_list ap; va_start (ap, f);
    std::vsnprintf (buf, sizeof (buf), f, ap);
    va_end (ap);
    return juce::String::fromUTF8 (buf);
}

inline float freqToX (float f, float fmin, float fmax) { return std::log (f / fmin) / std::log (fmax / fmin); }
inline float xToFreq (float x01, float fmin, float fmax) { return fmin * std::pow (fmax / fmin, x01); }
} // namespace look
