// mask::scope — supports de diffusion (Spotify, Concert, CD), plateformes de streaming, profils de style, prédictions d'export
#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>
#include "Analysis.h"

namespace mask
{
inline std::string fmt (const char* f, ...)
{
    char buf[1024];
    va_list a; va_start (a, f);
    std::vsnprintf (buf, sizeof (buf), f, a);
    va_end (a);
    return buf;
}

enum Target { TargetOff = 0, TargetSpotify, TargetConcert, TargetCd };
enum SpotMode { SpotNormal = 0, SpotLoud, SpotQuiet };
enum Tab { TabOverview = 0, TabSpectrogram, TabSpectrum, TabSoundField, TabLoudness, TabHistory, TabLevels, TabExport, kTabs };

inline const char* targetName (int t) { static const char* n[] = { "OFF", "SPOTIFY", "CONCERT", "CD" }; return n[clampi (t, 0, 3)]; }
inline const char* spotModeName (int m) { static const char* n[] = { "NORMAL -14", "LOUD -11", "QUIET -19" }; return n[clampi (m, 0, 2)]; }
inline float spotifyLufs (int m) { static const float v[] = { -14.0f, -11.0f, -19.0f }; return v[clampi (m, 0, 2)]; }

// Plafond de true peak conseillé selon le support
inline float tpCeiling (int target, float lufsI)
{
    switch (target)
    {
        case TargetSpotify: return lufsI > -14.0f ? -2.0f : -1.0f;   // Spotify for Artists : -2 dBTP si > -14 LUFS, sinon -1
        case TargetCd:      return -0.3f;                            // usage de mastering CD (marge pour les DAC à suréchantillonnage)
        default:            return -1.0f;
    }
}

// ---------- plateformes de streaming ----------
struct Platform
{
    const char* name;
    float lufs;
    int boost;          // 0 = ne fait que baisser ; 1 = remonte dans la limite du pic ; 2 = remonte avec limiteur (Spotify Loud)
    int normalizes;
    const char* note;
};
inline const Platform* platforms (int& n)
{
    static const Platform p[] = {
        { "Spotify",      -14.0f, 1, 1, "Normal -14 / Loud -11 / Quiet -19 ; ne s'applique pas au lecteur web" },
        { "Apple Music",  -16.0f, 1, 1, "Sound Check (activable) ; remonte les morceaux faibles sans limiteur" },
        { "YouTube",      -14.0f, 0, 1, "baisse seulement les morceaux forts" },
        { "Tidal",        -14.0f, 0, 1, "normalisation d'album ; baisse seulement (selon les sources)" },
        { "Amazon Music", -14.0f, 0, 1, "baisse seulement ; certaines sources conseillent -2 dBTP" },
        { "Deezer",       -15.0f, 0, 1, "toujours active, par morceau" },
        { "SoundCloud",     0.0f, 0, 0, "pas de normalisation (à vérifier)" },
    };
    n = 7;
    return p;
}

enum PState { PsKept = 0, PsDown, PsUp, PsUpLimitedByPeak, PsPlatformLimiter, PsNone };
struct PlatformRow
{
    const Platform* plat = nullptr;
    float target = 0, gain = 0, outLufs = 0, outTp = 0, outTpCodec = 0, limiterGr = 0;
    int state = PsKept;
    bool clipRisk = false;
};

inline PlatformRow predictPlatform (const Platform& p, float spotifyTarget, bool isSpotify, int spotMode, float lufsI, float tpMax, float codecOvershoot)
{
    PlatformRow r;
    r.plat = &p;
    r.target = isSpotify ? spotifyTarget : p.lufs;
    r.outLufs = lufsI; r.outTp = tpMax;
    if (! p.normalizes) { r.state = PsNone; }
    else
    {
        float g = r.target - lufsI;
        if (g < -0.25f) { r.state = PsDown; }
        else if (g > 0.25f)
        {
            if (p.boost == 0) { g = 0.0f; r.state = PsKept; }
            else if (isSpotify && spotMode == SpotLoud)
            {
                // Spotify Loud : gain complet + limiteur à -1 dB (échantillons), attaque 5 ms, release 100 ms
                r.state = PsUp;
                const float peakAfter = tpMax + g;
                if (peakAfter > -1.0f) { r.limiterGr = peakAfter + 1.0f; r.state = PsPlatformLimiter; }
            }
            else
            {
                const float maxG = -1.0f - tpMax;          // on garde 1 dB de marge pour l'encodage
                if (g > maxG) { g = std::max (0.0f, maxG); r.state = PsUpLimitedByPeak; }
                else r.state = PsUp;
            }
        }
        else { g = 0.0f; r.state = PsKept; }
        r.gain = g;
        r.outLufs = lufsI + g;
        r.outTp = tpMax + g - (r.state == PsPlatformLimiter ? r.limiterGr : 0.0f);
    }
    r.outTpCodec = r.outTp + codecOvershoot;
    r.clipRisk = r.outTpCodec > 0.0f || (tpMax + codecOvershoot) > 0.0f;   // au décodage (avant gain) ou après gain
    return r;
}

// ---------- profils de style ----------
struct Style
{
    const char* name;
    float lufsLo, lufsHi;     // fenêtre de sonie intégrée habituelle des masters du genre (repère, pas une règle)
    float plrMin;
    float slope;              // pente de référence du spectre (dB/octave, densité spectrale)
    float hi[kBands], lo[kBands];   // écarts tolérés au-dessus / en dessous de la référence (dB)
    const char* blurb;
};
inline const Style* styles (int& n)
{
    static const Style s[] = {
        { "PUNK POP / EMO", -10.0f, -7.0f, 7.5f, 4.5f,
          { 6, 6, 4, 5, 5, 5, 6, 7 }, { -14, -9, -8, -6, -8, -8, -9, -12 },
          "guitares saturées doublées L/R, voix devant, caisse claire qui claque" },
        { "HYPERPOP", -9.0f, -5.0f, 6.0f, 4.0f,
          { 9, 8, 4, 5, 5, 6, 7, 9 }, { -12, -8, -8, -7, -8, -8, -8, -9 },
          "808/sub saturés, voix très brillante et très présente, écrêtage assumé" },
        { "ALT / INDIE", -13.0f, -9.0f, 9.0f, 4.5f,
          { 5, 6, 4, 5, 5, 5, 6, 7 }, { -15, -9, -8, -6, -8, -8, -10, -13 },
          "plus de dynamique, grain plus naturel" },
        { "TECHNO", -10.0f, -6.0f, 7.0f, 4.5f,
          { 9, 8, 4, 5, 5, 5, 6, 8 }, { -10, -7, -9, -8, -9, -9, -10, -13 },
          "kick et basse au centre, sidechain, hats brillants" },
        { "MIX PERSO", -10.0f, -6.0f, 7.0f, 4.2f,
          { 8, 7, 4, 5, 5, 5, 7, 8 }, { -13, -8, -8, -7, -8, -8, -9, -10 },
          "punk pop / emocore / hyperpop / alt avec touches techno" },
    };
    n = 5;
    return s;
}
inline const Style& styleAt (int i) { int n; return styles (n)[clampi (i, 0, 4)]; }

inline double bandCenter (int b) { return std::sqrt (bandEdges()[b] * bandEdges()[b + 1]); }

// Écart de chaque bande par rapport à une pente de référence (le niveau global est recalé sur la médiane des médiums)
inline void bandDeviation (const float* bandDb, const Style& st, float* dev)
{
    float rel[kBands];
    for (int b = 0; b < kBands; ++b)
        rel[b] = bandDb[b] + (float) (st.slope * std::log2 (bandCenter (b) / 1000.0));
    float mid[5];
    for (int i = 0; i < 5; ++i) mid[i] = rel[2 + i];
    std::sort (mid, mid + 5);
    const float off = mid[2];
    for (int b = 0; b < kBands; ++b) dev[b] = rel[b] - off;
}

} // namespace mask
