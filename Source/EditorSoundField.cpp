#include "PluginEditor.h"

using namespace mask;
using juce::Colour;
using juce::Justification;
using juce::Rectangle;

void ScopeEditor::drawSoundField (juce::Graphics& g, Rectangle<float> r, bool expanded)
{
    auto body = panel (g, r, juce::String::fromUTF8 ("CHAMP STÉRÉO"), col::violet, TabSoundField, expanded);
    if (expanded)
    {
        auto row = body.removeFromTop (22.0f * u);
        body.removeFromTop (4.0f * u);
        chip (g, row.removeFromLeft (190.0f * u), C_SfMode, RSpCtl);
    }
    Rectangle<float> side;
    if (expanded) { side = body.removeFromRight (body.getWidth() * 0.42f); body.removeFromRight (10.0f * u); }
    else
    {
        side = body.removeFromBottom (body.getHeight() * 0.34f);
        body.removeFromBottom (4.0f * u);
    }
    // ----- vectoscope
    const float sq = std::min (body.getWidth(), body.getHeight());
    auto sc = Rectangle<float> (body.getCentreX() - sq * 0.5f, body.getCentreY() - sq * 0.5f, sq, sq);
    scopeRect = sc;
    constexpr int S = 480;
    if (scopeImg.isNull() || scopeImg.getWidth() != S) { scopeImg = juce::Image (juce::Image::ARGB, S, S, true); const uint64_t w0 = eng.ring.written(); scopeLast = w0 > 4096 ? w0 - 4096 : 0; }
    {
        juce::Graphics gi (scopeImg);
        const bool polar = ui.sfMode == 2;
        gi.setColour (Colour::fromFloatRGBA (0, 0, 0, polar ? 1.0f : 0.20f));
        gi.fillRect (0, 0, S, S);
        const uint64_t w = eng.ring.written();
        uint64_t n = w > scopeLast ? w - scopeLast : 0;
        if (n > 4096) n = 4096;
        scopeLast = w;
        const int stride = std::max<int> (1, (int) (n / 1400));
        for (int k = 0; k < (polar ? 181 : 0); ++k) polarHist[k] *= 0.9f;
        for (uint64_t i = 0; i < n; i += (uint64_t) stride)
        {
            const uint64_t idx = w - n + i;
            const float L = eng.ring.L (idx), R = eng.ring.R (idx);
            if (polar)
            {
                const float mid = 0.5f * (L + R), side_ = 0.5f * (R - L);
                const float th = std::atan2 (side_, std::abs (mid)) + juce::MathConstants<float>::halfPi;     // 0 = G, π/2 = centre, π = D
                const int bin = juce::jlimit (0, 180, (int) std::lround (th * 180.0f / juce::MathConstants<float>::pi));
                polarHist[bin] += (mid * mid + side_ * side_) * 4.0f;
                continue;
            }
            float x, y;
            if (ui.sfMode == 1) { x = L; y = R; } else { x = 0.5f * (R - L); y = 0.5f * (L + R); }
            const float px = (0.5f + 0.47f * juce::jlimit (-1.0f, 1.0f, x)) * (float) S;
            const float py = (0.5f - 0.47f * juce::jlimit (-1.0f, 1.0f, y)) * (float) S;
            const float lvl = juce::jlimit (0.0f, 1.0f, std::sqrt (x * x + y * y));
            gi.setColour (col::violet.interpolatedWith (col::white, lvl).withAlpha (0.85f));
            gi.fillRect (px, py, 2.0f, 2.0f);
        }
    }
    g.setColour (col::bg);
    g.fillRoundedRectangle (sc, 6.0f * u);
    g.saveState();
    g.reduceClipRegion (sc.toNearestInt());
    if (ui.sfMode != 2)
    {
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImage (scopeImg, sc);
    }
    g.restoreState();
    // repères
    {
        const auto c = sc.getCentre();
        const float rad = sq * 0.47f;
        g.setColour (col::line);
        if (ui.sfMode != 2)
        {
            g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
            g.drawEllipse (c.x - rad * 0.5f, c.y - rad * 0.5f, rad, rad, 1.0f);
            g.drawLine (c.x - rad, c.y, c.x + rad, c.y, 1.0f);
            g.drawLine (c.x, c.y - rad, c.x, c.y + rad, 1.0f);
            if (ui.sfMode == 0) { g.setColour (col::line.withAlpha (0.6f)); g.drawLine (c.x - rad * 0.7f, c.y + rad * 0.7f, c.x + rad * 0.7f, c.y - rad * 0.7f, 1.0f); g.drawLine (c.x - rad * 0.7f, c.y - rad * 0.7f, c.x + rad * 0.7f, c.y + rad * 0.7f, 1.0f); }
            const float s = 9.0f;
            if (ui.sfMode == 0) { txt (g, "M", Rectangle<float> (c.x - 10 * u, sc.getY() + 2 * u, 20 * u, 12 * u), s, col::faint, Justification::centred, true); txt (g, "L", Rectangle<float> (sc.getX() + 4 * u, sc.getY() + 4 * u, 14 * u, 12 * u), s, col::faint, Justification::centred, true); txt (g, "R", Rectangle<float> (sc.getRight() - 18 * u, sc.getY() + 4 * u, 14 * u, 12 * u), s, col::faint, Justification::centred, true); }
            else { txt (g, "L", Rectangle<float> (c.x - 10 * u, sc.getBottom() - 14 * u, 20 * u, 12 * u), s, col::faint, Justification::centred, true); txt (g, "R", Rectangle<float> (sc.getRight() - 14 * u, c.y - 6 * u, 12 * u, 12 * u), s, col::faint, Justification::centred, true); }
        }
        else
        {
            // niveau polaire : demi-cercle, gauche → droite
            float mx = 1e-9f; for (float v : polarHist) mx = std::max (mx, v);
            juce::Path p;
            const float rr = sq * 0.46f;
            const float cy = sc.getBottom() - sq * 0.12f;
            p.startNewSubPath (c.x, cy);
            for (int k = 0; k <= 180; ++k)
            {
                const float th = (float) k * juce::MathConstants<float>::pi / 180.0f;
                float v = polarHist[k] / mx;
                v = std::pow (v, 0.45f);
                p.lineTo (c.x - std::cos (th) * rr * v, cy - std::sin (th) * rr * v);
            }
            p.closeSubPath();
            g.setColour (col::violet.withAlpha (0.35f)); g.fillPath (p);
            g.setColour (col::violet); g.strokePath (p, juce::PathStrokeType (1.5f));
            g.setColour (col::line);
            g.drawEllipse (c.x - rr, cy - rr, rr * 2, rr * 2, 1.0f);
            for (int k = 0; k <= 4; ++k) { const float th = (float) k * juce::MathConstants<float>::pi / 4.0f; g.drawLine (c.x, cy, c.x - std::cos (th) * rr, cy - std::sin (th) * rr, 1.0f); }
            txt (g, "L", Rectangle<float> (sc.getX() + 4 * u, cy - 6 * u, 14 * u, 12 * u), 9.0f, col::faint, Justification::centred, true);
            txt (g, "C", Rectangle<float> (c.x - 7 * u, cy - rr - 14 * u, 14 * u, 12 * u), 9.0f, col::faint, Justification::centred, true);
            txt (g, "R", Rectangle<float> (sc.getRight() - 18 * u, cy - 6 * u, 14 * u, 12 * u), 9.0f, col::faint, Justification::centred, true);
        }
    }
    addHit (sc, A_None, 0, RScope, TabSoundField);

    // ----- corrélation, balance, largeur
    auto in = side;
    auto corrBar = in.removeFromTop (expanded ? 26.0f * u : 22.0f * u);
    {
        txt (g, "PHASE", corrBar.removeFromLeft (40.0f * u), 9.0f, col::dim, Justification::centredLeft, true);
        auto val = corrBar.removeFromRight (40.0f * u);
        txt (g, juce::String (m.corr, 2), val, 11.0f, m.corr < 0 ? col::bad : (m.corr < 0.3f ? col::amber : col::mint), Justification::centredRight, true);
        auto bar = corrBar.reduced (0, 6.0f * u);
        g.setGradientFill (juce::ColourGradient (col::bad.withAlpha (0.55f), bar.getX(), 0, col::mint.withAlpha (0.55f), bar.getRight(), 0, false));
        g.fillRoundedRectangle (bar, 3.0f * u);
        const float nx = bar.getX() + (m.corr + 1.0f) * 0.5f * bar.getWidth();
        g.setColour (col::white);
        g.fillRoundedRectangle (nx - 1.5f * u, bar.getY() - 3.0f * u, 3.0f * u, bar.getHeight() + 6.0f * u, 1.5f * u);
        rule (g, bar.getCentreX(), bar.getY(), bar.getCentreX(), bar.getBottom(), col::bg, 1.0f);
        addHit (corrBar, A_None, 0, RCorr);
    }
    in.removeFromTop (3.0f * u);
    auto balRow = in.removeFromTop (expanded ? 22.0f * u : 18.0f * u);
    {
        txt (g, "BAL.", balRow.removeFromLeft (40.0f * u), 9.0f, col::dim, Justification::centredLeft, true);
        txt (g, look::fmtU ("%+.1f dB", m.balanceDb), balRow.removeFromRight (60.0f * u), 10.0f, std::abs (m.balanceDb) > 1.5f ? col::amber : col::text, Justification::centredRight, true);
        auto bar = balRow.reduced (0, 6.0f * u);
        g.setColour (col::panel2);
        g.fillRoundedRectangle (bar, 3.0f * u);
        const float bx = bar.getCentreX() + juce::jlimit (-6.0f, 6.0f, m.balanceDb) / 6.0f * bar.getWidth() * 0.5f;
        g.setColour (col::violet);
        g.fillRoundedRectangle (Rectangle<float> (std::min (bar.getCentreX(), bx), bar.getY(), std::max (2.0f, std::abs (bx - bar.getCentreX())), bar.getHeight()), 2.0f * u);
        addHit (balRow, A_None, 0, RBalance);
    }
    in.removeFromTop (3.0f * u);
    {
        auto fold = in.removeFromTop (expanded ? 20.0f * u : 16.0f * u);
        txt (g, "MONO", fold.removeFromLeft (40.0f * u), 9.0f, col::dim, Justification::centredLeft, true);
        txt (g, look::fmtU ("%.1f dB en mono", m.monoFoldDb), fold, 10.0f, m.monoFoldDb < -3.0f ? col::amber : col::text, Justification::centredLeft, true);
        addHit (fold, A_None, 0, RMonoFold);
    }
    if (expanded)
    {
        in.removeFromTop (6.0f * u);
        // niveaux Mid / Side
        auto ms = in.removeFromTop (44.0f * u);
        for (int k = 0; k < 2; ++k)
        {
            auto row = ms.removeFromTop (20.0f * u);
            txt (g, k == 0 ? "MID" : "SIDE", row.removeFromLeft (40.0f * u), 9.0f, col::dim, Justification::centredLeft, true);
            txt (g, look::fmtDb (k == 0 ? m.midDb : m.sideDb, 1), row.removeFromRight (46.0f * u), 10.0f, col::text, Justification::centredRight, true);
            drawGaugeBar (g, row.reduced (0, 5.0f * u), k == 0 ? m.midDb : m.sideDb, -60.0f, 0.0f, 1000.0f, k == 0 ? col::mint : col::violet, false);
        }
        in.removeFromTop (6.0f * u);
        // historique de corrélation
        auto hist = in.removeFromTop (in.getHeight() * 0.38f);
        txt (g, juce::String::fromUTF8 ("CORRÉLATION · 60 dernières secondes"), hist.removeFromTop (14.0f * u), 8.5f, col::faint, Justification::centredLeft, true);
        g.setColour (col::bg); g.fillRoundedRectangle (hist, 4.0f * u);
        const auto& hs = eng.meters.history;
        const uint64_t N = hs.size();
        if (N > 2)
        {
            juce::Path p;
            const int nn = (int) std::min<uint64_t> (N, 600);
            for (int i = 0; i < nn; ++i)
            {
                const float c = hs.at (N - (uint64_t) nn + (uint64_t) i).corr;
                const float x = hist.getRight() - hist.getWidth() * (float) (nn - 1 - i) / 600.0f;
                const float y = hist.getCentreY() - c * hist.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col::violet); g.strokePath (p, juce::PathStrokeType (1.4f));
        }
        rule (g, hist.getX(), hist.getCentreY(), hist.getRight(), hist.getCentreY(), col::faint);
        in.removeFromTop (6.0f * u);
        // corrélation par zone
        txt (g, juce::String::fromUTF8 ("CORRÉLATION PAR ZONE (idéal : graves mono, aigus plus larges)"), in.removeFromTop (14.0f * u), 8.5f, col::faint, Justification::centredLeft, true);
        const float colW = in.getWidth() / (float) kBands;
        auto bars = in.removeFromTop (in.getHeight() - 14.0f * u);
        for (int b = 0; b < kBands; ++b)
        {
            auto c = Rectangle<float> (bars.getX() + (float) b * colW, bars.getY(), colW, bars.getHeight()).reduced (3.0f * u, 0);
            g.setColour (col::bg); g.fillRoundedRectangle (c, 3.0f * u);
            const float v = a.bandCorr[b];
            const float y = c.getCentreY() - v * c.getHeight() * 0.5f;
            g.setColour (v < 0.0f ? col::bad : (v < 0.3f ? col::amber : col::violet));
            g.fillRect (Rectangle<float> (c.getX() + 2.0f * u, std::min (y, c.getCentreY()), c.getWidth() - 4.0f * u, std::max (1.5f, std::abs (y - c.getCentreY()))));
            rule (g, c.getX(), c.getCentreY(), c.getRight(), c.getCentreY(), col::faint);
            txt (g, bandName (b), Rectangle<float> (c.getX() - 6 * u, bars.getBottom() + 1.0f * u, c.getWidth() + 12 * u, 12.0f * u), 7.0f, col::dim, Justification::centred, true);
            addHit (Rectangle<float> (c.getX(), bars.getY(), c.getWidth(), bars.getHeight() + 13.0f * u), A_None, 0, RWidthBands, -1, b);
        }
    }
}
