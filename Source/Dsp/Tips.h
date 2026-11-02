// mask::scope — bulles d'astuces au survol (contenu en direct : valeurs mesurées, BPM, style, support)
#pragma once
#include "Coach.h"

namespace mask
{
enum Region
{
    RNone = 0, RTabs, RTipsBtn, RTargetBtn, RStyleBtn, RBpm, RReset, RHold, RSettings, RModuleExpand, RCoach,
    RSpgMain, RSpgBars, RSpgEnv, RSpgHit, RSpgCtl,
    RSpectrum, RSpBands, RSpSub, RSpCtl,
    RScope, RCorr, RBalance, RWidthBands, RMonoFold,
    RLdM, RLdS, RLdI, RLdLra, RLdTp, RLdPlr, RLdPsr, RLdHist, RLdBar,
    RHistory, RLvPeak, RLvRms, RLvCrest, RLvDc, RLvClip,
    RExVerdict, RExTable, RExCodec, RExChecks, RExSpot
};

struct Hover
{
    float freq = 0, db = -140, timeS = 0;   // sous la souris (spectre / spectrogramme)
    float x01 = 0, y01 = 0;
    int band = -1, hitSeg = -1;             // segment ADSR survolé : 0 A, 1 D, 2 S, 3 R
    bool hasData = false;
};

struct Tip { std::string title, body, live; };

inline void freqZone (float f, int style, std::string& title, std::string& text)
{
    const bool hyper = style == 1 || style == 4, punk = style == 0 || style == 4, tech = style == 3 || style == 4;
    if (f < 40.0f)       { title = "Sub-grave"; text = "Peu audible, mais mange du headroom. Passe-haut vers 30 Hz sur tout sauf kick/808. En salle, les résonances de la pièce amplifient cette zone."; }
    else if (f < 60.0f)  { title = "Poussée du kick / 808"; text = std::string ("Fondamentale des kicks graves et des 808 (A1 = 55 Hz). Accorde-la sur la tonalité du morceau.") + (hyper ? " En hyperpop, sature l'808 pour que ses harmoniques s'entendent sur téléphone." : ""); }
    else if (f < 100.0f) { title = "Corps du kick et de la basse"; text = std::string ("Zone où kick et basse se disputent la place : sépare-les (kick vers 60 Hz, basse vers 90–100 Hz) ou sidechain.") + (tech ? " En techno : un seul des deux domine le bas." : ""); }
    else if (f < 200.0f) { title = "Punch de la basse"; text = std::string ("Rondeur de la basse et bas de la caisse claire/toms.") + (punk ? " En punk pop, la basse picking vit ici et au-dessus (150–800 Hz)." : ""); }
    else if (f < 400.0f) { title = "Zone boueuse"; text = "Accumulation de guitares, synthés et voix : creuse 2–3 dB vers 250 Hz si c'est flou. Le corps de la caisse claire est vers 200 Hz."; }
    else if (f < 800.0f) { title = "Corps des médiums"; text = "Corps des voix et des guitares. Trop de 500–800 Hz donne un son nasal ou « boîte »."; }
    else if (f < 1600.0f){ title = "Médiums hauts"; text = "Zone qui peut sonner nasale ou « cornet ». Les guitares empilées s'y marchent dessus : EQ dynamique déclenché par la voix."; }
    else if (f < 3000.0f){ title = "Présence / intelligibilité"; text = "Compréhension des paroles. Les guitares saturées mordent ici : laisse de la place à la voix lead."; }
    else if (f < 5000.0f){ title = "Mordant et agressivité"; text = "Attaque des guitares, voix criée, caisse claire. Trop de 3–5 kHz = fatigue d'écoute ; manque = son mou."; }
    else if (f < 9000.0f){ title = "Sibilances et brillance"; text = "Les « S » et « CH » de la voix (6–8 kHz), hi-hats, cymbales. Un de-esser ici avant tout boost d'aigus."; }
    else if (f < 14000.0f){ title = "Brillance haute"; text = std::string ("Gloss de la voix et des cymbales.") + (hyper ? " Shelf +1,5–2 dB vers 10–12 kHz après compression pour la voix hyperpop." : "") + " Les encodages ≈ 96 kbps coupent vers 16–17 kHz."; }
    else                 { title = "Air"; text = "Donne l'ouverture, peu audible seul. Ogg à 96 kbps le supprime ; les écoutes en casque le révèlent."; }
}

inline const char* levelWord (float db)
{
    if (db > -12.0f) return "très fort";
    if (db > -30.0f) return "fort";
    if (db > -50.0f) return "moyen";
    if (db > -70.0f) return "faible";
    return "très faible";
}

inline Tip tipFor (int region, const CoachCtx& c, const Hover& h)
{
    Tip t;
    const MeterBlock& m = c.m;
    const AnalysisBlock& a = c.a;
    const Style& st = styleAt (c.style);
    const bool hb = c.bpm > 30.0;
    double q = 0, e = 0, s = 0, ed = 0;
    if (hb) bpmNumbers (c.bpm, q, e, s, ed);
    const float T = c.target == TargetSpotify ? spotifyLufs (c.spot) : 0.0f;
    switch (region)
    {
        case RTipsBtn:
            t.title = "Astuces"; t.body = "Active ou coupe les bulles. Quand c'est actif, reste une demi-seconde sur une zone pour qu'une bulle apparaisse : explication, lecture en direct et conseil adapté à ton style et au tempo.";
            break;
        case RTargetBtn:
            t.title = "Support cible"; t.body = "SPOTIFY : normalisation en LUFS, plafond de true peak, encodage Ogg. CONCERT : repères pour la sono (sub, mono, dynamique). CD : 16 bits, plafond -0,3 dBTP, pas de normalisation. OFF : aucun repère.";
            t.live = fmt ("Support : %s%s.", targetName (c.target), c.target == TargetSpotify ? (std::string (" (écoute ") + spotModeName (c.spot) + ")").c_str() : "");
            break;
        case RStyleBtn:
            t.title = "Style"; t.body = "Règle les repères de spectre, de dynamique et les idées proposées. Ce sont des repères indicatifs, pas des règles : l'oreille décide.";
            t.live = fmt ("%s — %s. Sonie habituelle des masters du genre : %.0f à %.0f LUFS, PLR conseillé ≥ %.0f dB.", st.name, st.blurb, st.lufsLo, st.lufsHi, st.plrMin);
            break;
        case RBpm:
            t.title = "Tempo (BPM)"; t.body = c.bpmFromHost ? "Lu depuis Logic. Double-clic pour le fixer à la main." : "Réglé à la main. Clique sur AUTO pour suivre Logic.";
            if (hb) t.live = fmt ("1/4 = %.0f ms · 1/8 = %.0f · 1/16 = %.0f · 1/8 pointé = %.0f · mesure = %.0f ms.", q, e, s, ed, q * c.sigNum);
            break;
        case RReset:
            t.title = "Remise à zéro"; t.body = "Efface la sonie intégrée, la LRA, les pics et l'historique. Fait automatiquement au démarrage de la lecture (réglage). Pour prédire l'export : remets à zéro, joue le morceau en entier.";
            break;
        case RHold:
            t.title = "Gel"; t.body = "Fige le spectrogramme et l'enveloppe pour les examiner. Passe la souris dessus pour lire fréquence, niveau et instant."; break;
        case RSettings: t.title = "Réglages"; t.body = "Remise à zéro auto au départ de la lecture, seuil de détection des coups, palette du spectrogramme, délai des bulles."; break;
        case RModuleExpand: t.title = "Agrandir"; t.body = "Clique sur le titre ou double-clique le module pour l'afficher en grand avec toutes les informations complémentaires. Échap pour revenir à la vue d'ensemble."; break;
        case RCoach:
            t.title = "Conseils en direct"; t.body = "Calculés à partir de ce qui passe en ce moment (niveaux, spectre, phase, coups) et de ton support. Un conseil apparaît quand le problème dure plus de quelques secondes. Clique pour ouvrir le module concerné.";
            break;
        case RSpgMain:
        {
            t.title = "Spectrogramme"; t.body = "Axe horizontal : le temps (le plus récent à droite). Axe vertical : la fréquence (20 Hz à 20 kHz, échelle log). Couleur : l'énergie. Une ligne horizontale = une note tenue ; une ligne verticale = un coup (transitoire).";
            if (h.hasData)
            {
                std::string zt, zx; freqZone (h.freq, c.style, zt, zx);
                int cents = 0; const int mid = noteFromHz (h.freq, cents);
                t.live = fmt ("%.0f Hz (%s%d, %+d cents) · %.0f dB (%s) · il y a %.2f s.\n%s : %s", h.freq, midiName (mid), midiOctave (mid), cents, h.db, levelWord (h.db), h.timeS, zt.c_str(), zx.c_str());
            }
            break;
        }
        case RSpgBars:
        {
            t.title = "Spectre moyen (barres)"; t.body = "Niveau par fréquence, aligné sur l'axe vertical du spectrogramme. Le trait fin est la crête récente. Cherche le « trou » ou la « bosse » en regardant la ligne de couleur à gauche.";
            if (h.hasData)
            {
                std::string zt, zx; freqZone (h.freq, c.style, zt, zx);
                t.live = fmt ("%.0f Hz : %.1f dB.\n%s : %s", h.freq, h.db, zt.c_str(), zx.c_str());
            }
            break;
        }
        case RSpgEnv:
            t.title = "Enveloppe (forme d'onde)"; t.body = "Forme d'onde et enveloppe de crête au même temps que le spectrogramme. Les bandes colorées marquent A (attaque), D (decay), S (sustain) et R (release) de chaque coup détecté.";
            if (a.haveHit && hb) t.live = fmt ("Dernier coup : A %.0f ms, D %.0f, S %.0f, R %.0f. Un temps = %.0f ms.", a.lastHit.aMs, a.lastHit.dMs, a.lastHit.sMs, a.lastHit.rMs, q);
            break;
        case RSpgHit:
        {
            t.title = "Analyse ADSR du dernier coup";
            t.body = "A : du pied (10 % de l'amplitude) à la crête. D : chute initiale. S : plateau ou queue lente. R : extinction finale. T20/T40/T60 : temps pour perdre 20/40/60 dB depuis la crête.";
            if (a.haveHit)
            {
                const HitSummary& hs = a.lastHit;
                if (h.hitSeg == 0) t.live = fmt ("Attaque : %.0f ms%s. %s", hs.aMs, hb ? (std::string (" (≈ ") + noteName (hs.aMs, c.bpm) + ")").c_str() : "", hs.aMs <= 3 ? "Clic très sec : bon pour un kick punchy." : (hs.aMs >= 25 ? "Attaque molle : ajoute un clic (3–5 kHz) ou un transient shaper." : "Attaque équilibrée."));
                else if (h.hitSeg == 1) t.live = fmt ("Decay : %.0f ms, pente %.0f dB/s.", hs.dMs, hs.slopeD);
                else if (h.hitSeg == 2) t.live = fmt ("Sustain : %.0f ms à %.1f dB sous la crête (pente %.0f dB/s).", hs.sMs, hs.sustainDb, hs.slopeS);
                else if (h.hitSeg == 3) t.live = fmt ("Release : %.0f ms%s, pente %.0f dB/s.%s", hs.rMs, hb ? (std::string (" (≈ ") + noteName (hs.rMs, c.bpm) + ")").c_str() : "", hs.slopeR, hs.truncated ? " Coupé par le coup suivant." : "");
                else t.live = fmt ("Crête %.1f dBFS · longueur %.0f ms%s. Pour une queue qui respire au tempo : T40 = %.0f ms contre un temps de %.0f ms.", hs.peakDb, hs.lengthMs, hs.truncated ? " (coupé)" : "", hs.t40, q);
            }
            break;
        }
        case RSpgCtl:
            t.title = "Réglages du spectrogramme"; t.body = "SOURCE : mix, milieu (centre) ou côtés. RÉSOLUTION : grande = notes précises, petite = transitoires nets. TEMPS : durée visible. PLAGE : niveaux affichés. PENTE : compense la chute naturelle des aigus."; break;
        case RSpectrum:
            t.title = "Analyseur de spectre"; t.body = "Niveau par bande de fréquence (échelle log). Courbe pleine : instantané. Trait fin : crête. Pointillé : moyenne depuis la remise à zéro. Un mix équilibré descend doucement vers les aigus (environ -3 à -4,5 dB par octave).";
            if (h.hasData)
            {
                std::string zt, zx; freqZone (h.freq, c.style, zt, zx);
                t.live = fmt ("%.0f Hz : %.1f dB.\n%s : %s", h.freq, h.db, zt.c_str(), zx.c_str());
            }
            break;
        case RSpBands:
        {
            t.title = "Équilibre tonal";
            t.body = "Écart de chaque zone par rapport à une courbe de référence de ton style (en dB, 0 = dans le repère). Une barre hors de la zone grise signale un excès ou un manque probable.";
            if (h.band >= 0 && h.band < kBands)
            {
                float dev[kBands]; bandDeviation (a.bandLiveDb, st, dev);
                t.live = fmt ("%s (%.0f–%.0f Hz) : %+.1f dB vs repère %s · %.0f %% de l'énergie.", bandName (h.band), bandEdges()[h.band], bandEdges()[h.band + 1], dev[h.band], st.name, a.bandShareLive[h.band] * 100.0f);
            }
            break;
        }
        case RSpSub:
            t.title = "Fondamentale grave"; t.body = "Fréquence du pic le plus net entre 25 et 160 Hz : celle de ton kick ou 808. Accorde-la sur la tonalité du morceau.";
            if (a.subProm > 10.0f) { int cents = 0; const int mid = noteFromHz (a.subHz, cents); t.live = fmt ("%.1f Hz = %s%d (%+d cents).", a.subHz, midiName (mid), midiOctave (mid), cents); }
            break;
        case RSpCtl:
            t.title = "Réglages du spectre"; t.body = "FFT : grande = précis dans les graves (plus lent). LISSAGE : fraction d'octave. VITESSE : réactivité. Les courbes L, R, MID, SIDE, ST se cumulent."; break;
        case RScope:
            t.title = "Vectorscope"; t.body = "Chaque point est un instant du signal : vertical = centre (Mid), horizontal = côtés (Side). Une ligne verticale = mono, un nuage large = stéréo ; une diagonale = un seul côté. Un point loin du centre horizontal en opposition de phase est un risque en mono.";
            t.live = fmt ("Corrélation %.2f · largeur Side/Mid %+.1f dB.", m.corr, m.sideDb - m.midDb);
            break;
        case RCorr:
            t.title = "Corrélation de phase"; t.body = "+1 : mono parfait. 0 : stéréo large non corrélée. Négatif : phases opposées, le signal disparaît en mono. En musique : 0,3 à 0,9 est typique.";
            t.live = fmt ("Actuelle : %.2f. Perte en mono : %.1f dB.%s", m.corr, -m.monoFoldDb, m.corr < 0.0f ? " Cherche un doublage inversé ou un élargisseur trop fort." : "");
            break;
        case RBalance:
            t.title = "Balance gauche/droite"; t.body = "Différence de niveau entre G et D (RMS). Un léger écart est normal avec des guitares doublées."; t.live = fmt ("%+.1f dB.", m.balanceDb); break;
        case RWidthBands:
        {
            t.title = "Largeur par bande"; t.body = "Corrélation de phase par zone de fréquences. Idéal : graves proches de +1 (mono), aigus plus bas (larges). Une barre rouge à gauche de zéro = problème de phase.";
            if (h.band >= 0 && h.band < kBands) t.live = fmt ("%s : corrélation %.2f · Side %.1f dB sous/sur le Mid.", bandName (h.band), a.bandCorr[h.band], a.bandSideDb[h.band]);
            break;
        }
        case RMonoFold:
            t.title = "Compatibilité mono"; t.body = "Niveau perdu quand G et D sont additionnés. Téléphones, enceintes Bluetooth et sonos de petites salles sont souvent mono."; t.live = fmt ("%.1f dB.", m.monoFoldDb); break;
        case RLdM:
            t.title = "Sonie momentanée (400 ms)"; t.body = "Réaction rapide, façon « battement » : suit les coups de kick et de caisse claire.";
            t.live = fmt ("%.1f LUFS (max %.1f).", m.lufsM, m.lufsMmax); break;
        case RLdS:
            t.title = "Sonie court terme (3 s)"; t.body = "Niveau « ressenti » sur quelques secondes : c'est lui qui montre si le refrain est plus fort que le couplet.";
            t.live = fmt ("%.1f LUFS (max %.1f).", m.lufsS, m.lufsSmax); break;
        case RLdI:
        {
            t.title = "Sonie intégrée"; t.body = "Moyenne sur toute la mesure, silences exclus (norme BS.1770 / EBU R128). C'est ce que les plateformes utilisent pour normaliser.";
            std::string x = fmt ("%.1f LUFS", m.lufsI);
            if (c.target == TargetSpotify && m.lufsI > -100.0f) x += fmt (" · Spotify (%s) : %+.1f dB appliqués.", spotModeName (c.spot), T - m.lufsI > 0 ? std::min (T - m.lufsI, -1.0f - m.tpMax) : T - m.lufsI);
            x += fmt (" Repère %s : %.0f à %.0f LUFS.", st.name, st.lufsLo, st.lufsHi);
            t.live = x; break;
        }
        case RLdLra:
            t.title = "Plage de sonie (LRA)"; t.body = "Écart entre les passages calmes et forts (centiles 10–95 du court terme). Beaucoup de pop-punk compressée : 3 à 6 LU ; morceaux avec gros contrastes : 8 LU et plus.";
            t.live = fmt ("%.1f LU.", m.lra); break;
        case RLdTp:
            t.title = "True peak"; t.body = "Crête réelle du signal reconstruit (suréchantillonné ×4), qui peut dépasser les échantillons. Garde-la sous le plafond de ton support.";
            t.live = fmt ("G %.1f · D %.1f · max %.1f dBTP. Plafond conseillé %s : %.1f dBTP.", m.tpL, m.tpR, m.tpMax, targetName (c.target), tpCeiling (c.target, m.lufsI)); break;
        case RLdPlr:
            t.title = "PLR (crête / sonie)"; t.body = "True peak max moins sonie intégrée : la réserve dynamique du master. Un PLR faible = limiteur trop sollicité.";
            t.live = fmt ("%.1f dB. Repère %s : ≥ %.0f dB.", m.plr, st.name, st.plrMin); break;
        case RLdPsr:
            t.title = "PSR (crête / court terme)"; t.body = "Même idée sur les 3 dernières secondes : montre si les transitoires sont écrasés en ce moment."; t.live = fmt ("%.1f dB.", m.psr); break;
        case RLdHist:
            t.title = "Distribution de la sonie"; t.body = "Combien de temps tu passes à chaque niveau (court terme). Un pic étroit = niveau constant ; une plaine large = dynamique. Le trait de la cible montre où la plateforme te ramène.";
            break;
        case RLdBar: t.title = "Échelle de sonie"; t.body = "Barres M (400 ms) et S (3 s) en LUFS. La ligne marque la cible du support choisi."; break;
        case RHistory:
            t.title = "Historique"; t.body = "Sonie momentanée et court terme, crête et corrélation dans le temps. Passe la souris pour lire un instant : repère le refrain, le drop ou le passage qui sort du lot.";
            if (h.hasData) t.live = fmt ("Il y a %.0f s : %.1f LUFS.", h.timeS, h.db);
            break;
        case RLvPeak: t.title = "Crête (échantillons)"; t.body = "Plus haut échantillon, par canal. Sans suréchantillonnage : regarde plutôt le true peak."; t.live = fmt ("G %.1f · D %.1f · max %.1f dBFS.", m.peakL, m.peakR, m.peakMax); break;
        case RLvRms: t.title = "RMS (300 ms)"; t.body = "Niveau moyen non pondéré. Un sinus plein échelle lit -3 dB."; t.live = fmt ("G %.1f · D %.1f dBFS.", m.rmsL, m.rmsR); break;
        case RLvCrest: t.title = "Facteur de crête"; t.body = "Crête moins RMS sur 3 s. En dessous de 6 dB, le signal est très écrasé ; 12 dB et plus : très dynamique."; t.live = fmt ("%.1f dB.", m.crest3s); break;
        case RLvDc: t.title = "Décalage DC"; t.body = "Composante continue : elle consomme du headroom. Un passe-haut à 10–20 Hz l'enlève."; t.live = fmt ("%.0f dBFS.", m.dcDb); break;
        case RLvClip: t.title = "Écrêtages"; t.body = "Nombre d'événements où un échantillon atteint 0 dBFS (et dépassements de true peak) depuis la remise à zéro."; t.live = fmt ("%u écrêtages · %u dépassements true peak.", m.clipCount, m.tpOverCount); break;
        case RExVerdict:
            t.title = "Verdict d'export"; t.body = "Résumé de ce qui arrivera à ton morceau chez la plateforme choisie, à partir de la dernière lecture complète. Joue le morceau du début à la fin après une remise à zéro pour des chiffres fiables.";
            t.live = fmt ("Mesure : %.0f s.", m.measuredSec); break;
        case RExTable: t.title = "Plateformes"; t.body = "Gain appliqué par chaque plateforme sur ton master. « Pic après » ajoute l'estimation de dépassement due à l'encodage."; break;
        case RExCodec:
            t.title = "Simulation d'encodage"; t.body = "Modèle de codec à perte, calibré sur de vrais encodages Ogg Vorbis. Estime de combien les crêtes dépassent après encodage. C'est une estimation : l'écart réel dépend du contenu (±1,5 dB à 96 kbps).";
            break;
        case RExChecks: t.title = "Check-list"; t.body = "Vert : bon. Orange : à surveiller. Rouge : à corriger avant l'export. Chaque ligne explique pourquoi."; break;
        case RExSpot: t.title = "Mode d'écoute Spotify"; t.body = "Les auditeurs Premium choisissent Normal (-14), Loud (-11) ou Quiet (-19). Le défaut est Normal. En Loud, Spotify applique un limiteur si le pic dépasse -1 dB."; break;
        default: break;
    }
    return t;
}

} // namespace mask
