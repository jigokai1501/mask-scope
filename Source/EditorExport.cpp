#include "PluginEditor.h"

using namespace mask;
using juce::Colour;
using juce::Justification;
using juce::Rectangle;

namespace
{
struct Check { int status; juce::String name, value, sub; };   // 0 ok · 1 à surveiller · 2 à corriger · 3 info
Colour statusColour (int s) { return s == 0 ? col::mint : (s == 1 ? col::amber : (s == 2 ? col::bad : col::blue)); }

const char* stateWord (int s)
{
    switch (s)
    {
        case PsKept: return "inchangé";
        case PsDown: return "baissé";
        case PsUp: return "remonté";
        case PsUpLimitedByPeak: return "remonté, limité par le pic";
        case PsPlatformLimiter: return "limiteur de la plateforme";
        default: return "pas de normalisation";
    }
}
Colour stateColour (int s)
{
    switch (s) { case PsKept: return col::mint; case PsDown: return col::blue; case PsUp: return col::mint; case PsUpLimitedByPeak: return col::amber; case PsPlatformLimiter: return col::bad; default: return col::dim; }
}
}

void ScopeEditor::drawExport (juce::Graphics& g, Rectangle<float> area)
{
    const Style& st = styleAt (ui.style);
    const bool valid = m.lufsI > -100.0f && m.measuredSec >= 3.0f;
    int np; const Platform* plats = platforms (np);
    const int cod = juce::jlimit (0, kCodecs - 1, ui.exCodec);
    const float os = a.codec[cod].valid ? a.codec[cod].overshootMaxDb : 0.0f;
    const float ceil = tpCeiling (ui.target, m.lufsI);
    const float T = spotifyLufs (ui.spot);

    // ----- bandeau supérieur
    auto top = area.removeFromTop (30.0f * u);
    {
        txt (g, look::U ("EXPORT · PRÉDICTION"), top.removeFromLeft (190.0f * u), 12.0f, col::mint, Justification::centredLeft, true);
        chip (g, top.removeFromLeft (150.0f * u), C_Target, RTargetBtn, nullptr, col::mint); top.removeFromLeft (6.0f * u);
        if (ui.target == TargetSpotify) { chip (g, top.removeFromLeft (190.0f * u), C_Spot, RExSpot); top.removeFromLeft (6.0f * u); }
        chip (g, top.removeFromLeft (200.0f * u), C_ExCodec, RExCodec);
        txt (g, look::fmtU ("mesure : %d:%02d  ·  %s", (int) m.measuredSec / 60, (int) m.measuredSec % 60, m.playing ? "lecture" : "arrêt"), top.removeFromRight (240.0f * u), 10.0f, col::dim, Justification::centredRight);
    }
    area.removeFromTop (6.0f * u);

    // ----- consigne si mesure trop courte
    const bool shortMeasure = m.measuredSec < 20.0f;
    if (shortMeasure)
    {
        auto b = area.removeFromBottom (40.0f * u);
        area.removeFromBottom (6.0f * u);
        g.setColour (col::amber.withAlpha (0.12f)); g.fillRoundedRectangle (b, 6.0f * u);
        g.setColour (col::amber); g.drawRoundedRectangle (b.reduced (0.5f), 6.0f * u, 1.0f);
        txt (g, juce::String::fromUTF8 ("Pour des chiffres fiables :  1) clique RAZ   2) lis le morceau du début à la fin (ou fais un bounce)   3) reviens ici. La sonie intégrée et le true peak sont exacts ; la simulation d'encodage s'affine avec la durée."), b.reduced (12.0f * u, 2.0f * u), 10.0f, col::amber, Justification::centredLeft);
        addHit (b, A_None, 0, RExVerdict);
    }

    const float gap = 8.0f * u;
    auto colA = area.removeFromLeft (area.getWidth() * 0.34f - gap * 0.5f);
    area.removeFromLeft (gap);
    auto colB = area.removeFromLeft (area.getWidth() * 0.55f - gap * 0.5f);
    area.removeFromLeft (gap);
    auto colC = area;

    auto card = [&] (Rectangle<float> rc, const juce::String& title, Colour c)
    {
        g.setColour (col::panel); g.fillRoundedRectangle (rc, 8.0f * u);
        g.setColour (col::line); g.drawRoundedRectangle (rc.reduced (0.5f), 8.0f * u, 1.0f);
        auto h = rc.removeFromTop (24.0f * u);
        g.setColour (c); g.fillRoundedRectangle (h.getX() + 8.0f * u, h.getCentreY() - 5.0f * u, 3.0f * u, 10.0f * u, 1.5f * u);
        txt (g, title, h.withTrimmedLeft (18.0f * u), 11.0f, c, Justification::centredLeft, true);
        return rc.reduced (12.0f * u, 6.0f * u);
    };

    // =============================================================== VERDICT
    {
        auto in = card (colA, "VERDICT", col::mint);
        addHit (colA, A_None, 0, RExVerdict);
        if (ui.target == TargetOff)
        {
            txt (g, look::U ("Choisis un support dans l'en-tête (SPOTIFY, CONCERT ou CD) pour obtenir une prédiction."), in, 12.0f, col::dim, Justification::topLeft);
        }
        else if (! valid)
        {
            txt (g, "Pas encore de mesure.", in.removeFromTop (22.0f * u), 15.0f, col::text, Justification::centredLeft, true);
            txt (g, look::U ("Lance la lecture du morceau : la sonie intégrée, le true peak et la dynamique se calculent au fil de l'eau."), in, 11.0f, col::dim, Justification::topLeft);
        }
        else if (ui.target == TargetSpotify)
        {
            const auto row = predictPlatform (plats[0], T, true, ui.spot, m.lufsI, m.tpMax, os);
            juce::String head;
            if (row.state == PsDown) head = look::fmtU ("Spotify va te baisser de %.1f dB", -row.gain);
            else if (row.state == PsUp) head = look::fmtU ("Spotify va te remonter de %.1f dB", row.gain);
            else if (row.state == PsUpLimitedByPeak) head = look::fmtU ("Spotify te remonte de %.1f dB seulement (pic)", row.gain);
            else if (row.state == PsPlatformLimiter) head = look::fmtU ("Spotify te remonte de %.1f dB avec son limiteur", row.gain);
            else head = "Spotify ne touche pas ton niveau";
            txt (g, head, in.removeFromTop (44.0f * u), 16.0f, row.state == PsDown && -row.gain > 4.0f ? col::amber : col::text, Justification::topLeft, true);
            // échelle de sonie
            auto sc = in.removeFromTop (46.0f * u);
            const float lo = -26.0f, hi = -2.0f;
            auto X = [&] (float v) { return sc.getX() + juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo)) * sc.getWidth(); };
            auto bar = sc.withTrimmedTop (16.0f * u).withHeight (10.0f * u);
            g.setColour (col::bg); g.fillRoundedRectangle (bar, 3.0f * u);
            g.setColour (col::violet.withAlpha (0.25f)); g.fillRect (Rectangle<float> (X (st.lufsLo), bar.getY(), X (st.lufsHi) - X (st.lufsLo), bar.getHeight()));
            g.setColour (col::mint); g.fillRect (X (T) - 1.5f, bar.getY() - 4.0f, 3.0f, bar.getHeight() + 8.0f);
            g.setColour (col::amber); g.fillRoundedRectangle (X (m.lufsI) - 4.0f * u, bar.getY() - 3.0f, 8.0f * u, bar.getHeight() + 6.0f, 3.0f * u);
            txt (g, "TOI " + juce::String (m.lufsI, 1), Rectangle<float> (X (m.lufsI) - 40.0f * u, sc.getY(), 80.0f * u, 13.0f * u), 9.0f, col::amber, Justification::centred, true);
            txt (g, "SPOTIFY " + juce::String ((int) T), Rectangle<float> (X (T) - 45.0f * u, bar.getBottom() + 6.0f * u, 90.0f * u, 13.0f * u), 9.0f, col::mint, Justification::centred, true);
            txt (g, st.name, Rectangle<float> (X (st.lufsLo), bar.getBottom() + 18.0f * u, X (st.lufsHi) - X (st.lufsLo), 12.0f * u), 7.5f, col::violet, Justification::centred);
            in.removeFromTop (10.0f * u);
            auto line = [&] (const juce::String& k, const juce::String& v, Colour c = col::text)
            {
                auto rw = in.removeFromTop (19.0f * u);
                txt (g, k, rw.removeFromLeft (rw.getWidth() * 0.46f), 10.0f, col::dim, Justification::centredLeft);
                txt (g, v, rw, 10.5f, c, Justification::centredLeft, true);
            };
            line (look::U ("Sonie intégrée"), look::fmtU ("%.1f → %.1f LUFS", m.lufsI, row.outLufs));
            line ("True peak", look::fmtU ("%+.1f → %+.1f dBTP", m.tpMax, row.outTp), row.outTp > 0 ? col::bad : col::text);
            line (look::U ("Après encodage (est.)"), look::fmtU ("%+.1f dBTP au décodage", m.tpMax + os), m.tpMax + os > 0 ? col::bad : (m.tpMax + os > ceil ? col::amber : col::mint));
            line (look::U ("PLR (dynamique de crête)"), look::fmtU ("%.1f dB (repère ≥ %.0f)", m.plr, st.plrMin), m.plr >= st.plrMin ? col::mint : col::amber);
            if (row.state == PsDown)
                line (look::U ("Niveau « gaspillé »"), look::fmtU ("%.1f dB de limiteur sans gain", -row.gain), -row.gain > 4.0f ? col::amber : col::text);
            if (row.state == PsPlatformLimiter) line ("Limiteur Spotify", look::fmtU ("jusqu'à %.1f dB sur les crêtes", row.limiterGr), col::bad);
            in.removeFromTop (6.0f * u);
            g.setColour (col::dim);
            g.setFont (juce::FontOptions (10.0f * u));
            juce::String adv;
            if (row.state == PsDown && -row.gain > 4.0f)
                adv = juce::String::fromUTF8 ("Tout ce qui dépasse -14 est retiré à la lecture : la loudness ne te rapporte rien ici, mais ses effets (transitoires écrasés) restent. 2 à 3 dB de limiteur en moins gardent le même volume perçu avec plus de punch.");
            else if (row.state == PsDown)
                adv = juce::String::fromUTF8 ("Écart raisonnable : tu gardes un master dense, Spotify rattrape la différence sans toucher au son.");
            else if (row.state == PsUp || row.state == PsUpLimitedByPeak)
                adv = juce::String::fromUTF8 ("Ton master est plus doux que la cible : sa dynamique est préservée. Spotify ne remonte que dans la limite de ton pic.");
            else adv = juce::String::fromUTF8 ("Tu es pile sur la cible : aucun traitement côté Spotify.");
            g.drawFittedText (adv, in.removeFromTop (74.0f * u).toNearestInt(), Justification::topLeft, 6, 0.9f);
        }
        else if (ui.target == TargetConcert)
        {
            const bool inWin = m.lufsI >= st.lufsLo - 1.0f && m.lufsI <= st.lufsHi + 1.0f;
            txt (g, "Playback de concert", in.removeFromTop (26.0f * u), 16.0f, col::text, Justification::topLeft, true);
            para (g, juce::String::fromUTF8 ("Pas de normalisation : la sono joue ton fichier tel quel. Repères pratiques (pas une norme), pour ton style."), in.removeFromTop (44.0f * u), 10.0f, col::dim, 3);
            auto line = [&] (const juce::String& k, const juce::String& v, Colour c = col::text)
            {
                auto rw = in.removeFromTop (20.0f * u);
                txt (g, k, rw.removeFromLeft (rw.getWidth() * 0.46f), 10.0f, col::dim, Justification::centredLeft);
                txt (g, v, rw, 10.5f, c, Justification::centredLeft, true);
            };
            line (look::U ("Sonie intégrée"), look::fmtU ("%.1f LUFS (repère %.0f … %.0f)", m.lufsI, st.lufsLo, st.lufsHi), inWin ? col::mint : col::amber);
            line ("True peak", look::fmtU ("%+.1f dBTP (plafond %.1f)", m.tpMax, ceil), m.tpMax <= ceil ? col::mint : col::amber);
            line ("PLR", look::fmtU ("%.1f dB (≥ 8 conseillé en salle)", m.plr), m.plr >= 8.0f ? col::mint : col::amber);
            line (look::U ("Énergie sous 60 Hz"), look::fmtU ("%.0f %% de l'énergie", a.bandShareSess[0] * 100.0f), a.bandShareSess[0] > 0.10f ? col::amber : col::mint);
            line ("Compat. mono", look::fmtU ("%.1f dB", m.monoFoldDb), m.monoFoldDb > -3.0f ? col::mint : col::amber);
            in.removeFromTop (8.0f * u);
            g.setColour (col::dim); g.setFont (juce::FontOptions (10.0f * u));
            g.drawFittedText (juce::String::fromUTF8 ("À haut niveau sonore, le sub et les 2,5–5 kHz fatiguent vite. Laisse de la marge à l'ingé son (pas de limiteur à fond) et vérifie que le grave est en mono : beaucoup de sonos sont en mono ou en subs séparés."), in.removeFromTop (74.0f * u).toNearestInt(), Justification::topLeft, 6, 0.9f);
        }
        else
        {
            const bool inWin = m.lufsI >= st.lufsLo - 1.0f && m.lufsI <= st.lufsHi + 1.0f;
            txt (g, look::U ("CD (16 bits · 44,1 kHz)"), in.removeFromTop (26.0f * u), 16.0f, col::text, Justification::topLeft, true);
            para (g, juce::String::fromUTF8 ("Aucune normalisation sur le disque : on entend ton niveau. Les lecteurs et rippeurs avec Sound Check ou ReplayGain peuvent en appliquer une."), in.removeFromTop (44.0f * u), 10.0f, col::dim, 3);
            auto line = [&] (const juce::String& k, const juce::String& v, Colour c = col::text)
            {
                auto rw = in.removeFromTop (20.0f * u);
                txt (g, k, rw.removeFromLeft (rw.getWidth() * 0.46f), 10.0f, col::dim, Justification::centredLeft);
                txt (g, v, rw, 10.5f, c, Justification::centredLeft, true);
            };
            line (look::U ("Sonie intégrée"), look::fmtU ("%.1f LUFS (repère %.0f … %.0f)", m.lufsI, st.lufsLo, st.lufsHi), inWin ? col::mint : col::amber);
            line ("True peak", look::fmtU ("%+.1f dBTP (plafond %.1f)", m.tpMax, ceil), m.tpMax <= ceil ? col::mint : (m.tpMax <= 0 ? col::amber : col::bad));
            line (look::U ("Écrêtages"), look::fmtU ("%u", m.clipCount), m.clipCount ? col::bad : col::mint);
            line ("PLR", look::fmtU ("%.1f dB", m.plr), m.plr >= st.plrMin ? col::mint : col::amber);
            in.removeFromTop (8.0f * u);
            g.setColour (col::dim); g.setFont (juce::FontOptions (10.0f * u));
            g.drawFittedText (juce::String::fromUTF8 ("Dernier maillon : conversion 24 → 16 bits avec dither (TPDF ou POW-r), une seule fois. Vérifie aussi les silences de début/fin et les espaces entre pistes."), in.removeFromTop (74.0f * u).toNearestInt(), Justification::topLeft, 6, 0.9f);
        }
        // ----- à faire avant l'export (conseils en cours, hors idées)
        if (valid && ui.target != TargetOff && in.getHeight() > 70.0f * u)
        {
            in.removeFromTop (6.0f * u);
            rule (g, in.getX(), in.getY(), in.getRight(), in.getY(), col::line);
            in.removeFromTop (6.0f * u);
            txt (g, "À FAIRE AVANT L'EXPORT", in.removeFromTop (16.0f * u), 9.5f, col::mint, Justification::centredLeft, true);
            int shown = 0;
            for (const auto& ad : advice)
            {
                if (ad.sev < 1 || in.getHeight() < 36.0f * u || shown >= 4) continue;
                auto row = in.removeFromTop (42.0f * u);
                const Colour sc = look::sevColour (ad.sev);
                g.setColour (sc);
                g.fillEllipse (row.getX(), row.getY() + 3.0f * u, 7.0f * u, 7.0f * u);
                auto tx = row.withTrimmedLeft (14.0f * u);
                txt (g, look::fromStd (ad.title), tx.removeFromTop (14.0f * u), 9.5f, col::text, Justification::centredLeft, true);
                g.setColour (col::dim); g.setFont (juce::FontOptions (8.8f * u));
                g.drawFittedText (look::fromStd (ad.text), tx.toNearestInt(), Justification::topLeft, 2, 0.9f);
                ++shown;
            }
            if (shown == 0) txt (g, "Rien d'urgent : continue à mesurer sur toute la durée du morceau.", in.removeFromTop (16.0f * u), 9.5f, col::dim, Justification::centredLeft);
        }
    }

    // =============================================================== PLATEFORMES + CODEC
    {
        auto tableArea = colB.removeFromTop (colB.getHeight() * 0.54f);
        colB.removeFromTop (gap);
        auto in = card (tableArea, "PLATEFORMES DE STREAMING", col::amber);
        addHit (tableArea, A_None, 0, RExTable);
        const float cw[6] = { 0.22f, 0.09f, 0.10f, 0.15f, 0.14f, 0.30f };
        const char* hd[6] = { "PLATEFORME", "CIBLE", "GAIN", "LUFS APRÈS", "PIC APRÈS", "ÉTAT" };
        auto hrow = in.removeFromTop (14.0f * u);
        for (int c = 0; c < 6; ++c) { auto cellR = hrow.removeFromLeft (in.getWidth() * cw[c]); if (c == 5) cellR.removeFromLeft (6.0f * u); txt (g, juce::String::fromUTF8 (hd[c]), cellR, 8.0f, col::faint, c == 0 || c == 5 ? Justification::centredLeft : Justification::centredRight, true); }
        const float rh = std::min (32.0f * u, (in.getHeight() - 32.0f * u) / (float) np);
        for (int i = 0; i < np; ++i)
        {
            auto row = in.removeFromTop (rh);
            const auto pr = predictPlatform (plats[i], T, i == 0, ui.spot, valid ? m.lufsI : -14.0f, valid ? m.tpMax : -1.0f, os);
            if (i % 2 == 0) { g.setColour (col::white.withAlpha (0.025f)); g.fillRect (row); }
            const Colour sc = valid ? stateColour (pr.state) : col::faint;
            auto cell = [&] (int c) { return row.removeFromLeft (in.getWidth() * cw[c]); };
            txt (g, i == 0 ? juce::String ("Spotify (") + spotModeName (ui.spot) + ")" : juce::String (plats[i].name), cell (0), 9.5f, col::text, Justification::centredLeft, true);
            txt (g, plats[i].normalizes ? juce::String ((int) pr.target) : juce::String (look::U ("—")), cell (1), 9.5f, col::dim, Justification::centredRight);
            txt (g, valid && plats[i].normalizes ? look::fmtU ("%+.1f", pr.gain) : juce::String (look::U ("—")), cell (2), 9.5f, sc, Justification::centredRight, true);
            txt (g, valid ? juce::String (pr.outLufs, 1) : juce::String (look::U ("—")), cell (3), 9.5f, col::text, Justification::centredRight);
            txt (g, valid ? look::fmtU ("%+.1f", pr.outTp) + (pr.clipRisk ? " !" : "") : juce::String (look::U ("—")), cell (4), 9.5f, pr.clipRisk ? col::bad : col::text, Justification::centredRight, pr.clipRisk);
            txt (g, valid ? juce::String::fromUTF8 (stateWord (pr.state)) : juce::String (look::U ("—")), row.withTrimmedLeft (6.0f * u), 8.5f, sc, Justification::centredLeft, true);
        }
        txt (g, juce::String::fromUTF8 ("« ! » : le pic décodé (pic + dépassement d'encodage estimé) dépasse 0 dBFS. Sources : pages officielles Spotify for Artists ; autres plateformes : valeurs publiques courantes, à vérifier."), in.removeFromTop (28.0f * u), 8.0f, col::faint, Justification::topLeft);
    }
    {
        auto in = card (colB, juce::String::fromUTF8 ("SIMULATION D'ENCODAGE (estimation)"), col::cyan);
        addHit (colB, A_None, 0, RExCodec);
        int nc; const CodecProfile* prof = codecProfiles (nc);
        static const float unc[3] = { 1.5f, 0.7f, 0.1f };
        const float cw[5] = { 0.22f, 0.20f, 0.22f, 0.20f, 0.16f };
        const char* hd[5] = { "CODEC", "DÉPASSEMENT", "PIC DÉCODÉ", "COUPURE", "ÉCART" };
        auto hrow = in.removeFromTop (14.0f * u);
        for (int c = 0; c < 5; ++c) txt (g, juce::String::fromUTF8 (hd[c]), hrow.removeFromLeft (in.getWidth() * cw[c]), 8.0f, col::faint, c == 0 ? Justification::centredLeft : Justification::centredRight, true);
        for (int i = 0; i < nc; ++i)
        {
            auto row = in.removeFromTop (24.0f * u);
            const bool sel = i == cod;
            if (sel) { g.setColour (col::cyan.withAlpha (0.08f)); g.fillRect (row); }
            const bool ok = a.codec[i].valid != 0;
            const float o = ok ? a.codec[i].overshootMaxDb : 0.0f;
            const float dec = m.tpMax + o;
            auto cell = [&] (int c) { return row.removeFromLeft (in.getWidth() * cw[c]); };
            txt (g, prof[i].name, cell (0), 9.5f, sel ? col::cyan : col::text, Justification::centredLeft, true);
            txt (g, ok ? look::fmtU ("%+.1f dB", o) : juce::String (look::U ("—")), cell (1), 9.5f, col::text, Justification::centredRight);
            txt (g, ok && valid ? look::fmtU ("%+.1f dBTP", dec) : juce::String (look::U ("—")), cell (2), 9.5f, ok && valid ? (dec > 0 ? col::bad : (dec > ceil ? col::amber : col::mint)) : col::faint, Justification::centredRight, true);
            txt (g, i == 2 ? juce::String ("> 20 kHz") : look::fmtU ("%.1f kHz", prof[i].cutoffHz / 1000.0f), cell (3), 9.5f, col::dim, Justification::centredRight);
            txt (g, juce::String::fromUTF8 ("± ") + juce::String (unc[i], 1), row, 9.5f, col::faint, Justification::centredRight);
        }
        in.removeFromTop (6.0f * u);
        g.setColour (col::dim); g.setFont (juce::FontOptions (9.5f * u));
        g.drawFittedText (juce::String::fromUTF8 ("Modèle de codec à perte calibré sur de vrais encodages Ogg Vorbis (ffmpeg/libvorbis) : il estime de combien les crêtes dépassent après encodage. Le vrai résultat dépend du contenu ; la ligne « écart » donne la dispersion observée. Pour un contrôle définitif, exporte et encode ton fichier réellement. Aigus 9–20 kHz : ")
                          + juce::String (a.bandShareSess[7] * 100.0f, 1) + juce::String::fromUTF8 (" % de l'énergie (coupés à 96 kbps au-delà de 16,7 kHz)."), in.toNearestInt(), Justification::topLeft, 7, 0.9f);
    }

    // =============================================================== CHECK-LIST
    {
        std::vector<Check> cl;
        const float plrDelta = m.plr - st.plrMin;
        cl.push_back ({ m.measuredSec >= 60.0f ? 0 : (m.measuredSec >= 15.0f ? 1 : 3), look::U ("Mesure complète"), look::fmtU ("%d:%02d", (int) m.measuredSec / 60, (int) m.measuredSec % 60), juce::String::fromUTF8 ("Joue le morceau en entier après RAZ.") });
        if (valid)
        {
            cl.push_back ({ m.tpMax <= ceil ? 0 : (m.tpMax <= 0 ? 1 : 2), "True peak", look::fmtU ("%+.1f dBTP", m.tpMax), look::fmtU ("plafond conseillé %s : %.1f dBTP", targetName (ui.target), ceil) });
            cl.push_back ({ m.clipCount == 0 ? 0 : 2, juce::String::fromUTF8 ("Écrêtages"), juce::String ((int) m.clipCount), juce::String::fromUTF8 ("échantillons à 0 dBFS") });
            if (ui.target == TargetSpotify)
            {
                const float ex = m.lufsI - T;
                cl.push_back ({ ex <= 1.5f ? 0 : (ex <= 4.0f ? 1 : 2), juce::String::fromUTF8 ("Écart à la cible Spotify"), look::fmtU ("%+.1f LU", ex), juce::String::fromUTF8 ("au-delà de la cible, aucun gain de volume") });
                cl.push_back ({ m.tpMax + a.codec[0].overshootMaxDb <= 0 ? 0 : 2, juce::String::fromUTF8 ("Marge d'encodage (OGG 96)"), look::fmtU ("%+.1f dBTP décodé", m.tpMax + a.codec[0].overshootMaxDb), juce::String::fromUTF8 ("estimation, voir simulation") });
            }
            else
            {
                const bool inWin = m.lufsI >= st.lufsLo - 1.0f && m.lufsI <= st.lufsHi + 1.0f;
                cl.push_back ({ inWin ? 0 : 1, "Sonie vs style", look::fmtU ("%.1f LUFS", m.lufsI), look::fmtU ("repère %s : %.0f … %.0f", st.name, st.lufsLo, st.lufsHi) });
            }
            cl.push_back ({ plrDelta >= 0 ? 0 : (plrDelta >= -1.5f ? 1 : 2), "PLR", look::fmtU ("%.1f dB", m.plr), look::fmtU ("repère %s : au moins %.0f dB", st.name, st.plrMin) });
            cl.push_back ({ m.monoFoldDb >= -3.0f ? 0 : (m.monoFoldDb >= -6.0f ? 1 : 2), "Compat. mono", look::fmtU ("%.1f dB", m.monoFoldDb), juce::String::fromUTF8 ("perte quand G + D sont additionnés") });
            cl.push_back ({ (a.bandCorr[0] >= 0.75f && a.bandCorr[1] >= 0.75f) ? 0 : 1, juce::String::fromUTF8 ("Graves en mono"), look::fmtU ("%.2f / %.2f", a.bandCorr[0], a.bandCorr[1]), juce::String::fromUTF8 ("corrélation sub / basses") });
            cl.push_back ({ m.dcDb < -60.0f ? 0 : 1, juce::String::fromUTF8 ("Décalage DC"), look::fmtDb (m.dcDb, 0) + " dBFS", juce::String() });
            if (ui.target == TargetConcert) cl.push_back ({ a.bandShareSess[0] <= 0.10f ? 0 : 1, "Sub pour la sono", look::fmtU ("%.0f %%", a.bandShareSess[0] * 100.0f), juce::String::fromUTF8 ("part de l'énergie sous 60 Hz") });
            if (ui.target == TargetCd) cl.push_back ({ 3, "Dither 16 bits", juce::String::fromUTF8 ("à faire"), juce::String::fromUTF8 ("dernier maillon, une seule fois") });
        }
        auto in = card (colC, juce::String::fromUTF8 ("CHECK-LIST"), col::blue);
        addHit (colC, A_None, 0, RExChecks);
        const float rh = std::min (46.0f * u, in.getHeight() / (float) std::max<size_t> (cl.size(), 1));
        for (const auto& c : cl)
        {
            auto row = in.removeFromTop (rh);
            const Colour sc = statusColour (c.status);
            g.setColour (sc);
            g.fillEllipse (row.getX(), row.getY() + 6.0f * u, 8.0f * u, 8.0f * u);
            auto tx = row.withTrimmedLeft (16.0f * u);
            auto l1 = tx.removeFromTop (16.0f * u);
            txt (g, c.name, l1.removeFromLeft (l1.getWidth() * 0.52f), 10.0f, col::text, Justification::centredLeft, true);
            txt (g, c.value, l1, 10.0f, sc, Justification::centredRight, true);
            txt (g, c.sub, tx.removeFromTop (13.0f * u), 8.5f, col::faint, Justification::centredLeft);
            rule (g, row.getX(), row.getBottom() - 1.0f, row.getRight(), row.getBottom() - 1.0f, col::line.withAlpha (0.6f));
        }
    }
}
