#include "PluginEditor.h"

using namespace mask;
using juce::Colour;
using juce::Justification;
using juce::Rectangle;

namespace
{
const float kRanges[][2] = { { -90, -10 }, { -80, 0 }, { -100, -20 }, { -70, -10 }, { -90, 0 } };
const float kTilts[] = { 0.0f, 1.5f, 3.0f, 4.5f };
}

void ScopeEditor::drawSpectrum (juce::Graphics& g, Rectangle<float> r, bool expanded)
{
    auto body = panel (g, r, "SPECTRE", col::cyan, TabSpectrum, expanded);

    // ----- contrôles : courbes affichées + réglages
    {
        auto row = body.removeFromTop (20.0f * u);
        for (int md = 0; md < kModes; ++md)
        {
            auto b = row.removeFromLeft ((expanded ? 52.0f : 44.0f) * u);
            row.removeFromLeft (4.0f * u);
            const bool on = (ui.spModes & (1 << md)) != 0;
            const Colour c = look::modeColour (md);
            g.setColour (on ? c.withAlpha (0.18f) : col::panel2);
            g.fillRoundedRectangle (b, 4.0f * u);
            g.setColour (on ? c : col::line);
            g.drawRoundedRectangle (b.reduced (0.5f), 4.0f * u, 1.0f);
            txt (g, look::modeName (md), b, 9.5f, on ? c : col::faint, Justification::centred, true);
            addHit (b, A_Toggle, C_SpMode, RSpCtl, -1, md);
        }
        if (expanded)
        {
            row.removeFromLeft (6.0f * u);
            auto put = [&] (int ctl, float w) { auto b = row.removeFromLeft (w * u); row.removeFromLeft (5.0f * u); chip (g, b, ctl, RSpCtl); };
            put (C_SpFft, 96); put (C_SpSmooth, 140); put (C_SpSpeed, 164); put (C_SpTilt, 150); put (C_SpRange, 130);
        }
    }
    body.removeFromTop (4.0f * u);
    Rectangle<float> side;
    if (expanded)
    {
        auto row2 = body.removeFromTop (20.0f * u);
        body.removeFromTop (4.0f * u);
        auto put = [&] (int ctl, float w) { auto b = row2.removeFromLeft (w * u); row2.removeFromLeft (5.0f * u); chip (g, b, ctl, RSpCtl); };
        put (C_SpAvg, 120); put (C_SpPeak, 110);
        side = body.removeFromRight (body.getWidth() * 0.28f);
        body.removeFromRight (8.0f * u);
    }

    // ----- tracé
    auto plot = body;
    plot.removeFromLeft (30.0f * u);
    plot.removeFromBottom (14.0f * u);
    specPlot = plot;
    g.setColour (col::bg);
    g.fillRect (plot);
    const float floorDb = kRanges[juce::jlimit (0, 4, ui.spRange)][0], ceilDb = kRanges[juce::jlimit (0, 4, ui.spRange)][1];
    const float tilt = kTilts[juce::jlimit (0, 3, ui.spTilt)];
    const float fmax = haveSb ? sb->fmax : 20000.0f;
    auto X = [&] (float f) { return plot.getX() + look::freqToX (f, (float) kFMin, fmax) * plot.getWidth(); };
    auto Y = [&] (float db) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, (db - floorDb) / (ceilDb - floorDb)) * plot.getHeight(); };
    for (float db = std::ceil (floorDb / 10.0f) * 10.0f; db <= ceilDb; db += 10.0f)
    {
        rule (g, plot.getX(), Y (db), plot.getRight(), Y (db), col::line.withAlpha (0.8f));
        txt (g, juce::String ((int) db), Rectangle<float> (r.getX() + 6.0f * u, Y (db) - 6.0f * u, 26.0f * u, 12.0f * u), 8.0f, col::faint, Justification::centredRight);
    }
    for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        if (f > fmax) continue;
        rule (g, X (f), plot.getY(), X (f), plot.getBottom(), col::line.withAlpha (0.8f));
        txt (g, f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), Rectangle<float> (X (f) - 16.0f * u, plot.getBottom() + 1.0f * u, 32.0f * u, 12.0f * u), 8.0f, col::faint, Justification::centred);
    }
    // zones
    for (int b = 0; b < kBands; ++b)
    {
        const float x0 = X ((float) bandEdges()[b]), x1 = std::min (plot.getRight(), X ((float) std::min<double> (bandEdges()[b + 1], fmax)));
        if (b % 2 == 0) { g.setColour (col::white.withAlpha (0.02f)); g.fillRect (Rectangle<float> (x0, plot.getY(), x1 - x0, plot.getHeight())); }
        if (expanded || b % 2 == 0) txt (g, bandName (b), Rectangle<float> (x0, plot.getY() + 1.0f * u, x1 - x0, 11.0f * u), 7.5f, col::faint, Justification::centred, true);
    }
    if (haveSb)
    {
        bool first = true;
        for (int md = 0; md < kModes; ++md)
        {
            if (! (ui.spModes & (1 << md))) continue;
            const Colour c = look::modeColour (md);
            juce::Path live, avg, peak;
            for (int i = 0; i < kSpecPts; ++i)
            {
                const float f = look::xToFreq ((float) i / (kSpecPts - 1), (float) kFMin, fmax);
                const float tl = tilt * std::log2 (f / 1000.0f);
                const float x = plot.getX() + (float) i / (kSpecPts - 1) * plot.getWidth();
                const float yl = Y (sb->live[md][i] + tl), ya = Y (sb->avg[md][i] + tl), yp = Y (sb->peak[md][i] + tl);
                if (i == 0) { live.startNewSubPath (x, yl); avg.startNewSubPath (x, ya); peak.startNewSubPath (x, yp); }
                else { live.lineTo (x, yl); avg.lineTo (x, ya); peak.lineTo (x, yp); }
            }
            if (first)
            {
                juce::Path fill (live);
                fill.lineTo (plot.getRight(), plot.getBottom());
                fill.lineTo (plot.getX(), plot.getBottom());
                fill.closeSubPath();
                g.setGradientFill (juce::ColourGradient (c.withAlpha (0.30f), 0, plot.getY(), c.withAlpha (0.02f), 0, plot.getBottom(), false));
                g.fillPath (fill);
                first = false;
            }
            if (ui.spPeak)
            {
                juce::Path dashed;
                const float dl[] = { 3.0f, 3.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, peak, dl, 2);
                g.setColour (c.withAlpha (0.55f));
                g.fillPath (dashed);
            }
            if (ui.spAvg) { g.setColour (c.withAlpha (0.5f)); g.strokePath (avg, juce::PathStrokeType (1.0f)); }
            g.setColour (c);
            g.strokePath (live, juce::PathStrokeType (1.6f * u));
        }
        // fondamentale grave
        if (a.subProm > 10.0f && a.subHz >= kFMin)
        {
            const float x = X (a.subHz);
            g.setColour (col::yellow.withAlpha (0.8f));
            g.drawLine (x, plot.getBottom() - 8.0f * u, x, plot.getBottom(), 2.0f);
            int cents = 0; const int mid = noteFromHz (a.subHz, cents);
            txt (g, juce::String ((int) std::lround (a.subHz)) + look::U (" Hz · ") + midiName (mid) + juce::String (midiOctave (mid)), Rectangle<float> (x - 4.0f * u, plot.getBottom() - 22.0f * u, 90.0f * u, 12.0f * u), 8.5f, col::yellow, Justification::centredLeft, true);
            addHit (Rectangle<float> (x - 8.0f * u, plot.getBottom() - 24.0f * u, 100.0f * u, 24.0f * u), A_None, 0, RSpSub);
        }
    }
    g.setColour (col::line);
    g.drawRect (plot, 1.0f);
    if (mouseInside && plot.contains (mouse)) { g.setColour (col::white.withAlpha (0.35f)); g.drawVerticalLine ((int) mouse.x, plot.getY(), plot.getBottom()); }
    addHit (plot, A_None, 0, RSpectrum, TabSpectrum);

    // ----- colonne de droite (vue agrandie)
    if (expanded)
    {
        g.setColour (col::bg);
        g.fillRoundedRectangle (side, 6.0f * u);
        auto in = side.reduced (8.0f * u, 6.0f * u);
        txt (g, look::U ("ÉQUILIBRE TONAL vs repère"), in.removeFromTop (14.0f * u), 9.5f, col::dim, Justification::centredLeft, true);
        txt (g, juce::String (styleAt (ui.style).name), in.removeFromTop (12.0f * u), 8.5f, col::violet, Justification::centredLeft, true);
        in.removeFromTop (4.0f * u);
        auto bars = in.removeFromTop (in.getHeight() * 0.50f);
        drawBandBars (g, bars, false);
        in.removeFromTop (6.0f * u);
        // tableau niveau moyen / part d'énergie
        txt (g, look::U ("BANDE            LIVE   MOY.   ÉNERGIE"), in.removeFromTop (12.0f * u), 8.0f, col::faint, Justification::centredLeft, true);
        const float rowH = std::min (14.0f * u, in.getHeight() / (float) kBands);
        for (int b = 0; b < kBands; ++b)
        {
            auto row = in.removeFromTop (rowH);
            auto c1 = row.removeFromLeft (row.getWidth() * 0.36f);
            txt (g, bandName (b), c1, 8.5f, col::dim, Justification::centredLeft, true);
            auto c2 = row.removeFromLeft (row.getWidth() * 0.28f);
            txt (g, look::fmtDb (a.bandLiveDb[b], 0), c2, 8.5f, col::text, Justification::centredRight);
            auto c3 = row.removeFromLeft (row.getWidth() * 0.40f);
            txt (g, a.sessFrames > 1 ? look::fmtDb (a.bandSessDb[b], 0) : juce::String (look::U ("—")), c3, 8.5f, col::dim, Justification::centredRight);
            txt (g, juce::String ((int) std::lround (a.bandShareSess[b] * 100.0f)) + " %", row, 8.5f, col::dim, Justification::centredRight);
        }
        in.removeFromTop (4.0f * u);
        const float low = (a.bandShareSess[0] + a.bandShareSess[1]) * 100.0f;
        txt (g, juce::String::fromUTF8 ("Sous 150 Hz : ") + juce::String ((int) std::lround (low)) + look::U (" % de l'énergie (inaudible sur téléphone)"), in.removeFromTop (24.0f * u), 8.5f, col::dim, Justification::topLeft);
    }
}
