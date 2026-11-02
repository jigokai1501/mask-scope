# mask::scope — analyseur pour Logic Pro (AU + VST3)

Analyseur de spectre / spectrogramme / champ stéréo / sonie / historique, avec **bulles d'astuces en direct** (selon le BPM, ton style et le support de diffusion) et une **page EXPORT** qui prédit ce qui arrivera à ton morceau sur Spotify, en concert ou sur CD.
Il ne modifie jamais l'audio (sortie = entrée, bit à bit), n'a aucun trim, mix ni gain : c'est uniquement un instrument de mesure. Latence 0.

- Nom affiché : `mask::scope` · fichier : `mask-scope` · fabricant AU `Mask` · AU `aufx` / `Mscp` / `Mask`
- Dominante de couleur : **menthe #35f0b2** (charte `mask::` : « mask:: » en blanc, le nom en couleur)
- JUCE 8.0.15 · macOS 11+ (universel arm64 + x86_64) · stéréo ou mono

## Les modules

| Onglet | Contenu | Inspiré de |
|---|---|---|
| **VUE** | tous les modules en un coup d'œil ; clic sur le titre ou double-clic = plein écran, Échap = retour | Insight 2 |
| **SPECTRO** | spectrogramme au centre, spectre en barres à droite (même axe de fréquences), enveloppe/forme d'onde en bas avec les zones A D S R de chaque coup, panneau « dernier coup » | Vision 4X |
| **SPECTRE** | analyseur L / R / MID / SIDE / ST, moyenne, crête, FFT 2048→32768, lissage en fraction d'octave, vitesse, pente, équilibre tonal par zones vs repère, fondamentale grave (note + cents) | Insight 2 |
| **STÉRÉO** | goniomètre, L/R, polaire (niveau), corrélation (+ historique 60 s), balance, compatibilité mono, Mid/Side, corrélation par zone | Insight 2 |
| **SONIE** | LUFS momentané / court terme / intégré (BS.1770-4, EBU R128), LRA, true peak, PLR, PSR, distribution du court terme, écart à la cible | Loudness Meter |
| **HISTO** | historique 1 min / 5 min / 15 min / session : sonie, true peak, corrélation | Insight 2 |
| **NIVEAUX** | crête, RMS, facteur de crête, DC, écrêtages, marge | |
| **EXPORT** | verdict, plateformes de streaming, simulation d'encodage, check-list, « à faire avant l'export » | |

