#include "PluginEditor.h"

using namespace mask;
using juce::Colour;
using juce::Justification;
using juce::Rectangle;

namespace
{
const float kTimes[] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 15.0f, 30.0f };
const float kRanges[][2] = { { -90, -10 }, { -80, 0 }, { -100, -20 }, { -70, -10 }, { -90, 0 } };
const float kTilts[] = { 0.0f, 1.5f, 3.0f, 4.5f };
Colour segColour (char role)
{
    switch (role) { case 'A': return col::pink; case 'D': return col::amber; case 'S': return col::mint; default: return col::blue; }
}
}

// ---------------------------------------------------------------- barres d'équilibre tonal (8 zones)
void ScopeEditor::drawBandBars (juce::Graphics& g, Rectangle<float> r, bool vertical)
{
    const Style& st = styleAt (ui.style);
    float dev[kBands];
    bandDeviation (a.bandLiveDb, st, dev);
    const float range = 14.0f;
    if (! vertical)
    {
        const float rowH = r.getHeight() / (float) kBands;
        for (int b = 0; b < kBands; ++b)
        {
            auto row = Rectangle<float> (r.getX(), r.getY() + (float) b * rowH, r.getWidth(), rowH).reduced (0, 1.5f * u);
            auto label = row.removeFromLeft (row.getWidth() * 0.30f);
            auto val = row.removeFromRight (row.getWidth() * 0.20f);
            txt (g, bandName (b), label, 9.0f, col::dim, Justification::centredLeft, true);
            // zone de tolérance et barre
            g.setColour (col::panel2);
            g.fillRoundedRectangle (row, 3.0f * u);
            const float x0 = row.getX() + row.getWidth() * (0.5f + st.lo[b] / (2 * range));
            const float x1 = row.getX() + row.getWidth() * (0.5f + st.hi[b] / (2 * range));
            g.setColour (col::mint.withAlpha (0.10f));
            g.fillRect (Rectangle<float> (juce::jmax (row.getX(), x0), row.getY(), juce::jmin (row.getRight(), x1) - juce::jmax (row.getX(), x0), row.getHeight()));
            const float d = juce::jlimit (-range, range, dev[b]);
            const float cx = row.getCentreX();
            const float vx = cx + d / (2 * range) * row.getWidth();
            const bool bad = dev[b] > st.hi[b] || dev[b] < st.lo[b];
            const Colour c = bad ? col::amber : col::mint;
            g.setColour (a.bandShareLive[b] > 1e-5f ? c : col::faint);
            g.fillRoundedRectangle (Rectangle<float> (std::min (cx, vx), row.getY() + 2.0f * u, std::max (1.5f, std::abs (vx - cx)), row.getHeight() - 4.0f * u), 2.0f * u);
            rule (g, cx, row.getY(), cx, row.getBottom(), col::faint.withAlpha (0.8f));
            txt (g, a.bandShareLive[b] > 1e-5f ? juce::String (dev[b], 1) : juce::String (look::U ("—")), val, 9.0f, bad ? col::amber : col::text, Justification::centredRight, true);
            addHit (Rectangle<float> (r.getX(), r.getY() + (float) b * rowH, r.getWidth(), rowH), A_None, 0, RSpBands, -1, b);
        }
    }
    else
    {
        const float colW = r.getWidth() / (float) kBands;
        for (int b = 0; b < kBands; ++b)
        {
            auto c = Rectangle<float> (r.getX() + (float) b * colW, r.getY(), colW, r.getHeight()).reduced (3.0f * u, 0);
            auto label = c.removeFromBottom (14.0f * u);
            txt (g, bandName (b), label, 8.0f, col::dim, Justification::centred, true);
            g.setColour (col::panel2);
            g.fillRoundedRectangle (c, 3.0f * u);
            const float y0 = c.getCentreY() - st.hi[b] / (2 * range) * c.getHeight();
            const float y1 = c.getCentreY() - st.lo[b] / (2 * range) * c.getHeight();
            g.setColour (col::mint.withAlpha (0.10f));
            g.fillRect (Rectangle<float> (c.getX(), juce::jmax (c.getY(), y0), c.getWidth(), juce::jmin (c.getBottom(), y1) - juce::jmax (c.getY(), y0)));
            const float d = juce::jlimit (-range, range, dev[b]);
            const float vy = c.getCentreY() - d / (2 * range) * c.getHeight();
            const bool bad = dev[b] > st.hi[b] || dev[b] < st.lo[b];
            g.setColour (a.bandShareLive[b] > 1e-5f ? (bad ? col::amber : col::mint) : col::faint);
            g.fillRoundedRectangle (Rectangle<float> (c.getX() + 3.0f * u, std::min (c.getCentreY(), vy), c.getWidth() - 6.0f * u, std::max (1.5f, std::abs (vy - c.getCentreY()))), 2.0f * u);
            rule (g, c.getX(), c.getCentreY(), c.getRight(), c.getCentreY(), col::faint.withAlpha (0.8f));
            addHit (Rectangle<float> (r.getX() + (float) b * colW, r.getY(), colW, r.getHeight()), A_None, 0, RSpBands, -1, b);
        }
    }
}

