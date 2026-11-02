// mask::scope — réglages d'interface (thread message uniquement), sauvegardés avec le projet
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/Targets.h"

struct UiState
{
    int tab = mask::TabOverview;
    int target = mask::TargetSpotify, spot = mask::SpotNormal, style = 4;
    bool tips = true, bpmAuto = true;
    int dwellMs = 450;
    double bpmManual = 0.0;
    // spectrogramme
    int spgSource = 0, spgTime = 4, spgRange = 0, spgTilt = 1, spgPalette = 0;
    // spectre
    int spModes = 1;             // bits : ST, MID, SIDE, L, R
    int spTilt = 0, spRange = 4, spAvg = 1, spPeak = 1;
    // champ stéréo et historique
    int sfMode = 0, histSpan = 1;
    // export
    int exCodec = 0;

    void save (juce::XmlElement& x) const
    {
        x.setAttribute ("tab", tab); x.setAttribute ("target", target); x.setAttribute ("spot", spot); x.setAttribute ("style", style);
        x.setAttribute ("tips", tips); x.setAttribute ("bpmAuto", bpmAuto); x.setAttribute ("dwell", dwellMs); x.setAttribute ("bpm", bpmManual);
        x.setAttribute ("spgSource", spgSource); x.setAttribute ("spgTime", spgTime); x.setAttribute ("spgRange", spgRange);
        x.setAttribute ("spgTilt", spgTilt); x.setAttribute ("spgPalette", spgPalette);
        x.setAttribute ("spModes", spModes); x.setAttribute ("spTilt", spTilt); x.setAttribute ("spRange", spRange);
        x.setAttribute ("spAvg", spAvg); x.setAttribute ("spPeak", spPeak);
        x.setAttribute ("sfMode", sfMode); x.setAttribute ("histSpan", histSpan); x.setAttribute ("exCodec", exCodec);
    }
    void load (const juce::XmlElement& x)
    {
        tab = x.getIntAttribute ("tab", tab); target = x.getIntAttribute ("target", target); spot = x.getIntAttribute ("spot", spot);
        style = x.getIntAttribute ("style", style); tips = x.getBoolAttribute ("tips", tips); bpmAuto = x.getBoolAttribute ("bpmAuto", bpmAuto);
        dwellMs = x.getIntAttribute ("dwell", dwellMs); bpmManual = x.getDoubleAttribute ("bpm", bpmManual);
        spgSource = x.getIntAttribute ("spgSource", spgSource); spgTime = x.getIntAttribute ("spgTime", spgTime);
        spgRange = x.getIntAttribute ("spgRange", spgRange); spgTilt = x.getIntAttribute ("spgTilt", spgTilt);
        spgPalette = x.getIntAttribute ("spgPalette", spgPalette);
        spModes = x.getIntAttribute ("spModes", spModes); spTilt = x.getIntAttribute ("spTilt", spTilt); spRange = x.getIntAttribute ("spRange", spRange);
        spAvg = x.getIntAttribute ("spAvg", spAvg); spPeak = x.getIntAttribute ("spPeak", spPeak);
        sfMode = x.getIntAttribute ("sfMode", sfMode); histSpan = x.getIntAttribute ("histSpan", histSpan); exCodec = x.getIntAttribute ("exCodec", exCodec);
        tab = juce::jlimit (0, mask::kTabs - 1, tab);
    }
};