### Le spectrogramme (calqué sur Vision 4X)
- Spectrogramme au centre, **barres de spectre à droite alignées sur le même axe de fréquences**, **enveloppe en bas** sur le même axe de temps.
- Chaque coup détecté est décomposé en **A** (attaque, du pied à 10 % jusqu'à la crête), **D** (decay), **S** (plateau ou queue lente), **R** (release), avec durées en ms, **valeur de note selon le BPM** (« ≈ 1/16 »), pentes en dB/s et T20 / T40 / T60.
  Si la queue est coupée par le coup suivant, c'est indiqué.
- Durée visible 0,5 s → 30 s (molette), plage de niveaux, pente de compensation, palette (MASK / ICE / MINT / GRAY), résolution 2048 / 4096 / 8192, source MIX / MID / SIDE, GEL.

## Les astuces
- Bouton **ASTUCES** (en-tête) : OFF = aucune bulle, pas de bandeau coach. ON = reste ~½ s sur une zone (réglable 250 ms → 1 s) : bulle avec explication, **lecture en direct** (fréquence, note, niveau, valeurs mesurées) et conseil.
- Les conseils dépendent du **BPM** (lu dans Logic, ou saisi), du **style** (MIX PERSO punk pop/emo/hyperpop/alt + touches techno, ou PUNK POP / EMO, HYPERPOP, ALT, TECHNO) et du **support**.
- **Bandeau COACH** en bas : 3 conseils maximum, classés par gravité, avec hystérésis (un problème n'apparaît que s'il dure, et ne clignote pas). Un clic ouvre le module concerné. Quand tout va bien, il propose une idée à apprendre toutes les 25 s.

## Support cible et page EXPORT
- **SPOTIFY** (écoute Premium Normal -14 / Loud -11 / Quiet -19) · **CONCERT** · **CD** · **OFF**.
- Spotify (source : Spotify for Artists) : normalisation à -14 LUFS (BS.1770), true peak conseillé ≤ -1 dBTP, ≤ -2 dBTP si le master dépasse -14 LUFS ; remontée des morceaux faibles limitée par 1 dB de marge ; en Loud, limiteur à -1 dB (attaque 5 ms, release 100 ms).
- Tableau pour Spotify, Apple Music (-16), YouTube (-14, baisse seulement), Tidal, Amazon, Deezer (-15), SoundCloud : gain appliqué, sonie et pic après normalisation, état. Les valeurs hors Spotify viennent de sources publiques courantes (à vérifier).
- **Simulation d'encodage** (OGG 96 / 160 / 320 kbps) : estime de combien les crêtes dépassent après encodage et la coupure des aigus.
  C'est un **modèle**, calibré sur de vrais encodages libvorbis (`tools/`). Dispersion observée : ± 1,5 dB à 96 kbps, ± 0,7 à 160, ± 0,1 à 320.
- Pour des chiffres fiables : **RAZ**, lire le morceau en entier (ou bounce), revenir sur EXPORT. Remise à zéro automatique au départ de la lecture (réglage).
- CONCERT et CD : repères pratiques (plafond de true peak, fenêtre de sonie du style, sub, mono, PLR, dither 16 bits), **pas des normes**.

## Validation (faite sous Linux, JUCE 8.0.15 / GCC 13)
- Sonie : sinus 997 Hz -23 dBFS stéréo = -23,0 LUFS (M, S, I) à 44,1 / 48 / 96 kHz ; mono gauche 0 dBFS = -3,01 LUFS ; porte relative (20 s à -20 + 20 s à -40 → -20,0) ; LRA 10 LU sur 20 s à -20 / 20 s à -30 ; true peak d'un sinus fs/4 à 45° = 0 dBTP (échantillons à -3 dBFS).
- Spectre : sinus -6 dBFS lu -6,02 dB ; Mid/Side et corrélation corrects sur signaux en phase / en opposition ; 200 colonnes/s de spectrogramme ; fondamentale 52 Hz trouvée à 52,0 Hz.
- ADSR : kick exponentiel (T20 = 139 ms pour 133 attendus) ; nappe A 80 / D 100 / S 600 / R 300 ms retrouvée (S à -6,0 dB, plateau détecté).
- ASan + UBSan propres. Charge mesurée : thread audio ≈ 0,9 % d'un cœur (pire bloc 0,32 ms pour un budget de 10,7 ms), thread d'analyse ≈ 9 % d'un cœur (FFT maison ; vDSP serait plus rapide sur Mac).
- Le `.vst3` construit se charge comme dans un hôte : audio inchangé (écart 0), état sauvegardé/restauré, éditeur créé.
- **Non fait** : compilation macOS, `auval`, essai dans Logic (voir « Build »).

## Limites connues
- L'analyse spectrale tourne dans un thread séparé : en **bounce rapide** (bien plus vite que le temps réel), elle échantillonne ; sonie intégrée, LRA, true peak, corrélation restent **exacts** (thread audio).
- Les repères de style (fenêtres de sonie, courbes de référence, seuils) sont des **heuristiques** générales pour punk pop / emo / hyperpop / alt / techno, pas une analyse d'un artiste précis. À ajuster à l'oreille.
- Détection des coups : sur un mix dense en continu, le « dernier coup » n'a pas de sens (il n'y a pas de transitoire isolé) ; elle est faite pour kick, caisse claire, notes.

## Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target MaskScope_AU MaskScope_VST3
```
Sur macOS : CI GitHub Actions `macos-15` (`.github/workflows/build.yml`) → AU + VST3 universels, signature ad hoc, `mask-scope-1.0.0.pkg`, `auval -v aufx Mscp Mask` (non bloquant).
Sous Linux sans en-têtes X11 de dev : `-DCMAKE_CXX_FLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0"`.

Tests : `g++ -std=c++17 -O2 -pthread Tests/loud_test.cpp` (et `dsp_test`, `coach_test`) ; interface hors écran : cible `EditorTest` (captures PNG) ; hôte VST3 : cible `LoadTest`.
Calibration du codec : `tools/gen_test_signals.py`, `tools/measure_real_codecs.py` (ffmpeg), `tools/calibrate_codec.py`.

## Fichiers
`Source/Dsp/` : `Common`, `Fft`, `Loudness`, `Meters` (thread audio), `Analysis`, `Envelope`, `Codec` (thread d'analyse), `Engine`, `Targets`, `Coach`, `Tips`.
`Source/` : `PluginProcessor`, `PluginEditor` + `Editor*.cpp` (un fichier par module), `SpgRenderer`, `Look`, `UiState`.