// ---------------------------------------------------------------- panneau ADSR du dernier coup
void ScopeEditor::drawHitPanel (juce::Graphics& g, Rectangle<float> r, bool expanded)
{
    hitRect = r;
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 6.0f * u);
    addHit (r, A_None, 0, RSpgHit);
    auto in = r.reduced (8.0f * u, 6.0f * u);
    txt (g, look::U ("DERNIER COUP · A D S R"), in.removeFromTop (14.0f * u), 9.5f, col::dim, Justification::centredLeft, true);
    if (! haveHitDetail)
    {
        txt (g, look::U ("En attente d'un coup (kick, caisse claire, note…)"), in, 10.0f, col::faint, Justification::centred);
        return;
    }
    const HitSummary& hs = hitDetail.s;
    // cases de valeurs
    auto boxes = in.removeFromBottom (expanded ? 76.0f * u : 44.0f * u);
    const char roles[4] = { 'A', 'D', 'S', 'R' };
    const float vals[4] = { hs.aMs, hs.dMs, hs.sMs, hs.rMs };
    const bool present[4] = { true, hs.hasD != 0, hs.hasS != 0, hs.hasR != 0 };
    const float bw = boxes.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto b = Rectangle<float> (boxes.getX() + (float) i * bw, boxes.getY(), bw, boxes.getHeight()).reduced (2.0f * u);
        const Colour c = segColour (roles[i]);
        g.setColour (c.withAlpha (present[i] ? 0.14f : 0.05f));
        g.fillRoundedRectangle (b, 5.0f * u);
        g.setColour (present[i] ? c : col::faint);
        g.fillRect (b.getX(), b.getY() + 4.0f * u, 2.5f * u, b.getHeight() - 8.0f * u);
        auto bi = b.reduced (7.0f * u, 3.0f * u);
        txt (g, juce::String::charToString (roles[i]), bi.removeFromTop (12.0f * u), 9.5f, present[i] ? c : col::faint, Justification::centredLeft, true);
        txt (g, present[i] ? juce::String ((int) std::lround (vals[i])) + " ms" : juce::String (look::U ("—")), bi.removeFromTop (16.0f * u), expanded ? 14.0f : 11.5f, present[i] ? col::text : col::faint, Justification::centredLeft, true);
        if (expanded && present[i])
        {
            const char* nn = bpm > 30.0 ? noteName (vals[i], bpm) : "";
            juce::String sub = bpm > 30.0 ? juce::String (look::U ("≈ ")) + nn : juce::String();
            if (i == 1 && hs.hasD) sub += juce::String::fromUTF8 ("  ") + juce::String ((int) std::lround (hs.slopeD)) + " dB/s";
            if (i == 2 && hs.hasS) sub += juce::String::fromUTF8 ("  ") + juce::String ((int) std::lround (hs.slopeS)) + " dB/s";
            if (i == 3 && hs.hasR) sub += juce::String::fromUTF8 ("  ") + juce::String ((int) std::lround (hs.slopeR)) + " dB/s";
            txt (g, sub, bi, 8.5f, col::dim, Justification::centredLeft);
        }
        addHit (b, A_Seg, i, RSpgHit);
    }
    // courbe
    auto plot = in.reduced (0, 2.0f * u);
    plot.removeFromLeft (26.0f * u);
    plot.removeFromBottom (14.0f * u);
    const float totalMs = std::max (30.0f, hitDetail.stepMs * (float) hitDetail.n);
    const float dbLo = -60.0f;
    auto X = [&] (float ms) { return plot.getX() + ms / totalMs * plot.getWidth(); };
    auto Y = [&] (float db) { return plot.getY() + (0.0f - juce::jlimit (dbLo, 0.0f, db)) / (-dbLo) * plot.getHeight(); };
    g.setColour (col::panel);
    g.fillRect (plot);
    for (int d : { 0, -20, -40, -60 })
    {
        rule (g, plot.getX(), Y ((float) d), plot.getRight(), Y ((float) d), col::line);
        txt (g, juce::String (d), Rectangle<float> (r.getX() + 2.0f * u, Y ((float) d) - 6.0f * u, 24.0f * u, 12.0f * u), 8.0f, col::faint, Justification::centredRight);
    }
    if (bpm > 30.0)
    {
        const double sixteenth = beatMs (bpm) / 4.0;
        for (int k = 1; k * sixteenth < totalMs; ++k)
        {
            const bool beat = (k % 4) == 0;
            rule (g, X ((float) (k * sixteenth)), plot.getY(), X ((float) (k * sixteenth)), plot.getBottom(), beat ? col::mint.withAlpha (0.35f) : col::faint.withAlpha (0.25f));
            if (beat && k * sixteenth * 1.0 < totalMs * 0.95) txt (g, juce::String (k / 4) + (k == 4 ? " temps" : ""), Rectangle<float> (X ((float) (k * sixteenth)) - 16.0f * u, plot.getBottom() + 1.0f * u, 32.0f * u, 12.0f * u), 8.0f, col::mint, Justification::centred);
        }
    }
    // zones colorées
    auto zone = [&] (float t0, float t1, Colour c) { g.setColour (c.withAlpha (0.13f)); g.fillRect (Rectangle<float> (X (t0), plot.getY(), std::max (1.0f, X (t1) - X (t0)), plot.getHeight())); };
    zone (hitDetail.tA0, hitDetail.tPeak, segColour ('A'));
    for (int i = 0; i < hitDetail.nSeg; ++i) zone (hitDetail.segT0[i], hitDetail.segT1[i], segColour (hitDetail.segRole[i]));
    // polyligne colorée par rôle
    auto colourAt = [&] (float t)
    {
        if (t <= hitDetail.tPeak) return segColour ('A');
        for (int i = 0; i < hitDetail.nSeg; ++i) if (t <= hitDetail.segT1[i]) return segColour (hitDetail.segRole[i]);
        return col::faint;
    };
    for (int i = 1; i < hitDetail.n; ++i)
    {
        const float t0 = hitDetail.stepMs * (float) (i - 1), t1 = hitDetail.stepMs * (float) i;
        g.setColour (colourAt (t1));
        g.drawLine (X (t0), Y (hitDetail.envDb[i - 1]), X (t1), Y (hitDetail.envDb[i]), 2.0f);
    }
    // T20 / T40 / T60
    for (auto [lvl, ms] : { std::pair<float, float> { -20.0f, hs.t20 }, { -40.0f, hs.t40 }, { -60.0f, hs.t60 } })
    {
        if (ms <= 0) continue;
        const float t = hitDetail.tPeak + ms;
        g.setColour (col::white.withAlpha (0.7f));
        g.fillEllipse (X (t) - 2.5f * u, Y (lvl) - 2.5f * u, 5.0f * u, 5.0f * u);
        if (expanded) txt (g, "T" + juce::String ((int) -lvl), Rectangle<float> (X (t) + 3.0f * u, Y (lvl) - 12.0f * u, 30.0f * u, 12.0f * u), 8.0f, col::white, Justification::centredLeft, true);
    }
    if (expanded)
    {
        auto foot = Rectangle<float> (plot.getX(), plot.getBottom() + 12.0f * u, plot.getWidth(), 12.0f * u);
        juce::String s = juce::String::fromUTF8 ("crête ") + juce::String (hs.peakDb, 1) + " dBFS";
        if (hs.hasS) s += juce::String::fromUTF8 ("  ·  S à ") + juce::String (hs.sustainDb, 1) + " dB";
        if (hs.truncated) s += juce::String::fromUTF8 ("  ·  coupé par le coup suivant");
        txt (g, s, foot, 8.5f, col::dim, Justification::centredLeft);
    }
}

