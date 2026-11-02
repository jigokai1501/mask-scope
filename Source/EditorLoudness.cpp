#include "PluginEditor.h"

using namespace mask;
using juce::Colour;
using juce::Justification;
using juce::Rectangle;

void ScopeEditor::tile (juce::Graphics& g, Rectangle<float> r, const juce::String& label, const juce::String& value, const juce::String& unit, Colour c, int region)
{
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 5.0f * u);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f * u, 1.0f);
    auto in = r.reduced (7.0f * u, 3.0f * u);
    txt (g, label, in.removeFromTop (11.0f * u), 8.0f, col::faint, Justification::centredLeft, true);
    const float size = juce::jlimit (13.0f, 28.0f, r.getHeight() * 0.22f / u);
    const float vw = juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (size * u, juce::Font::bold)), value) + 4.0f * u;
    auto line = in;
    txt (g, value, line.removeFromLeft (vw), size, c, Justification::centredLeft, true);
    txt (g, unit, line, 8.5f, col::dim, Justification::centredLeft);
    addHit (r, A_None, 0, region);
}

static Colour okColour (bool ok, bool warn) { return ok ? col::mint : (warn ? col::amber : col::bad); }

// =================================================================== SONIE
void ScopeEditor::drawLoudness (juce::Graphics& g, Rectangle<float> r, bool expanded)
{
    auto body = panel (g, r, "SONIE", col::amber, TabLoudness, expanded);
    const Style& st = styleAt (ui.style);
    const float T = ui.target == TargetSpotify ? spotifyLufs (ui.spot) : 0.0f;
    const bool hasT = ui.target == TargetSpotify;
    const bool valid = m.lufsI > -100.0f;
    const float delta = valid && hasT ? m.lufsI - T : 0.0f;
    const Colour dc = ! hasT ? col::dim : (std::abs (delta) <= 1.0f ? col::mint : (delta > 0 ? col::amber : col::blue));

    auto bigInt = [&] (Rectangle<float> rc, float sz)
    {
        txt (g, look::U ("INTÉGRÉE"), rc.removeFromTop (12.0f * u), 8.5f, col::faint, Justification::centredLeft, true);
        txt (g, valid ? juce::String (m.lufsI, 1) : juce::String (look::U ("—")), rc.removeFromTop (sz * 1.25f * u), sz, col::text, Justification::centredLeft, true);
        txt (g, "LUFS", rc, 9.0f, col::dim, Justification::centredLeft, true);
        addHit (rc.withHeight (sz * 1.8f * u), A_None, 0, RLdI);
    };

    if (! expanded)
    {
        auto top = body.removeFromTop (58.0f * u);
        auto left = top.removeFromLeft (top.getWidth() * 0.52f);
        bigInt (left, 24.0f);
        auto right = top.reduced (2.0f * u, 6.0f * u);
        g.setColour (dc.withAlpha (0.14f)); g.fillRoundedRectangle (right, 5.0f * u);
        g.setColour (dc); g.drawRoundedRectangle (right.reduced (0.5f), 5.0f * u, 1.0f);
        txt (g, hasT && valid ? look::fmtU ("%+.1f LU", delta) : juce::String (look::U ("—")), right.removeFromTop (right.getHeight() * 0.55f), 13.0f, dc, Justification::centred, true);
        txt (g, hasT ? juce::String ("vs ") + targetName (ui.target) + " " + juce::String ((int) T) : juce::String ("pas de cible"), right, 8.5f, col::dim, Justification::centred);
        addHit (right, A_None, 0, RLdI);
        body.removeFromTop (4.0f * u);
        for (int k = 0; k < 2; ++k)
        {
            auto row = body.removeFromTop (17.0f * u);
            txt (g, k == 0 ? "M" : "S", row.removeFromLeft (14.0f * u), 9.5f, col::dim, Justification::centredLeft, true);
            auto val = row.removeFromRight (46.0f * u);
            const float v = k == 0 ? m.lufsM : m.lufsS;
            txt (g, look::fmtDb (v, 1), val, 10.5f, col::text, Justification::centredRight, true);
            drawGaugeBar (g, row.reduced (0, 4.0f * u), v, -50.0f, 0.0f, hasT ? T : 1000.0f, k == 0 ? col::amber.withAlpha (0.7f) : col::amber, false);
            addHit (row, A_None, 0, k == 0 ? RLdM : RLdS);
        }
        body.removeFromTop (4.0f * u);
        const float tw = body.getWidth() / 2.0f, th = body.getHeight() / 2.0f;
        auto t1 = Rectangle<float> (body.getX(), body.getY(), tw, th).reduced (2.0f * u);
        tile (g, t1, "LRA", m.lra > 0 ? juce::String (m.lra, 1) : juce::String (look::U ("—")), "LU", col::text, RLdLra);
        tile (g, Rectangle<float> (body.getX() + tw, body.getY(), tw, th).reduced (2.0f * u), "TRUE PEAK MAX", look::fmtDb (m.tpMax, 1), "dBTP", okColour (m.tpMax <= tpCeiling (ui.target, m.lufsI), m.tpMax <= 0), RLdTp);
        tile (g, Rectangle<float> (body.getX(), body.getY() + th, tw, th).reduced (2.0f * u), "PLR", m.plr > 0 ? juce::String (m.plr, 1) : juce::String (look::U ("—")), "dB", okColour (m.plr >= st.plrMin || m.plr <= 0, m.plr >= st.plrMin - 1.5f), RLdPlr);
        tile (g, Rectangle<float> (body.getX() + tw, body.getY() + th, tw, th).reduced (2.0f * u), "PSR", m.psr > 0 ? juce::String (m.psr, 1) : juce::String (look::U ("—")), "dB", col::text, RLdPsr);
        return;
    }

    // ----- vue agrandie
    auto left = body.removeFromLeft (body.getWidth() * 0.22f);
    body.removeFromLeft (8.0f * u);
    auto right = body.removeFromRight (body.getWidth() * 0.42f);
    body.removeFromRight (8.0f * u);
    // barres verticales M / S
    {
        g.setColour (col::bg); g.fillRoundedRectangle (left, 6.0f * u);
        auto in = left.reduced (10.0f * u, 8.0f * u);
        auto labels = in.removeFromBottom (36.0f * u);
        auto scale = in.removeFromLeft (26.0f * u);
        const float lo = -50.0f, hi = 0.0f;
        auto Yv = [&] (float v) { return in.getBottom() - juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo)) * in.getHeight(); };
        for (float v = lo; v <= hi; v += 10.0f)
        {
            txt (g, juce::String ((int) v), Rectangle<float> (scale.getX(), Yv (v) - 6.0f * u, scale.getWidth(), 12.0f * u), 8.0f, col::faint, Justification::centredRight);
            rule (g, in.getX(), Yv (v), in.getRight(), Yv (v), col::line);
        }
        const float bw = in.getWidth() / 2.0f;
        for (int k = 0; k < 2; ++k)
        {
            auto b = Rectangle<float> (in.getX() + (float) k * bw, in.getY(), bw, in.getHeight()).reduced (6.0f * u, 0);
            const float v = k == 0 ? m.lufsM : m.lufsS;
            g.setColour (col::panel2); g.fillRoundedRectangle (b, 3.0f * u);
            g.setColour (k == 0 ? col::amber.withAlpha (0.65f) : col::amber);
            g.fillRoundedRectangle (Rectangle<float> (b.getX(), Yv (v), b.getWidth(), b.getBottom() - Yv (v)), 3.0f * u);
            const float mx = k == 0 ? m.lufsMmax : m.lufsSmax;
            if (mx > -100.0f) { rule (g, b.getX(), Yv (mx), b.getRight(), Yv (mx), col::white, 1.5f); }
            addHit (b, A_None, 0, k == 0 ? RLdM : RLdS);
            txt (g, k == 0 ? "M" : "S", Rectangle<float> (b.getX(), labels.getY(), b.getWidth(), 14.0f * u), 9.0f, col::dim, Justification::centred, true);
            txt (g, look::fmtDb (v, 1), Rectangle<float> (b.getX() - 6 * u, labels.getY() + 14.0f * u, b.getWidth() + 12 * u, 16.0f * u), 11.0f, col::text, Justification::centred, true);
        }
        if (hasT) { rule (g, in.getX(), Yv (T), in.getRight(), Yv (T), col::mint, 1.5f); txt (g, juce::String ((int) T), Rectangle<float> (in.getRight() - 30 * u, Yv (T) - 12 * u, 30 * u, 11 * u), 8.0f, col::mint, Justification::centredRight, true); }
        addHit (left, A_None, 0, RLdBar);
    }
    // distribution
    {
        g.setColour (col::bg); g.fillRoundedRectangle (body, 6.0f * u);
        auto in = body.reduced (10.0f * u, 8.0f * u);
        txt (g, "DISTRIBUTION DU COURT TERME (3 s)", in.removeFromTop (14.0f * u), 9.0f, col::dim, Justification::centredLeft, true);
        auto labels = in.removeFromBottom (14.0f * u);
        auto plot = in;
        float mx = 1.0f; for (int i = 0; i < kHistBins; ++i) mx = std::max (mx, m.hist[i]);
        auto X = [&] (float v) { return plot.getX() + (v - (float) kHistLo) / (50.0f) * plot.getWidth(); };
        // fenêtre de style
        g.setColour (col::violet.withAlpha (0.10f));
        g.fillRect (Rectangle<float> (X (st.lufsLo), plot.getY(), X (st.lufsHi) - X (st.lufsLo), plot.getHeight()));
        const float bw = plot.getWidth() / (float) kHistBins;
        for (int i = 0; i < kHistBins; ++i)
        {
            const float h = m.hist[i] / mx * plot.getHeight();
            g.setColour (col::amber.withAlpha (0.8f));
            g.fillRect (Rectangle<float> (plot.getX() + (float) i * bw, plot.getBottom() - h, std::max (1.0f, bw - 0.5f), h));
        }
        for (float v = -50.0f; v <= 0.0f; v += 10.0f)
        {
            rule (g, X (v), plot.getY(), X (v), plot.getBottom(), col::line);
            txt (g, juce::String ((int) v), Rectangle<float> (X (v) - 14 * u, labels.getY(), 28 * u, 12 * u), 8.0f, col::faint, Justification::centred);
        }
        if (valid) { rule (g, X (m.lufsI), plot.getY(), X (m.lufsI), plot.getBottom(), col::white, 1.5f); txt (g, "I", Rectangle<float> (X (m.lufsI) + 2 * u, plot.getY(), 12 * u, 12 * u), 9.0f, col::white, Justification::centredLeft, true); }
        if (hasT) { rule (g, X (T), plot.getY(), X (T), plot.getBottom(), col::mint, 1.5f); txt (g, targetName (ui.target), Rectangle<float> (X (T) + 2 * u, plot.getY() + 12 * u, 60 * u, 12 * u), 8.5f, col::mint, Justification::centredLeft, true); }
        txt (g, juce::String (look::U ("repère ")) + st.name + " " + juce::String ((int) st.lufsLo) + look::U (" … ") + juce::String ((int) st.lufsHi), Rectangle<float> (plot.getX() + 4 * u, plot.getBottom() - 14 * u, 220 * u, 12 * u), 8.0f, col::violet, Justification::centredLeft);
        addHit (plot, A_None, 0, RLdHist);
    }
    // tuiles
    {
        const float cw = right.getWidth() / 2.0f, rh = right.getHeight() / 5.0f;
        auto cell = [&] (int cx, int ry) { return Rectangle<float> (right.getX() + (float) cx * cw, right.getY() + (float) ry * rh, cw, rh).reduced (2.0f * u); };
        tile (g, cell (0, 0), look::U ("INTÉGRÉE"), valid ? juce::String (m.lufsI, 1) : juce::String (look::U ("—")), "LUFS", col::text, RLdI);
        tile (g, cell (1, 0), hasT ? juce::String (look::U ("ÉCART ")) + targetName (ui.target) : juce::String (look::U ("ÉCART CIBLE")), hasT && valid ? look::fmtU ("%+.1f", delta) : juce::String (look::U ("—")), "LU", dc, RLdI);
        tile (g, cell (0, 1), "COURT TERME MAX", look::fmtDb (m.lufsSmax, 1), "LUFS", col::text, RLdS);
        tile (g, cell (1, 1), look::U ("MOMENTANÉE MAX"), look::fmtDb (m.lufsMmax, 1), "LUFS", col::text, RLdM);
        tile (g, cell (0, 2), "PLAGE (LRA)", m.lra > 0 ? juce::String (m.lra, 1) : juce::String (look::U ("—")), "LU", col::text, RLdLra);
        tile (g, cell (1, 2), "PORTE RELATIVE", juce::String (m.gateLevel, 1), "LUFS", col::dim, RLdI);
        tile (g, cell (0, 3), "TRUE PEAK G / D", look::fmtDb (m.tpL, 1) + " / " + look::fmtDb (m.tpR, 1), "", col::text, RLdTp);
        tile (g, cell (1, 3), "TRUE PEAK MAX", look::fmtDb (m.tpMax, 1), "dBTP", okColour (m.tpMax <= tpCeiling (ui.target, m.lufsI), m.tpMax <= 0), RLdTp);
        tile (g, cell (0, 4), "PLR", m.plr > 0 ? juce::String (m.plr, 1) : juce::String (look::U ("—")), "dB", okColour (m.plr >= st.plrMin || m.plr <= 0, m.plr >= st.plrMin - 1.5f), RLdPlr);
        tile (g, cell (1, 4), "PSR (3 s)", m.psr > 0 ? juce::String (m.psr, 1) : juce::String (look::U ("—")), "dB", col::text, RLdPsr);
    }
}

