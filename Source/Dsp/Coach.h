// mask::scope — coach temps réel : conseils selon mesures, BPM, style et support cible
#pragma once
#include <map>
#include "Targets.h"

namespace mask
{
struct CoachCtx
{
    MeterBlock m;
    AnalysisBlock a;
    double bpm = 0.0;
    int sigNum = 4;
    bool bpmFromHost = false;
    int target = TargetSpotify, spot = SpotNormal, style = 4;
    bool signal = false;       // du signal passe
};

struct Advice
{
    int id = 0;
    int sev = 0;               // 0 idée · 1 conseil · 2 attention · 3 problème
    int tab = TabOverview;     // module à ouvrir
    std::string title, text;
};

inline float targetLufsOf (const CoachCtx& c)
{
    if (c.target == TargetSpotify) return spotifyLufs (c.spot);
    return 0.0f;
}

inline void bpmNumbers (double bpm, double& q, double& e, double& s, double& ed)
{
    q = beatMs (bpm); e = q / 2; s = q / 4; ed = q * 0.75;
}

inline Advice learnTip (int index, const CoachCtx& c)
{
    Advice a; a.id = 900 + index; a.sev = 0; a.tab = TabOverview;
    const bool hb = c.bpm > 30.0;
    double q = 0, e = 0, s = 0, ed = 0;
    if (hb) bpmNumbers (c.bpm, q, e, s, ed);
    struct T { std::string t, x; };
    std::vector<T> list;
    // spécifiques au style
    const int st = c.style;
    if (st == 0 || st == 4)
    {
        list.push_back ({ "Guitares doublées", "Joue deux prises de guitares et mets-les à gauche et à droite à 100 %. Garde kick, basse, caisse claire et voix lead au centre : si le centre est vide, le morceau sonne creux." });
        list.push_back ({ "Mur de guitares", "Passe-haut vers 80–100 Hz sur les guitares, creuse 250–400 Hz, petite bosse douce à 2–3 kHz pour la présence. La basse porte le grave." });
        list.push_back ({ "Voix émo", "Deux compresseurs légers en série valent mieux qu'un seul fort. De-esser vers 6–8 kHz, et automatise la réverb/delay sur le dernier mot de la phrase." });
        list.push_back ({ "Caisse claire", "Double-la avec un clap ou une 2e caisse claire : corps vers 200 Hz, claquement entre 4 et 6 kHz. Un transient shaper règle son sustain sans toucher au tempo." });
    }
    if (st == 1 || st == 4)
    {
        list.push_back ({ "808 audible partout", "Ajoute de la saturation ou du clipper : les harmoniques entre 100 et 300 Hz font entendre le sub sur téléphone. Accorde l'808 sur la tonalité du morceau." });
        list.push_back ({ "Voix brillante", "Saturation parallèle, puis shelf +2 dB vers 10–12 kHz après la compression. Passe un de-esser avant de booster les aigus." });
        list.push_back ({ "Écrêtage créatif", "Un clipper avant le limiteur donne de la densité sans pomper. Surveille le PLR (≥ 6 dB) et le true peak pour que l'export reste propre." });
        list.push_back ({ "Aigus larges, graves mono", "Mono sous ~150 Hz, large au-dessus (doublages, chorus, effet Haas 10–20 ms). Contrôle la corrélation dans le module Champ stéréo." });
    }
    if (st == 3 || st == 4)
    {
        list.push_back ({ "Kick et basse", hb ? fmt ("Un seul des deux domine le bas : sépare vers 90–100 Hz et mets un sidechain calé sur le 1/4 (%.0f ms). Basse en mono.", q)
                                               : std::string ("Un seul des deux domine le bas : sépare vers 90–100 Hz, sidechain calé sur le 1/4, basse en mono.") });
        list.push_back ({ "Hi-hats", "Passe-haut vers 400–500 Hz sur hats et percussions pour dégager les médiums. Niveau environ 8 à 12 dB sous le kick." });
    }
    if (st == 2)
    {
        list.push_back ({ "Dynamique d'abord", "Garde un PLR autour de 9–12 dB : le refrain gagne en impact grâce au contraste plus qu'au volume." });
        list.push_back ({ "Grain naturel", "Préfère la saturation douce (bande, tube) à la compression forte pour garder de la vie dans les transitoires." });
    }
    // calés sur le tempo
    if (hb)
    {
        list.push_back ({ "Release du compresseur de bus", fmt ("Cale le release sur le groove : environ 1/8 (%.0f ms) pour que le compresseur remonte avant le coup suivant. Trop long = pompage mou, trop court = basses déformées.", e) });
        list.push_back ({ "Pré-delay de réverb voix", fmt ("Un pré-delay de 1/64 à 1/32 (%.0f–%.0f ms) garde la voix devant. La queue de réverb doit mourir avant le temps fort suivant (≈ %.0f ms).", q / 16, q / 8, q) });
        list.push_back ({ "Delay en rythme", fmt ("Un delay en 1/8 pointé (%.0f ms) donne le rebond des refrains. Passe-bas vers 3–4 kHz pour qu'il reste derrière la voix.", ed) });
        list.push_back ({ "Sidechain kick / basse", fmt ("Release entre 1/16 et 1/8 (%.0f–%.0f ms) : le kick respire sans trou audible dans la basse.", s, e) });
    }
    // mastering et diffusion
    list.push_back ({ "Spotify normalise", "Spotify ramène les morceaux forts à -14 LUFS : aucun gain de volume au-delà. Cherche le meilleur compromis son/densité plutôt qu'un chiffre." });
    list.push_back ({ "Marge d'encodage", "Laisse -1 dBTP (-2 dBTP si tu dépasses -14 LUFS) : l'encodage Ogg/AAC peut faire dépasser les crêtes. Vérifie dans la page EXPORT." });
    list.push_back ({ "Écoute en mono", "Teste ton mix en mono 30 secondes : si guitares ou voix disparaissent, il y a un problème de phase." });
    list.push_back ({ "Morceau de référence", "Charge un titre de ton style au même niveau sonore que le tien et compare les spectrogrammes : vise la même forme, pas le même volume." });
    const auto& pick = list[(size_t) (index % (int) list.size())];
    a.title = pick.t; a.text = pick.x;
    return a;
}

class CoachEngine
{
public:
    // tous les conseils vrais « maintenant » ; update() applique l'hystérésis
    void evaluate (const CoachCtx& c, std::vector<Advice>& out) const
    {
        out.clear();
        const MeterBlock& m = c.m;
        const AnalysisBlock& a = c.a;
        const Style& st = styleAt (c.style);
        const bool sig = c.signal;
        const bool haveBpm = c.bpm > 30.0;
        double q = 0, e = 0, s = 0, ed = 0;
        if (haveBpm) bpmNumbers (c.bpm, q, e, s, ed);
        auto add = [&] (int id, int sev, int tab, std::string t, std::string x) { Advice ad; ad.id = id; ad.sev = sev; ad.tab = tab; ad.title = std::move (t); ad.text = std::move (x); out.push_back (std::move (ad)); };
        const float ceil = tpCeiling (c.target, m.lufsI);
        const bool measured = m.measuredSec >= 15.0f;

        // ----- niveaux / support -----
        if (m.tpMax > 0.0f)
            add (1, 3, TabLoudness, fmt ("True peak à %+.1f dBTP", m.tpMax),
                 fmt ("Les crêtes dépassent 0 dBTP : ça écrête au décodage (Ogg/AAC/MP3) et sur certains convertisseurs. Baisse le plafond du limiteur à %.1f dBTP et active son mode True Peak / oversampling.", ceil));
        else if (c.target != TargetOff && m.tpMax > ceil)
            add (2, 2, TabLoudness, fmt ("Plafond %s : %.1f dBTP", targetName (c.target), ceil),
                 fmt ("Ton pic est à %+.1f dBTP. Pour %s, garde-toi %.1f dBTP de plafond pour que l'encodage ou le convertisseur ne dépasse pas.", m.tpMax, targetName (c.target), ceil));
        if (c.target == TargetSpotify && measured && m.lufsI > -100.0f)
        {
            const float T = spotifyLufs (c.spot);
            const float ex = m.lufsI - T;
            if (ex > 1.0f)
                add (3, ex > 6.0f ? 2 : 1, TabExport, fmt ("Spotify va te baisser de %.1f dB", ex),
                     fmt ("Intégré à %.1f LUFS ; Spotify (%s) ramène à %.0f. Ce qui dépasse n'ajoute aucun volume, mais le limiteur a déjà écrasé les transitoires (PLR %.1f dB). Souvent, 2 à 3 dB de limiteur en moins rendent le morceau plus percutant à volume égal.", m.lufsI, spotModeName (c.spot), T, m.plr));
            else if (ex < -3.0f)
                add (4, 0, TabExport, fmt ("Spotify va te remonter de %.1f dB", -ex),
                     fmt ("Ton morceau est plus faible que la cible : Spotify le remonte, mais sans dépasser -1 dBTP (marge disponible : %.1f dB).", std::max (0.0f, -1.0f - m.tpMax)));
        }
        if (measured && m.plr > 0.0f && m.plr < st.plrMin)
            add (5, 2, TabLoudness, fmt ("PLR faible : %.1f dB", m.plr),
                 fmt ("Écart crête/sonie intégrée = %.1f dB (repère %s : au moins %.0f). Les transitoires sont écrasés : essaie un clipper doux avant le limiteur ou 1–2 dB de limitation en moins.", m.plr, st.name, st.plrMin));
        if (sig && m.crest3s > 0.0f && m.crest3s < 5.5f && m.lufsS > -30.0f)
            add (6, 1, TabLevels, fmt ("Crête/RMS bas : %.1f dB", m.crest3s),
                 haveBpm ? fmt ("Le signal est très compressé sur les 3 dernières secondes. Si c'est avant le master, relâche le comp de bus (ratio 2:1, release ≈ 1/8 = %.0f ms).", e)
                         : std::string ("Le signal est très compressé sur les 3 dernières secondes. Si c'est avant le master, relâche le compresseur de bus."));
        if (m.measuredSec >= 40.0f)
        {
            if (m.lra < 2.0f)
                add (7, 0, TabLoudness, fmt ("Peu de contraste (LRA %.1f LU)", m.lra),
                     "Couplet et refrain sont presque au même niveau. Automatise +1 à +2 dB sur le refrain ou le drop : l'impact vient du contraste, surtout en punk pop / emo.");
            else if (m.lra > 12.0f)
                add (7, 1, TabLoudness, fmt ("Dynamique très large (LRA %.1f LU)", m.lra),
                     "Les passages calmes seront trop faibles en streaming et en voiture. Remonte-les ou compresse légèrement le bus.");
        }
        if (m.dcDb > -60.0f) add (8, 2, TabLevels, "Décalage DC détecté", "Ajoute un passe-haut à 10–20 Hz : un DC mange du headroom et peut faire des clics.");
        if (m.clipCount > 0) add (9, 2, TabLevels, fmt ("Écrêtage détecté (%u)", m.clipCount), "Des échantillons atteignent 0 dBFS. Baisse le gain avant le limiteur ; si l'écrêtage est voulu, vérifie qu'il sonne bien après encodage (page EXPORT).");

        // ----- champ stéréo -----
        if (sig && ! m.mono)
        {
            if (m.corr < -0.15f)
                add (10, 2, TabSoundField, fmt ("Phase négative (corrélation %.2f)", m.corr),
                     fmt ("En mono tu perds %.1f dB. Cherche une piste en opposition de phase (micros stéréo, doublage inversé, élargisseur) et teste le bouton Ø d'un plugin.", -m.monoFoldDb));
            else if (m.monoFoldDb < -3.0f)
                add (11, 1, TabSoundField, fmt ("Compatibilité mono : -%.1f dB", -m.monoFoldDb),
                     "En mono (téléphone, enceinte Bluetooth, sono) le mix perd du niveau. Réduis la largeur des synthés/voix larges (Haas, chorus).");
            if ((a.bandCorr[0] < 0.75f || a.bandCorr[1] < 0.75f) && a.bandShareLive[0] + a.bandShareLive[1] > 0.05f)
                add (12, 1, TabSoundField, "Graves trop larges", "Sous ~120 Hz mets tout en mono (Utility / width 0 %) : plus de punch, plus de headroom, et compatible vinyle et sono.");
            if (std::abs (m.balanceDb) > 1.5f)
                add (13, 1, TabSoundField, fmt ("Déséquilibre gauche/droite : %+.1f dB", m.balanceDb), "Vérifie le pan des guitares doublées, ou un élément unique placé hors centre.");
            if (a.bandCorr[3] > 0.97f && a.bandCorr[5] > 0.97f && a.bandCorr[7] > 0.97f && a.bandSideDb[5] < -30.0f)
                add (14, 0, TabSoundField, "Image très étroite", "Le mix est quasi mono : double-track les guitares (L/R 100 %) et élargis les nappes avec un imager sur les médiums/aigus seulement.");
        }

        // ----- spectre -----
        if (sig && a.sessFrames > 2.0f)
        {
            float dev[kBands];
            bandDeviation (a.bandLiveDb, st, dev);
            for (int b = 0; b < kBands; ++b)
            {
                if (a.bandShareLive[b] < 1e-5f) continue;
                if (dev[b] > st.hi[b]) addBandHigh (add, b, dev[b], c, e);
                else if (dev[b] < st.lo[b]) addBandLow (add, b, dev[b], c);
            }
            int cents = 0;
            if (a.subProm > 15.0f && a.subHz >= 30.0f && a.subHz <= 90.0f)
            {
                const int mid = noteFromHz (a.subHz, cents);
                add (31, 0, TabSpectrum, fmt ("Fondamentale grave : %.0f Hz (%s%d)", a.subHz, midiName (mid), midiOctave (mid)),
                     fmt ("Accorde ton kick/808 sur la tonalité du morceau (ici %+d cents de la note). Évite deux notes de sub qui se chevauchent : elles s'annulent ou se masquent.", cents));
            }
        }

        // ----- coups (ADSR) -----
        if (a.haveHit && sig)
        {
            const HitSummary& h = a.lastHit;
            if (h.aMs <= 2.0f) add (35, 0, TabSpectrogram, fmt ("Attaque très sèche (%.0f ms)", h.aMs), "Le clic est très marqué : parfait pour un kick punchy. S'il claque trop, un passe-bas ou un clipper léger sur la partie claquante l'adoucit.");
            else if (h.aMs >= 25.0f) add (36, 0, TabSpectrogram, fmt ("Attaque molle (%.0f ms)", h.aMs), "Il manque du « tac » : transient shaper en positif, couche d'un clic entre 3 et 5 kHz, ou compresseur avec attaque lente (20–30 ms).");
            if (haveBpm && h.t40 > 0.75f * (float) q)
                add (37, 1, TabSpectrogram, "La queue déborde sur le temps suivant",
                     fmt ("Le dernier coup met %.0f ms à tomber de 40 dB (≈ %s) pour un temps de %.0f ms : il chevauche le suivant. Raccourcis le decay ou ajoute un sidechain.", h.t40, noteName (h.t40, c.bpm), q));
        }

        // ----- BPM -----
        if (haveBpm)
        {
            add (32, 0, TabOverview, fmt ("À %.0f BPM", c.bpm),
                 fmt ("1/4 = %.0f ms · 1/8 = %.0f · 1/16 = %.0f · 1/8 pointée = %.0f. Release de comp bus ≈ %.0f–%.0f ms, pré-delay de réverb voix ≈ %.0f ms, delay 1/8 pointé = %.0f ms.", q, e, s, ed, s, e, q / 16, ed));
            if (c.bpm >= 165.0)
                add (33, 0, TabOverview, "Tempo rapide", fmt ("Garde kick et basse courts (moins d'un 1/16 = %.0f ms) pour que ça respire, des réverbes qui meurent en 1 temps (%.0f ms) et des releases courts.", s, q));
            else if (c.bpm <= 100.0)
                add (34, 0, TabOverview, "Tempo lent", fmt ("Tu as de la place : queues de réverb plus longues (1/2 note = %.0f ms) et delays pointés ; attention aux creux d'énergie entre les coups.", q * 2));
        }

        // ----- support -----
        if (c.target == TargetSpotify && a.codec[0].valid)
        {
            const float os = a.codec[0].overshootMaxDb;
            if (m.tpMax > -100.0f && os > 0.5f)
            {
                const float after = m.tpMax + os;
                add (38, after > 0.0f ? 2 : 1, TabExport, fmt ("Encodage 96 kbps : +%.1f dB estimés", os),
                     fmt ("Avec un pic à %+.1f dBTP tu iras vers %+.1f dBTP après encodage (estimation, ±1,5 dB selon le contenu). Reste à %.1f dBTP ou moins.", m.tpMax, after, ceil));
            }
        }
        if (c.target == TargetConcert && sig)
        {
            if (a.bandShareLive[0] > 0.12f) add (39, 1, TabSpectrum, "Sub dominant pour la sono", "Les subs de salle réagissent fort sous 40 Hz (résonances, rumble). Passe-haut vers 30 Hz et reste prudent entre 30 et 40 Hz.");
            if (measured && m.plr > 0.0f && m.plr < 7.0f) add (41, 1, TabLoudness, "Playback de concert très écrasé", "À haut niveau sonore, un master sans dynamique fatigue vite. Garde un PLR d'au moins 8 dB ; l'ingénieur du son aura plus de marge.");
        }
        if (c.target == TargetCd) add (40, 0, TabExport, "Export CD : 16 bits / 44,1 kHz", "Dither (TPDF ou POW-r) en tout dernier maillon, une seule fois. Plafond conseillé -0,3 dBTP : pas de normalisation sur CD.");
    }