// ---------------------------------------------------------------- module spectrogramme
void ScopeEditor::drawSpectrogram (juce::Graphics& g, Rectangle<float> r, bool expanded)
{
    auto body = panel (g, r, "SPECTROGRAMME", col::mint, TabSpectrogram, expanded);
    const float timeSec = kTimes[juce::jlimit (0, 6, ui.spgTime)];
    const float floorDb = kRanges[juce::jlimit (0, 4, ui.spgRange)][0], ceilDb = kRanges[juce::jlimit (0, 4, ui.spgRange)][1];
    const float tilt = kTilts[juce::jlimit (0, 3, ui.spgTilt)];

    // ligne de contrôles
    {
        auto row = body.removeFromTop (22.0f * u);
        body.removeFromTop (4.0f * u);
        const float cw = (expanded ? 118.0f : 108.0f) * u;
        auto put = [&] (int ctl, float wmul = 1.0f) { auto b = row.removeFromLeft (cw * wmul); row.removeFromLeft (5.0f * u); chip (g, b, ctl, RSpgCtl); };
        put (C_SpgSource); put (C_SpgTime);
        if (expanded) { put (C_SpgRange); put (C_SpgTilt, 1.1f); put (C_SpgPalette); put (C_SpgRes, 1.25f); }
        if (eng.settings.hold.load()) txt (g, "GEL", row.removeFromRight (40.0f * u), 10.0f, col::cyan, Justification::centredRight, true);
    }
    Rectangle<float> side;
    if (expanded) { side = body.removeFromRight (body.getWidth() * 0.27f); body.removeFromRight (8.0f * u); }
    auto main = body;
    auto env = main.removeFromBottom (main.getHeight() * (expanded ? 0.24f : 0.23f));
    main.removeFromBottom (5.0f * u);
    auto bars = main.removeFromRight (main.getWidth() * (expanded ? 0.17f : 0.21f));
    main.removeFromRight (4.0f * u);
    spgRect = main; barsRect = bars; envRect = env.withWidth (main.getWidth()).withX (main.getX());
    envRect.setHeight (env.getHeight());

    // ----- spectrogramme
    {
        g.setColour (col::bg);
        g.fillRect (main);
        const int wx = spg.writeX();
        const int W = SpgRenderer::W, H = SpgRenderer::H;
        const float f1 = (float) (W - wx) / (float) W;
        g.saveState();
        g.reduceClipRegion (main.toNearestInt());
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        const int mx = (int) main.getX(), my = (int) main.getY(), mw = (int) main.getWidth(), mh = (int) main.getHeight();
        const int w1 = (int) std::lround (mw * f1);
        if (W - wx > 0 && w1 > 0) g.drawImage (spg.image(), mx, my, w1, mh, wx, 0, W - wx, H);
        if (wx > 0 && mw - w1 > 0) g.drawImage (spg.image(), mx + w1, my, mw - w1, mh, 0, 0, wx, H);
        g.restoreState();
        // grille de fréquences
        const float fmax = spgFmax;
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            if (f > fmax) break;
            const float y = main.getBottom() - look::freqToX (f, (float) kFMin, fmax) * main.getHeight();
            g.setColour (col::white.withAlpha (0.10f));
            g.drawHorizontalLine ((int) y, main.getX(), main.getRight());
            txt (g, f >= 1000 ? juce::String (f / 1000.0f, f == 1000 || f == 2000 || f == 5000 || f == 10000 ? 0 : 1) + "k" : juce::String ((int) f), Rectangle<float> (main.getX() + 2.0f * u, y - 11.0f * u, 34.0f * u, 11.0f * u), 8.0f, col::white.withAlpha (0.65f), Justification::centredLeft);
        }
        // repères de temps
        txt (g, "-" + juce::String (timeSec, timeSec < 1 ? 1 : 0) + " s", Rectangle<float> (main.getX() + 4.0f * u, main.getBottom() - 12.0f * u, 50.0f * u, 11.0f * u), 8.0f, col::white.withAlpha (0.5f), Justification::centredLeft);
        txt (g, "0", Rectangle<float> (main.getRight() - 24.0f * u, main.getBottom() - 12.0f * u, 20.0f * u, 11.0f * u), 8.0f, col::white.withAlpha (0.5f), Justification::centredRight);
        g.setColour (col::line);
        g.drawRect (main, 1.0f);
        // réticule de survol
        if (mouseInside && main.contains (mouse))
        {
            g.setColour (col::white.withAlpha (0.35f));
            g.drawVerticalLine ((int) mouse.x, main.getY(), main.getBottom());
            g.drawHorizontalLine ((int) mouse.y, main.getX(), main.getRight());
        }
        addHit (main, A_None, 0, RSpgMain, TabSpectrogram);
    }

    // ----- barres de spectre (alignées sur l'axe des fréquences)
    {
        g.setColour (col::bg);
        g.fillRect (bars);
        if (haveSb)
        {
            const int mode = ui.spgSource == 1 ? ModeMid : (ui.spgSource == 2 ? ModeSide : ModeST);
            const float inv = 1.0f / (ceilDb - floorDb);
            juce::Path live, peak;
            const int n = std::max (2, (int) bars.getHeight());
            for (int k = 0; k < n; ++k)
            {
                const float t = (float) k / (float) (n - 1);               // 0 = haut (aigus)
                const int i = juce::jlimit (0, kSpecPts - 1, (int) std::lround ((1.0f - t) * (kSpecPts - 1)));
                const float f = look::xToFreq ((float) i / (kSpecPts - 1), (float) kFMin, sb->fmax);
                const float tl = tilt * std::log2 (f / 1000.0f);
                const float x1 = bars.getX() + juce::jlimit (0.0f, 1.0f, (sb->live[mode][i] + tl - floorDb) * inv) * bars.getWidth();
                const float x2 = bars.getX() + juce::jlimit (0.0f, 1.0f, (sb->peak[mode][i] + tl - floorDb) * inv) * bars.getWidth();
                const float y = bars.getY() + t * bars.getHeight();
                if (k == 0) { live.startNewSubPath (bars.getX(), y); peak.startNewSubPath (x2, y); }
                live.lineTo (x1, y);
                peak.lineTo (x2, y);
            }
            live.lineTo (bars.getX(), bars.getBottom());
            live.closeSubPath();
            g.setGradientFill (juce::ColourGradient (col::mint.withAlpha (0.15f), bars.getX(), 0, col::mint.withAlpha (0.65f), bars.getRight(), 0, false));
            g.fillPath (live);
            g.setColour (col::white.withAlpha (0.55f));
            g.strokePath (peak, juce::PathStrokeType (1.0f));
            // limites des 8 zones
            for (int b = 1; b < kBands; ++b)
            {
                const float y = bars.getBottom() - look::freqToX ((float) bandEdges()[b], (float) kFMin, spgFmax) * bars.getHeight();
                g.setColour (col::white.withAlpha (0.08f));
                g.drawHorizontalLine ((int) y, bars.getX(), bars.getRight());
                if (expanded) txt (g, bandName (b), Rectangle<float> (bars.getRight() - 62.0f * u, y - 11.0f * u, 60.0f * u, 11.0f * u), 7.5f, col::faint, Justification::centredRight);
            }
        }
        g.setColour (col::line);
        g.drawRect (bars, 1.0f);
        addHit (bars, A_None, 0, RSpgBars, TabSpectrogram);
    }

    // ----- enveloppe et forme d'onde (même axe de temps que le spectrogramme)
    {
        g.setColour (col::bg);
        g.fillRect (envRect);
        const auto& es = eng.analyzer.envelope;
        const uint64_t N = es.size();
        const int px = std::max (2, (int) envRect.getWidth());
        const float span = timeSec * 1000.0f;
        const float bpp = span / (float) px;
        const float cy = envRect.getCentreY(), hh = envRect.getHeight() * 0.46f;
        juce::Path wave;
        wave.startNewSubPath (envRect.getX(), cy);
        std::vector<float> topY ((size_t) px), botY ((size_t) px);
        for (int x = 0; x < px; ++x)
        {
            const int64_t b0 = (int64_t) N - (int64_t) span + (int64_t) std::floor ((float) x * bpp);
            const int64_t b1 = std::max<int64_t> (b0 + 1, (int64_t) std::floor ((float) (x + 1) * bpp) + (int64_t) N - (int64_t) span);
            float mn = 0, mxv = 0;
            for (int64_t b = b0; b < b1; ++b) { if (b < 0 || b >= (int64_t) N) continue; mn = std::min (mn, es.minAt ((uint64_t) b)); mxv = std::max (mxv, es.maxAt ((uint64_t) b)); }
            topY[(size_t) x] = cy - juce::jlimit (-1.0f, 1.0f, mxv) * hh;
            botY[(size_t) x] = cy - juce::jlimit (-1.0f, 1.0f, mn) * hh;
        }
        // zones ADSR des coups récents
        const uint32_t hc = eng.analyzer.hits.count.load (std::memory_order_acquire);
        auto binToX = [&] (float bin) { return envRect.getX() + (bin - ((float) N - span)) / span * envRect.getWidth(); };
        for (uint32_t k = 0; k < std::min<uint32_t> (hc, HitTracker::kRingN); ++k)
        {
            const HitSummary& hs = eng.analyzer.hits.ring[(hc - 1 - k) % HitTracker::kRingN];
            const float start = (float) hs.startBin;
            if (start + hs.lengthMs < (float) N - span) break;
            float t = start + hs.peakOffMs;
            auto band = [&] (float b0, float b1, char role)
            {
                const float x0 = binToX (b0), x1 = binToX (b1);
                if (x1 < envRect.getX() || x0 > envRect.getRight()) return;
                auto rc = Rectangle<float> (juce::jmax (envRect.getX(), x0), envRect.getY(), juce::jmin (envRect.getRight(), x1) - juce::jmax (envRect.getX(), x0), envRect.getHeight());
                g.setColour (segColour (role).withAlpha (0.14f));
                g.fillRect (rc);
                g.setColour (segColour (role).withAlpha (0.9f));
                g.fillRect (rc.withHeight (3.0f * u));
                if (rc.getWidth() > 10.0f * u) txt (g, juce::String::charToString (role), rc.withTrimmedTop (3.0f * u).withHeight (11.0f * u), 8.0f, segColour (role), Justification::centred, true);
            };
            band (t - hs.aMs, t, 'A');
            if (hs.hasD) { band (t, t + hs.dMs, 'D'); t += hs.dMs; }
            if (hs.hasS) { band (t, t + hs.sMs, 'S'); t += hs.sMs; }
            if (hs.hasR) { band (t, t + hs.rMs, 'R'); }
        }
        g.setColour (col::mint.withAlpha (0.85f));
        for (int x = 0; x < px; ++x) g.drawVerticalLine ((int) (envRect.getX() + (float) x), topY[(size_t) x], botY[(size_t) x] + 1.0f);
        rule (g, envRect.getX(), cy, envRect.getRight(), cy, col::faint.withAlpha (0.5f));
        g.setColour (col::line);
        g.drawRect (envRect, 1.0f);
        txt (g, "ENVELOPPE", Rectangle<float> (envRect.getX() + 4.0f * u, envRect.getBottom() - 12.0f * u, 70.0f * u, 11.0f * u), 7.5f, col::faint, Justification::centredLeft, true);
        addHit (envRect, A_None, 0, RSpgEnv, TabSpectrogram);
        // lecture ADSR compacte sous les barres (vue d'ensemble)
        if (! expanded)
        {
            auto rb = Rectangle<float> (bars.getX(), envRect.getY(), bars.getWidth() + 4.0f * u, envRect.getHeight());
            hitRect = rb;
            g.setColour (col::bg);
            g.fillRect (rb);
            g.setColour (col::line);
            g.drawRect (rb, 1.0f);
            if (haveHitDetail)
            {
                const HitSummary& hs = hitDetail.s;
                const char roles[4] = { 'A', 'D', 'S', 'R' };
                const float vals[4] = { hs.aMs, hs.dMs, hs.sMs, hs.rMs };
                const bool pres[4] = { true, hs.hasD != 0, hs.hasS != 0, hs.hasR != 0 };
                auto in = rb.reduced (5.0f * u, 3.0f * u);
                const float lh = in.getHeight() / 4.0f;
                for (int i = 0; i < 4; ++i)
                {
                    auto line = Rectangle<float> (in.getX(), in.getY() + (float) i * lh, in.getWidth(), lh);
                    txt (g, juce::String::charToString (roles[i]), line.removeFromLeft (12.0f * u), 9.0f, segColour (roles[i]), Justification::centredLeft, true);
                    txt (g, pres[i] ? juce::String ((int) std::lround (vals[i])) + " ms" : juce::String (look::U ("—")), line, 9.5f, pres[i] ? col::text : col::faint, Justification::centredLeft, true);
                    addHit (Rectangle<float> (in.getX(), in.getY() + (float) i * lh, in.getWidth(), lh), A_Seg, i, RSpgHit);
                }
            }
            else txt (g, "ADSR", rb, 9.0f, col::faint, Justification::centred, true);
        }
    }

    // ----- colonne de droite (vue agrandie) : ADSR détaillé, équilibre tonal, graves
    if (expanded)
    {
        auto hitArea = side.removeFromTop (side.getHeight() * 0.56f);
        side.removeFromTop (6.0f * u);
        drawHitPanel (g, hitArea, true);
        auto bal = side.removeFromTop (side.getHeight() * 0.80f);
        g.setColour (col::bg);
        g.fillRoundedRectangle (side.getX(), bal.getY(), side.getWidth(), bal.getHeight() + side.getHeight(), 6.0f * u);
        auto bi = bal.reduced (8.0f * u, 6.0f * u);
        txt (g, look::U ("ÉQUILIBRE TONAL vs repère"), bi.removeFromTop (14.0f * u), 9.5f, col::dim, Justification::centredLeft, true);
        drawBandBars (g, bi, false);
        auto foot = side.reduced (8.0f * u, 2.0f * u);
        if (a.subProm > 10.0f)
        {
            int cents = 0; const int mid = noteFromHz (a.subHz, cents);
            txt (g, juce::String::fromUTF8 ("Fondamentale grave : ") + juce::String (a.subHz, 1) + " Hz (" + midiName (mid) + juce::String (midiOctave (mid)) + ", " + look::fmtU ("%+d", cents) + " ct)", foot, 9.5f, col::mint, Justification::centredLeft, true);
            addHit (foot, A_None, 0, RSpSub);
        }
        else txt (g, look::U ("Fondamentale grave : —"), foot, 9.5f, col::faint, Justification::centredLeft, true);
    }
}
