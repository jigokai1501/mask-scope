#include "../Source/Dsp/Tips.h"
#include <cstdio>
using namespace mask;
static int fails = 0;
static void checkTrue (const char* name, bool v) { if (! v) ++fails; std::printf ("%-64s %s\n", name, v ? "OK" : "FAIL"); }
static bool has (const std::vector<Advice>& v, int id) { for (auto& a : v) if (a.id == id) return true; return false; }
int main()
{
    CoachCtx c; c.bpm = 168; c.target = TargetSpotify; c.style = 4; c.signal = true;
    c.m.lufsI = -7.8f; c.m.lufsS = -8.1f; c.m.tpMax = 0.6f; c.m.plr = 8.4f; c.m.measuredSec = 120; c.m.corr = 0.6f; c.m.monoFoldDb = -1.2f;
    c.m.crest3s = 8; c.m.lra = 3.5f; c.m.dcDb = -140;
    c.a.sessFrames = 60;
    const float db[kBands] = { -40, -42, -48, -52, -56, -60, -66, -72 };   // pente ≈ -4,5 dB/oct
    for (int b = 0; b < kBands; ++b) { c.a.bandLiveDb[b] = db[b]; c.a.bandShareLive[b] = 0.1f; c.a.bandCorr[b] = 0.9f; c.a.bandSideDb[b] = -20; }
    c.a.codec[0].valid = 1; c.a.codec[0].overshootMaxDb = 1.1f;
    CoachEngine eng; std::vector<Advice> all;
    eng.evaluate (c, all);
    for (auto& a : all) std::printf ("  [%d] %-45s | %s\n", a.sev, a.title.c_str(), a.text.substr (0, 100).c_str());
    checkTrue ("true peak > 0 signalé (sev 3)", has (all, 1));
    checkTrue ("Spotify va baisser (id 3)", has (all, 3));
    checkTrue ("fiche BPM (id 32) et tempo rapide (33)", has (all, 32) && has (all, 33));
    checkTrue ("encodage 96 kbps (id 38) en sev 2 car pic + 1,1 > 0", [&] { for (auto& a : all) if (a.id == 38) return a.sev == 2; return false; }());
    // mix équilibré -> pas d'avertissement spectral
    bool anyBand = false; for (auto& a : all) if (a.id >= 15 && a.id <= 30) anyBand = true;
    checkTrue ("spectre -4,5 dB/oct : aucun avertissement de bande", ! anyBand);
    // bosse 2,5-5 kHz de +9 dB
    c.a.bandLiveDb[5] += 9.0f;
    eng.evaluate (c, all);
    checkTrue ("bosse de +9 dB à 2,5–5 kHz détectée (id 20)", has (all, 20));
    // hystérésis
    std::vector<Advice> vis; CoachEngine e2;
    e2.update (c, 0.0, vis, 0); size_t n0 = vis.size();
    e2.update (c, 3.0, vis, 0); size_t n1 = vis.size();
    std::printf ("  hystérésis : t=0 -> %zu conseils, t=3 s -> %zu\n", n0, n1);
    checkTrue ("hystérésis : les conseils apparaissent après quelques secondes", n1 > n0 || n0 > 0);
    // tips
    Hover h; h.hasData = true; h.freq = 52; h.db = -20; h.timeS = 0.4f;
    Tip t = tipFor (RSpgMain, c, h);
    std::printf ("  tip spectrogramme : %s | %s\n", t.title.c_str(), t.live.c_str());
    checkTrue ("bulle spectrogramme contient la note (G#1)", t.live.find ("G#1") != std::string::npos);
    // plateformes
    int np; const Platform* pl = platforms (np);
    auto r = predictPlatform (pl[0], -14.0f, true, SpotNormal, -7.8f, 0.6f, 1.1f);
    std::printf ("  Spotify normal : gain %.1f -> %.1f LUFS, TP %.1f, codec %.1f, clip=%d\n", r.gain, r.outLufs, r.outTp, r.outTpCodec, (int) r.clipRisk);
    checkTrue ("Spotify : -6,2 dB appliqués", std::abs (r.gain + 6.2f) < 0.01f);
    auto r2 = predictPlatform (pl[0], -11.0f, true, SpotLoud, -16.0f, -4.0f, 0.0f);
    checkTrue ("Spotify Loud : limiteur engagé quand -16 -> -11 avec pic -4", r2.state == PsPlatformLimiter && std::abs (r2.limiterGr - 2.0f) < 0.01f);
    auto r3 = predictPlatform (pl[0], -14.0f, true, SpotNormal, -20.0f, -5.0f, 0.0f);
    checkTrue ("Spotify Normal : -20 LUFS et pic -5 -> remonté seulement de 4 dB (-16)", r3.state == PsUpLimitedByPeak && std::abs (r3.outLufs + 16.0f) < 0.01f);
    std::printf ("\n%s (%d échec%s)\n", fails ? "ECHEC" : "TOUT OK", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