    // Hystérésis : un conseil apparaît s'il est vrai depuis minOn s, disparaît s'il est faux depuis minOff s
    void update (const CoachCtx& c, double nowSec, std::vector<Advice>& visible, int learnIndex)
    {
        std::vector<Advice> now;
        evaluate (c, now);
        for (auto& ad : now)
        {
            auto& st = state[ad.id];
            if (st.firstTrue < 0.0) st.firstTrue = nowSec;
            st.lastTrue = nowSec;
            st.adv = ad;
        }
        for (auto it = state.begin(); it != state.end();)
        {
            bool isNow = false;
            for (auto& ad : now) if (ad.id == it->first) { isNow = true; break; }
            if (! isNow)
            {
                it->second.firstTrue = -1.0;
                if (nowSec - it->second.lastTrue > 3.0) { it = state.erase (it); continue; }
            }
            ++it;
        }
        visible.clear();
        for (auto& kv : state)
        {
            const double minOn = kv.second.adv.sev >= 2 ? 1.0 : 2.5;
            const bool on = kv.second.firstTrue >= 0.0 && nowSec - kv.second.firstTrue >= minOn;
            const bool holding = kv.second.firstTrue < 0.0 && kv.second.shown;
            kv.second.shown = on || holding;
            if (kv.second.shown) visible.push_back (kv.second.adv);
        }
        std::sort (visible.begin(), visible.end(), [] (const Advice& x, const Advice& y) { return x.sev != y.sev ? x.sev > y.sev : x.id < y.id; });
        if ((int) visible.size() < 3 && c.signal) visible.push_back (learnTip (learnIndex, c));
        if (visible.size() > 6) visible.resize (6);
    }

private:
    struct St { double firstTrue = -1.0, lastTrue = -1.0; bool shown = false; Advice adv; };
    std::map<int, St> state;