// =================================================================== HISTORIQUE
void ScopeEditor::drawHistory (juce::Graphics& g, Rectangle<float> r, bool expanded)
{
    auto body = panel (g, r, "HISTORIQUE", col::blue, TabHistory, expanded);
    if (expanded)
    {
        auto row = body.removeFromTop (22.0f * u);
        body.removeFromTop (4.0f * u);
        chip (g, row.removeFromLeft (150.0f * u), C_HistSpan, RSpCtl);
    }
    const auto& hs = eng.meters.history;
    const uint64_t N = hs.size();
    const uint64_t first = hs.first();
    static const int spans[] = { 600, 3000, 9000, 0 };
    int spanPts = spans[juce::jlimit (0, 3, ui.histSpan)];
    if (spanPts == 0) spanPts = (int) std::max<uint64_t> (600, N - first);
    if (! expanded) spanPts = 600;
    const float T = ui.target == TargetSpotify ? spotifyLufs (ui.spot) : 1000.0f;
    const Style& st = styleAt (ui.style);

    const int panes = expanded ? 3 : 1;
    const float heights[3] = { expanded ? 0.52f : 1.0f, 0.24f, 0.24f };
    const char* names[3] = { "SONIE (LUFS)", "TRUE PEAK (dBTP)", "CORRÉLATION" };
    const float los[3] = { -45.0f, -20.0f, -1.0f }, his[3] = { 0.0f, 3.0f, 1.0f };
    auto whole = body;
    whole.removeFromBottom (14.0f * u);
    float yy = whole.getY();
    for (int p = 0; p < panes; ++p)
    {
        auto pane = Rectangle<float> (whole.getX(), yy, whole.getWidth(), whole.getHeight() * heights[p] - (p < panes - 1 ? 6.0f * u : 0.0f));
        yy += whole.getHeight() * heights[p];
        auto plot = pane.withTrimmedLeft (30.0f * u);
        g.setColour (col::bg); g.fillRect (plot);
        if (expanded) txt (g, names[p], plot.withHeight (12.0f * u).withTrimmedLeft (4.0f * u), 8.0f, col::faint, Justification::centredLeft, true);
        const float lo = los[p], hi = his[p];
        auto Y = [&] (float v) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo)) * plot.getHeight(); };
        const float stepDb = p == 2 ? 0.5f : 10.0f;
        for (float v = std::ceil (lo / stepDb) * stepDb; v <= hi; v += stepDb)
        {
            rule (g, plot.getX(), Y (v), plot.getRight(), Y (v), col::line);
            txt (g, p == 2 ? juce::String (v, 1) : juce::String ((int) v), Rectangle<float> (pane.getX(), Y (v) - 6.0f * u, 28.0f * u, 12.0f * u), 8.0f, col::faint, Justification::centredRight);
        }
        if (p == 0)
        {
            g.setColour (col::violet.withAlpha (0.08f));
            g.fillRect (Rectangle<float> (plot.getX(), Y (st.lufsHi), plot.getWidth(), Y (st.lufsLo) - Y (st.lufsHi)));
            if (T < 100.0f) { rule (g, plot.getX(), Y (T), plot.getRight(), Y (T), col::mint, 1.2f); txt (g, targetName (ui.target), Rectangle<float> (plot.getRight() - 70 * u, Y (T) - 12 * u, 68 * u, 11 * u), 8.0f, col::mint, Justification::centredRight, true); }
        }
        if (p == 1)
        {
            const float c = tpCeiling (ui.target, m.lufsI);
            rule (g, plot.getX(), Y (c), plot.getRight(), Y (c), col::amber, 1.0f);
            rule (g, plot.getX(), Y (0.0f), plot.getRight(), Y (0.0f), col::bad.withAlpha (0.6f), 1.0f);
        }
        if (p == 2) rule (g, plot.getX(), Y (0.0f), plot.getRight(), Y (0.0f), col::faint, 1.0f);
        // courbes
        const int px = std::max (2, (int) plot.getWidth());
        const float per = (float) spanPts / (float) px;
        juce::Path pm, ps, pt;
        bool started = false;
        for (int x = 0; x < px; ++x)
        {
            const int64_t i0 = (int64_t) N - (int64_t) spanPts + (int64_t) std::floor ((float) x * per);
            const int64_t i1 = std::max<int64_t> (i0 + 1, (int64_t) N - (int64_t) spanPts + (int64_t) std::floor ((float) (x + 1) * per));
            double sm = 0, ss = 0, sc = 0; float tp = -120.0f; int cnt = 0;
            for (int64_t i = i0; i < i1; ++i)
            {
                if (i < (int64_t) first || i >= (int64_t) N) continue;
                const HistPoint hp = hs.at ((uint64_t) i);
                sm += hp.m; ss += hp.s; sc += hp.corr; tp = std::max (tp, hp.tp); ++cnt;
            }
            if (cnt == 0) { started = false; continue; }
            const float xx = plot.getX() + (float) x;
            const float vm = (float) (sm / cnt), vs = (float) (ss / cnt), vc = (float) (sc / cnt);
            auto add = [&] (juce::Path& pp, float y) { if (! started) pp.startNewSubPath (xx, y); else pp.lineTo (xx, y); };
            if (p == 0) { add (pm, Y (vm)); add (ps, Y (vs)); }
            else if (p == 1) add (pt, Y (tp));
            else add (pt, Y (vc));
            started = true;
        }
        if (p == 0)
        {
            g.setColour (col::blue.withAlpha (0.45f)); g.strokePath (pm, juce::PathStrokeType (1.0f));
            g.setColour (col::blue); g.strokePath (ps, juce::PathStrokeType (1.8f * u));
        }
        else { g.setColour (p == 1 ? col::amber : col::violet); g.strokePath (pt, juce::PathStrokeType (1.4f * u)); }
        g.setColour (col::line); g.drawRect (plot, 1.0f);
        if (mouseInside && plot.contains (mouse)) { g.setColour (col::white.withAlpha (0.35f)); g.drawVerticalLine ((int) mouse.x, plot.getY(), plot.getBottom()); }
        if (p == 0)
        {
            histPlot = plot; histT0 = 0; histT1 = (float) spanPts / 10.0f; histFirst = first; histLast = N > 0 ? N - 1 : 0;
            addHit (plot, A_None, 0, RHistory, TabHistory);
        }
        else addHit (plot, A_None, 0, RHistory, TabHistory);
    }
    // axe du temps
    {
        auto ax = Rectangle<float> (whole.getX() + 30.0f * u, whole.getBottom() + 1.0f * u, whole.getWidth() - 30.0f * u, 12.0f * u);
        const float totalS = (float) spanPts / 10.0f;
        for (int k = 0; k <= 4; ++k)
        {
            const float s = totalS * (1.0f - (float) k / 4.0f);
            const juce::String lab = k == 4 ? juce::String ("0") : "-" + (s >= 60 ? juce::String ((int) s / 60) + "'" + (((int) s % 60) ? juce::String ((int) s % 60) : juce::String()) : juce::String ((int) s) + "s");
            const float x = ax.getX() + ax.getWidth() * (float) k / 4.0f;
            const auto lr = k == 0 ? Rectangle<float> (x, ax.getY(), 50 * u, 12 * u) : (k == 4 ? Rectangle<float> (x - 50 * u, ax.getY(), 50 * u, 12 * u) : Rectangle<float> (x - 25 * u, ax.getY(), 50 * u, 12 * u));
            txt (g, lab, lr, 8.0f, col::faint, k == 0 ? Justification::centredLeft : (k == 4 ? Justification::centredRight : Justification::centred));
        }
    }
}