    template <typename Add>
    static void addBandHigh (Add& add, int b, float dev, const CoachCtx& c, double eighthMs)
    {
        const int id = 15 + b;
        const std::string d = fmt ("+%.0f dB vs repère", dev);
        switch (b)
        {
            case 0: add (id, 1, TabSpectrum, "Sub trop fort (" + d + ")", "Le sub (20–60 Hz) mange le headroom et disparaît sur téléphone ou enceinte Bluetooth. Passe-haut vers 30 Hz sur tout sauf kick/808, et vérifie que le sub est en mono."); break;
            case 1: add (id, 1, TabSpectrum, "Basses 60–150 Hz dominantes (" + d + ")",
                         eighthMs > 0 ? fmt ("Kick et basse se marchent dessus : sidechain léger de la basse sur le kick (release ≈ %.0f ms) ou EQ complémentaire (kick vers 60 Hz, basse vers 90–100 Hz).", eighthMs)
                                      : std::string ("Kick et basse se marchent dessus : sidechain léger de la basse sur le kick ou EQ complémentaire (kick vers 60 Hz, basse vers 90–100 Hz).")); break;
            case 2: add (id, 1, TabSpectrum, "Zone boueuse 150–400 Hz (" + d + ")", "Creuse 2–3 dB vers 250 Hz sur guitares, synthés et voix, et garde la place pour le corps de la caisse claire."); break;
            case 3: add (id, 1, TabSpectrum, "Médiums 400 Hz–1 kHz chargés (" + d + ")", "Son nasal ou « boîte » : cherche l'accumulation vers 500–800 Hz sur les guitares empilées et les synthés."); break;
            case 4: add (id, 1, TabSpectrum, "1–2,5 kHz saillants (" + d + ")", "Ça couvre la voix : un EQ dynamique de -2 dB à 1,5–2 kHz sur guitares/synthés, déclenché par la voix, libère de la place."); break;
            case 5: add (id, 2, TabSpectrum, "2,5–5 kHz trop forts (" + d + ")", "Zone de mordant et de fatigue : -2 dB dynamique vers 3–4 kHz sur guitares saturées et voix, et contrôle les saturateurs en parallèle."); break;
            case 6: add (id, 1, TabSpectrum, "5–9 kHz saillants (" + d + ")", "Sibilances ou hi-hats : de-esser vers 6–8 kHz sur la voix, shelf -1,5 dB à 8 kHz sur les cymbales."); break;
            default: add (id, 1, TabSpectrum, "Aigus 9–20 kHz élevés (" + d + ")", "Brillant à l'excès : attention au casque et à l'encodage ; un saturateur en parallèle peut en être responsable."); break;
        }
        (void) c;
    }
    template <typename Add>
    static void addBandLow (Add& add, int b, float dev, const CoachCtx& c)
    {
        const int id = 23 + b;
        const std::string d = fmt ("%.0f dB vs repère", dev);
        switch (b)
        {
            case 0: if (c.style == 1 || c.style == 3 || c.style == 4) add (id, 0, TabSpectrum, "Sub discret (" + d + ")", "Pour un 808 ou un kick qui pousse : accorde-le sur la tonalité et ajoute des harmoniques (saturation) vers 100–200 Hz pour l'entendre sur téléphone."); break;
            case 1: add (id, 1, TabSpectrum, "Peu de basses 60–150 Hz (" + d + ")", "Kick et basse manquent de corps : shelf +1 à +2 dB vers 80–100 Hz sur le bus basse, ou renforce la basse."); break;
            case 2: add (id, 0, TabSpectrum, "Creux 150–400 Hz (" + d + ")", "Le son peut paraître fin : un peu de 200 Hz sur caisse claire, guitares et voix donne du corps."); break;
            case 3: add (id, 0, TabSpectrum, "Médiums creux (" + d + ")", "Guitares et voix manquent de corps : vérifie que tu n'as pas trop coupé vers 400–800 Hz."); break;
            case 5: add (id, 1, TabSpectrum, "Manque de mordant 2,5–5 kHz (" + d + ")", "Voix et caisse claire manquent de définition : +1,5 dB large vers 3 kHz, ou saturation douce sur la voix."); break;
            case 6: add (id, 0, TabSpectrum, "Peu de brillance 5–9 kHz (" + d + ")", "Ajoute de la clarté : shelf +1,5 dB à 6–8 kHz sur la voix ou les overheads."); break;
            case 7: add (id, 0, TabSpectrum, "Manque d'air 9–20 kHz (" + d + ")", "Pour un rendu brillant façon hyperpop : shelf +2 dB vers 10–12 kHz après la compression, et saturation parallèle sur la voix. Les encodages bas débit (≈ 96 kbps) coupent vers 16–17 kHz."); break;
            default: break;
        }
    }
};

} // namespace mask