// =================================================================== NIVEAUX
void ScopeEditor::drawLevels (juce::Graphics& g, Rectangle<float> r, bool expanded)
{
    auto body = panel (g, r, "NIVEAUX", col::pink, TabLevels, expanded);
    const float lo = -60.0f, hi = 0.0f;
    const float rowH = expanded ? 34.0f * u : 20.0f * u;
    const float rms[2] = { m.rmsL, m.rmsR };
    for (int c = 0; c < 2; ++c)
    {
        auto row = body.removeFromTop (rowH);
        body.removeFromTop (3.0f * u);
        txt (g, c == 0 ? "L" : "R", row.removeFromLeft (14.0f * u), 10.0f, col::dim, Justification::centredLeft, true);
        auto vals = row.removeFromRight ((expanded ? 120.0f : 84.0f) * u);
        auto bar = row.reduced (0, expanded ? 6.0f * u : 3.0f * u);
        g.setColour (col::bg); g.fillRoundedRectangle (bar, 3.0f * u);
        auto X = [&] (float v) { return bar.getX() + juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo)) * bar.getWidth(); };
        g.setColour (col::pink.withAlpha (0.85f));
        g.fillRoundedRectangle (Rectangle<float> (bar.getX(), bar.getY() + bar.getHeight() * 0.25f, X (rms[c]) - bar.getX(), bar.getHeight() * 0.5f), 2.0f * u);
        g.setColour (col::pink.withAlpha (0.35f));
        g.fillRect (Rectangle<float> (bar.getX(), bar.getY(), X (dispPk[c]) - bar.getX(), bar.getHeight() * 0.25f));
        rule (g, X (dispPk[c]), bar.getY(), X (dispPk[c]), bar.getBottom(), col::white, 1.5f);
        const float tpc = tpCeiling (ui.target, m.lufsI);
        if (ui.target != TargetOff) rule (g, X (tpc), bar.getY() - 1, X (tpc), bar.getBottom() + 1, col::amber.withAlpha (0.8f), 1.0f);
        txt (g, look::fmtDb (c == 0 ? m.peakL : m.peakR, 1) + " / " + look::fmtDb (rms[c], 1), vals, expanded ? 10.5f : 9.0f, col::text, Justification::centredRight, true);
        addHit (row, A_None, 0, RLvPeak);
        addHit (vals, A_None, 0, RLvRms);
    }
    if (expanded)
    {
        // graduation
        auto ax = body.removeFromTop (14.0f * u);
        ax.removeFromLeft (14.0f * u); ax.removeFromRight (120.0f * u);
        for (float v = lo; v <= hi; v += 10.0f)
            txt (g, juce::String ((int) v), Rectangle<float> (ax.getX() + (v - lo) / (hi - lo) * ax.getWidth() - 14 * u, ax.getY(), 28 * u, 12 * u), 8.0f, col::faint, Justification::centred);
        body.removeFromTop (8.0f * u);
        const float cw = body.getWidth() / 4.0f, rh = std::min (body.getHeight() / 2.0f, 118.0f * u);
        auto cell = [&] (int cx, int ry) { return Rectangle<float> (body.getX() + (float) cx * cw, body.getY() + (float) ry * rh, cw, rh).reduced (3.0f * u); };
        tile (g, cell (0, 0), look::U ("CRÊTE MAX"), look::fmtDb (m.peakMax, 1), "dBFS", m.peakMax >= -0.1f ? col::bad : col::text, RLvPeak);
        tile (g, cell (1, 0), "MARGE (HEADROOM)", m.peakMax > -100 ? juce::String (-m.peakMax, 1) : juce::String (look::U ("—")), "dB", col::text, RLvPeak);
        tile (g, cell (2, 0), look::U ("FACTEUR DE CRÊTE"), m.crest3s > 0 ? juce::String (m.crest3s, 1) : juce::String (look::U ("—")), "dB", m.crest3s > 0 && m.crest3s < 5.5f ? col::amber : col::text, RLvCrest);
        tile (g, cell (3, 0), "RMS 3 s", look::fmtDb (m.rms3s, 1), "dBFS", col::text, RLvRms);
        tile (g, cell (0, 1), look::U ("DÉCALAGE DC"), look::fmtDb (m.dcDb, 0), "dBFS", m.dcDb > -60 ? col::amber : col::text, RLvDc);
        tile (g, cell (1, 1), juce::String::fromUTF8 ("ÉCRÊTAGES"), juce::String ((int) m.clipCount), "", m.clipCount ? col::bad : col::mint, RLvClip);
        tile (g, cell (2, 1), look::U ("DÉPASS. TRUE PEAK"), juce::String ((int) m.tpOverCount), "", m.tpOverCount ? col::bad : col::mint, RLvClip);
        tile (g, cell (3, 1), look::U ("DURÉE MESURÉE"), look::fmtU ("%d:%02d", (int) m.measuredSec / 60, (int) m.measuredSec % 60), "", col::text, RExVerdict);
        auto legend = Rectangle<float> (body.getX(), body.getY() + rh * 2.0f + 14.0f * u, body.getWidth(), 80.0f * u);
        para (g, look::U ("Barre pleine : RMS sur 300 ms · trait blanc : crête avec retombée (24 dB/s) · trait orange : plafond de true peak conseillé pour le support choisi.\nPlus le facteur de crête est faible (< 6 dB), plus le signal est écrasé. Un décalage DC > -60 dBFS mange du headroom : ajoute un passe-haut 10–20 Hz. Les écrêtages comptent chaque événement où un échantillon atteint 0 dBFS."), legend, 10.0f, col::dim, 5);
    }
    else
    {
        body.removeFromTop (2.0f * u);
        txt (g, juce::String::fromUTF8 ("CRÊTE ") + (m.crest3s > 0 ? juce::String (m.crest3s, 1) : juce::String (look::U ("—"))) + look::U (" dB  ·  DC ") + look::fmtDb (m.dcDb, 0) + look::U ("  ·  ÉCR. ") + juce::String ((int) m.clipCount), body.removeFromTop (14.0f * u), 9.0f, m.clipCount ? col::bad : col::dim, Justification::centredLeft, true);
        addHit (body.withY (body.getY() - 14.0f * u).withHeight (14.0f * u), A_None, 0, RLvCrest);
    }
}
