#include "PluginEditor.h"

using namespace mask;
using juce::Colour;
using juce::Justification;
using juce::Rectangle;

namespace
{
const float kTimes[] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 15.0f, 30.0f };
const float kRanges[][2] = { { -90, -10 }, { -80, 0 }, { -100, -20 }, { -70, -10 }, { -90, 0 } };
const int kDwell[] = { 250, 450, 700, 1000 };
const float kThresh[] = { -60, -54, -48, -42, -36, -30 };
double nowSec() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }
const char* kTabNames[kTabs] = { "VUE", "SPECTRO", "SPECTRE", "STÉRÉO", "SONIE", "HISTO", "NIVEAUX", "EXPORT" };
}

// ================================================================= construction
ScopeEditor::ScopeEditor (ScopeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), eng (p.engine), ui (p.ui)
{
    sbOwner = std::make_unique<SpectrumBlock>();
    sb = sbOwner.get();
    std::memset ((void*) sb, 0, sizeof (SpectrumBlock));
    for (int m_ = 0; m_ < kModes; ++m_) for (int i = 0; i < kSpecPts; ++i) { sb->live[m_][i] = sb->avg[m_][i] = sb->peak[m_][i] = -140.0f; }
    setResizable (true, true);
    setResizeLimits (1100, 680, 2800, 1700);
    setSize (1280, 800);
    setWantsKeyboardFocus (true);
    bpmEdit.setMultiLine (false);
    bpmEdit.setJustification (Justification::centred);
    bpmEdit.setInputRestrictions (6, "0123456789.");
    bpmEdit.addListener (this);
    bpmEdit.setColour (juce::TextEditor::backgroundColourId, col::panel2);
    bpmEdit.setColour (juce::TextEditor::textColourId, col::text);
    bpmEdit.setColour (juce::TextEditor::outlineColourId, col::mint);
    addChildComponent (bpmEdit);
    startTimerHz (30);
}

ScopeEditor::~ScopeEditor() { stopTimer(); }

void ScopeEditor::resized() { u = juce::jlimit (0.8f, 1.9f, std::min ((float) getWidth() / 1280.0f, (float) getHeight() / 800.0f)); }

// ================================================================= données
void ScopeEditor::takeSnapshot()
{
    eng.meters.out.read (m);
    eng.analyzer.analysisOut.read (a);
    haveSb = eng.analyzer.spectrumOut.read (*sb) || haveSb;
    eng.analyzer.hits.latest.read (hitDetail);
    haveHitDetail = hitDetail.n > 0;
    // tempo : celui de l'hôte (AUTO) s'il est connu, sinon celui saisi à la main (0 = inconnu)
    if (ui.bpmAuto && eng.hostBpm.load() > 30.0f) { bpm = eng.hostBpm.load(); bpmFromHost = true; }
    else { bpm = ui.bpmManual > 30.0 ? ui.bpmManual : 0.0; bpmFromHost = false; }
    cctx.m = m; cctx.a = a; cctx.bpm = bpm; cctx.sigNum = eng.hostSigNum.load(); cctx.bpmFromHost = bpmFromHost;
    cctx.target = ui.target; cctx.spot = ui.spot; cctx.style = ui.style;
    cctx.signal = m.lufsS > -60.0f || m.rms3s > -62.0f;
    // lissage d'affichage des crêtes (retombée 24 dB/s)
    const double t = nowSec();
    const float dt = (float) juce::jlimit (0.0, 0.2, t - lastDispSec);
    lastDispSec = t;
    const float pk[2] = { m.peakL, m.peakR }, tp[2] = { m.tpL, m.tpR };
    for (int c = 0; c < 2; ++c)
    {
        dispPk[c] = std::max (pk[c], dispPk[c] - 24.0f * dt);
        dispTp[c] = std::max (tp[c], dispTp[c] - 24.0f * dt);
    }
}

void ScopeEditor::timerCallback()
{
    takeSnapshot();
    const double t = nowSec();
    if (t - lastCoachSec > 0.25)
    {
        lastCoachSec = t;
        if (t - lastLearnSec > 25.0) { lastLearnSec = t; ++learnIndex; }
        coach.update (cctx, t, advice, learnIndex);
    }
    if (ui.tab == TabOverview || ui.tab == TabSpectrogram)
    {
        SpgRenderer::View v;
        v.plane = juce::jlimit (0, 2, ui.spgSource);
        v.timeSec = kTimes[juce::jlimit (0, 6, ui.spgTime)];
        v.floorDb = kRanges[juce::jlimit (0, 4, ui.spgRange)][0];
        v.ceilDb = kRanges[juce::jlimit (0, 4, ui.spgRange)][1];
        static const float tilts[] = { 0.0f, 1.5f, 3.0f, 4.5f };
        v.tilt = tilts[juce::jlimit (0, 3, ui.spgTilt)];
        v.palette = ui.spgPalette;
        spgFmax = (float) std::min (20000.0, proc.sampleRate * 0.49);
        spg.update (eng.analyzer.spectrogram, v, spgFmax);
    }
    repaint();
}

// ================================================================= outils de dessin
void ScopeEditor::addHit (Rectangle<float> r, int action, int arg, int region, int moduleTab, int arg2)
{
    Hit h; h.r = r; h.action = action; h.arg = arg; h.arg2 = arg2; h.region = region; h.moduleTab = moduleTab;
    hits.push_back (h);
}

const ScopeEditor::Hit* ScopeEditor::hitAt (juce::Point<float> p, bool needAction) const
{
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->r.contains (p) && (needAction ? it->action != A_None : it->region != RNone)) return &*it;
    return nullptr;
}

void ScopeEditor::txt (juce::Graphics& g, const juce::String& s, Rectangle<float> r, float size, Colour c, Justification j, bool bold) const
{
    g.setColour (c);
    g.setFont (juce::FontOptions (size * u, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText (s, r, j, true);
}

void ScopeEditor::rule (juce::Graphics& g, float x1, float y1, float x2, float y2, Colour c, float th) const
{
    g.setColour (c);
    g.drawLine (x1, y1, x2, y2, th);
}

void ScopeEditor::button (juce::Graphics& g, Rectangle<float> r, const juce::String& label, bool on, int action, int arg, int region, Colour onColour)
{
    const bool hov = mouseInside && r.contains (mouse);
    g.setColour (on ? onColour.withAlpha (0.18f) : (hov ? col::panel2.brighter (0.15f) : col::panel2));
    g.fillRoundedRectangle (r, 5.0f * u);
    g.setColour (on ? onColour : col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f * u, 1.0f);
    txt (g, label, r, 10.5f, on ? onColour : col::dim, Justification::centred, true);
    addHit (r, action, arg, region);
}

juce::String ScopeEditor::ctlName (int id) const
{
    static const char* n[C_Count] = { "SOURCE", "TEMPS", "PLAGE", "PENTE", "PALETTE", "RÉSOL.", "FFT", "LISSAGE", "VITESSE", "PENTE", "PLAGE",
                                      "MOYENNE", "CRÊTE", "COURBES", "MODE", "DURÉE", "ÉCOUTE", "CODEC", "SUPPORT", "STYLE", "BULLES", "RAZ AUTO", "SEUIL COUPS" };
    return juce::String::fromUTF8 (n[id]);
}

juce::StringArray ScopeEditor::ctlOptions (int id) const
{
    juce::StringArray o;
    switch (id)
    {
        case C_SpgSource: o = { look::U ("MIX"), look::U ("MID"), look::U ("SIDE") }; break;
        case C_SpgTime: for (float t : kTimes) o.add (juce::String (t, t < 1 ? 1 : 0) + " s"); break;
        case C_SpgRange: case C_SpRange: for (auto& r : kRanges) o.add (juce::String ((int) r[0]) + look::U (" … ") + juce::String ((int) r[1])); break;
        case C_SpgTilt: case C_SpTilt: o = { look::U ("0"), look::U ("+1,5 dB/oct"), look::U ("+3 dB/oct"), look::U ("+4,5 dB/oct") }; break;
        case C_SpgPalette: o = { look::U ("MASK"), look::U ("ICE"), look::U ("MINT"), look::U ("GRAY") }; break;
        case C_SpgRes: o = { look::U ("2048 (net en temps)"), look::U ("4096"), look::U ("8192 (net en fréquence)") }; break;
        case C_SpFft: o = { look::U ("2048"), look::U ("4096"), look::U ("8192"), look::U ("16384"), look::U ("32768") }; break;
        case C_SpSmooth: o = { look::U ("AUCUN (1/96)"), look::U ("1/24 oct"), look::U ("1/12 oct"), look::U ("1/6 oct"), look::U ("1/3 oct") }; break;
        case C_SpSpeed: o = { look::U ("RAPIDE (40 ms)"), look::U ("MOYEN (120 ms)"), look::U ("LENT (350 ms)"), look::U ("TRÈS LENT (1 s)") }; break;
        case C_SpAvg: case C_SpPeak: case C_AutoReset: o = { look::U ("NON"), look::U ("OUI") }; break;
        case C_SfMode: o = { look::U ("GONIOMÈTRE"), look::U ("L / R"), look::U ("POLAIRE (niveau)") }; break;
        case C_HistSpan: o = { look::U ("1 min"), look::U ("5 min"), look::U ("15 min"), look::U ("SESSION") }; break;
        case C_Spot: o = { look::U ("NORMAL -14"), look::U ("LOUD -11"), look::U ("QUIET -19") }; break;
        case C_ExCodec: o = { look::U ("OGG 96 kbps"), look::U ("OGG 160 kbps"), look::U ("OGG 320 kbps") }; break;
        case C_Target: o = { look::U ("OFF"), look::U ("SPOTIFY"), look::U ("CONCERT"), look::U ("CD") }; break;
        case C_Style: { int n; for (int i = 0; i < 5; ++i) o.add (styles (n)[i].name); break; }
        case C_Dwell: o = { look::U ("250 ms"), look::U ("450 ms"), look::U ("700 ms"), look::U ("1 s") }; break;
        case C_HitThresh: for (float t : kThresh) o.add (juce::String ((int) t) + " dBFS"); break;
        default: break;
    }
    return o;
}

int ScopeEditor::getCtl (int id) const
{
    switch (id)
    {
        case C_SpgSource: return ui.spgSource; case C_SpgTime: return ui.spgTime; case C_SpgRange: return ui.spgRange;
        case C_SpgTilt: return ui.spgTilt; case C_SpgPalette: return ui.spgPalette;
        case C_SpgRes: return eng.settings.specFft.load(); case C_SpFft: return eng.settings.spectrumFft.load();
        case C_SpSmooth: return eng.settings.smooth.load(); case C_SpSpeed: return eng.settings.speed.load();
        case C_SpTilt: return ui.spTilt; case C_SpRange: return ui.spRange; case C_SpAvg: return ui.spAvg; case C_SpPeak: return ui.spPeak;
        case C_SfMode: return ui.sfMode; case C_HistSpan: return ui.histSpan; case C_Spot: return ui.spot; case C_ExCodec: return ui.exCodec;
        case C_Target: return ui.target; case C_Style: return ui.style;
        case C_Dwell: { int best = 0; for (int i = 0; i < 4; ++i) if (std::abs (kDwell[i] - ui.dwellMs) < std::abs (kDwell[best] - ui.dwellMs)) best = i; return best; }
        case C_AutoReset: return eng.settings.autoReset.load() ? 1 : 0;
        case C_HitThresh: { int best = 0; const float t = eng.settings.hitThresh.load(); for (int i = 0; i < 6; ++i) if (std::abs (kThresh[i] - t) < std::abs (kThresh[best] - t)) best = i; return best; }
        default: return 0;
    }
}

void ScopeEditor::setCtl (int id, int v)
{
    switch (id)
    {
        case C_SpgSource: ui.spgSource = v; break; case C_SpgTime: ui.spgTime = v; break; case C_SpgRange: ui.spgRange = v; break;
        case C_SpgTilt: ui.spgTilt = v; break; case C_SpgPalette: ui.spgPalette = v; break;
        case C_SpgRes: eng.settings.specFft.store (v); break; case C_SpFft: eng.settings.spectrumFft.store (v); break;
        case C_SpSmooth: eng.settings.smooth.store (v); break; case C_SpSpeed: eng.settings.speed.store (v); break;
        case C_SpTilt: ui.spTilt = v; break; case C_SpRange: ui.spRange = v; break; case C_SpAvg: ui.spAvg = v; break; case C_SpPeak: ui.spPeak = v; break;
        case C_SfMode: ui.sfMode = v; break; case C_HistSpan: ui.histSpan = v; break; case C_Spot: ui.spot = v; break; case C_ExCodec: ui.exCodec = v; break;
        case C_Target: ui.target = v; break; case C_Style: ui.style = v; break;
        case C_Dwell: ui.dwellMs = kDwell[juce::jlimit (0, 3, v)]; break;
        case C_AutoReset: eng.settings.autoReset.store (v != 0); break;
        case C_HitThresh: eng.settings.hitThresh.store (kThresh[juce::jlimit (0, 5, v)]); break;
        default: break;
    }
}

void ScopeEditor::chip (juce::Graphics& g, Rectangle<float> r, int ctl, int region, const char* label, Colour c)
{
    const auto opts = ctlOptions (ctl);
    const int cur = juce::jlimit (0, juce::jmax (0, opts.size() - 1), getCtl (ctl));
    const bool hov = mouseInside && r.contains (mouse);
    g.setColour (hov ? col::panel2.brighter (0.15f) : col::panel2);
    g.fillRoundedRectangle (r, 5.0f * u);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f * u, 1.0f);
    const juce::String lab = label ? juce::String::fromUTF8 (label) : ctlName (ctl);
    auto in = r.reduced (6.0f * u, 0);
    const float lw = juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (8.5f * u, juce::Font::bold)), lab) + 4.0f;
    txt (g, lab, in.removeFromLeft (lw), 8.5f, col::faint, Justification::centredLeft, true);
    txt (g, opts[cur], in, 10.5f, c.isTransparent() ? col::text : c, Justification::centredLeft, true);
    addHit (r, A_Menu, ctl, region);
}

Rectangle<float> ScopeEditor::panel (juce::Graphics& g, Rectangle<float> r, const juce::String& title, Colour c, int tab, bool expanded)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8.0f * u);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f * u, 1.0f);
    auto bar = r.removeFromTop (24.0f * u);
    auto body = r.reduced (8.0f * u, 4.0f * u);
    // titre
    g.setColour (c);
    g.fillRoundedRectangle (bar.getX() + 8.0f * u, bar.getCentreY() - 5.0f * u, 3.0f * u, 10.0f * u, 1.5f * u);
    txt (g, title, bar.withTrimmedLeft (18.0f * u), 11.0f, c, Justification::centredLeft, true);
    // icône agrandir / réduire
    const auto ic = Rectangle<float> (bar.getRight() - 24.0f * u, bar.getCentreY() - 6.0f * u, 12.0f * u, 12.0f * u);
    g.setColour (col::dim);
    if (! expanded) { g.drawRect (ic.reduced (1.0f * u), 1.2f); g.drawLine (ic.getX() + 6 * u, ic.getBottom() - 6 * u, ic.getRight() - 1 * u, ic.getY() + 1 * u, 1.2f); }
    else { g.drawLine (ic.getX(), ic.getY(), ic.getRight(), ic.getBottom(), 1.4f); g.drawLine (ic.getRight(), ic.getY(), ic.getX(), ic.getBottom(), 1.4f); }
    addHit (r.withHeight (0).withY (bar.getY()).withHeight (bar.getHeight()).withX (bar.getX()).withWidth (bar.getWidth()), A_Tab, expanded ? (int) TabOverview : tab, RModuleExpand);
    addHit (r.withTrimmedTop (0).withY (bar.getBottom()).withHeight (r.getHeight()), A_None, 0, RNone, tab);
    return body;
}

// ================================================================= en-tête
void ScopeEditor::drawHeader (juce::Graphics& g, Rectangle<float> r)
{
    g.setColour (col::bg);
    g.fillRect (r);
    auto row = r.reduced (10.0f * u, 6.0f * u);
    // logo : « mask:: » en blanc, le nom dans la couleur du plugin
    {
        const juce::Font f (juce::FontOptions (24.0f * u, juce::Font::bold));
        const float w1 = juce::GlyphArrangement::getStringWidth (f, "mask::");
        const float w2 = juce::GlyphArrangement::getStringWidth (f, "scope");
        auto lr = row.removeFromLeft (w1 + w2 + 16.0f * u);
        g.setFont (f);
        g.setColour (col::white);
        g.drawText ("mask::", lr.removeFromLeft (w1 + 2.0f), Justification::centredLeft);
        g.setColour (col::mint);
        g.drawText ("scope", lr, Justification::centredLeft);
    }
    // contrôles à droite
    const float h = row.getHeight();
    auto place = [&] (float w) { auto b = row.removeFromRight (w * u).withSizeKeepingCentre (w * u, h - 2.0f * u); row.removeFromRight (6.0f * u); return b; };
    button (g, place (28), "", false, A_Settings, 0, RSettings, col::dim);
    {   // engrenage simplifié
        const auto c = hits.back().r.getCentre();
        g.setColour (col::dim);
        g.drawEllipse (c.x - 5.0f * u, c.y - 5.0f * u, 10.0f * u, 10.0f * u, 1.4f);
        for (int i = 0; i < 6; ++i)
        {
            const float an = (float) i * juce::MathConstants<float>::pi / 3.0f;
            g.drawLine (c.x + std::cos (an) * 5.0f * u, c.y + std::sin (an) * 5.0f * u, c.x + std::cos (an) * 7.5f * u, c.y + std::sin (an) * 7.5f * u, 1.6f);
        }
    }
    button (g, place (52), "RAZ", false, A_Reset, 0, RReset, col::amber);
    button (g, place (50), "GEL", eng.settings.hold.load(), A_Hold, 0, RHold, col::cyan);
    button (g, place (96), ui.tips ? "ASTUCES ON" : "ASTUCES OFF", ui.tips, A_Tips, 0, RTipsBtn, col::mint);
    {
        auto b = place (106);
        const bool hov = mouseInside && b.contains (mouse);
        g.setColour (hov ? col::panel2.brighter (0.15f) : col::panel2);
        g.fillRoundedRectangle (b, 5.0f * u);
        g.setColour (col::line); g.drawRoundedRectangle (b.reduced (0.5f), 5.0f * u, 1.0f);
        auto in = b.reduced (6.0f * u, 0);
        txt (g, "BPM", in.removeFromLeft (24.0f * u), 8.5f, col::faint, Justification::centredLeft, true);
        txt (g, bpm > 30.0 ? juce::String (bpm, bpm == std::floor (bpm) ? 0 : 1) : juce::String (look::U ("—")), in.removeFromLeft (32.0f * u), 11.0f, col::text, Justification::centredLeft, true);
        txt (g, ui.bpmAuto && bpmFromHost ? look::U ("HÔTE") : "MAN.", in, 8.5f, ui.bpmAuto && bpmFromHost ? col::mint : col::amber, Justification::centredLeft, true);
        addHit (b, A_Bpm, 0, RBpm);
        bpmEdit.setBounds (b.toNearestInt());
    }
    chip (g, place (146), C_Style, RStyleBtn, nullptr, col::violet);
    {
        auto b = place (124);
        chip (g, b, C_Target, RTargetBtn, nullptr, col::mint);
    }
    // onglets
    row.removeFromLeft (6.0f * u);
    const float tw = std::min (84.0f * u, row.getWidth() / (float) kTabs);
    row.removeFromLeft (0.0f);
    for (int t = 0; t < kTabs; ++t)
    {
        auto b = row.removeFromLeft (tw).reduced (2.0f * u, 2.0f * u);
        const bool on = ui.tab == t;
        g.setColour (on ? col::mint.withAlpha (0.16f) : Colour (0x00000000));
        g.fillRoundedRectangle (b, 5.0f * u);
        if (on) { g.setColour (col::mint); g.fillRect (b.getX() + 6 * u, b.getBottom() - 2.0f * u, b.getWidth() - 12 * u, 2.0f * u); }
        txt (g, juce::String::fromUTF8 (kTabNames[t]), b, 9.5f, on ? col::mint : (mouseInside && b.contains (mouse) ? col::text : col::dim), Justification::centred, true);
        addHit (b, A_Tab, t, RTabs);
    }
    rule (g, r.getX(), r.getBottom() - 0.5f, r.getRight(), r.getBottom() - 0.5f, col::line);
}

// ================================================================= bandeau coach
void ScopeEditor::drawCoach (juce::Graphics& g, Rectangle<float> r)
{
    g.setColour (col::bg);
    g.fillRect (r);
    rule (g, r.getX(), r.getY() + 0.5f, r.getRight(), r.getY() + 0.5f, col::line);
    auto row = r.reduced (10.0f * u, 6.0f * u);
    auto head = row.removeFromLeft (64.0f * u);
    txt (g, "COACH", head.removeFromTop (14.0f * u), 10.0f, col::mint, Justification::centredLeft, true);
    txt (g, cctx.signal ? "en direct" : "en attente", head, 9.0f, col::faint, Justification::centredLeft);
    const int n = std::min<int> (3, (int) advice.size());
    if (n == 0)
    {
        txt (g, cctx.signal ? look::U ("Rien à signaler pour l'instant.") : look::U ("Lance la lecture : les conseils apparaissent dès que du signal passe."), row, 11.0f, col::dim);
        addHit (r, A_None, 0, RCoach);
        return;
    }
    const float cw = (row.getWidth() - (float) (n - 1) * 8.0f * u) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto c = row.removeFromLeft (cw);
        row.removeFromLeft (8.0f * u);
        const auto& ad = advice[(size_t) i];
        const Colour sc = look::sevColour (ad.sev);
        g.setColour (col::panel);
        g.fillRoundedRectangle (c, 6.0f * u);
        g.setColour (sc);
        g.fillRoundedRectangle (c.getX(), c.getY(), 3.5f * u, c.getHeight(), 1.5f * u);
        auto in = c.reduced (10.0f * u, 3.0f * u);
        txt (g, (ad.sev == 0 ? juce::String::fromUTF8 ("IDÉE · ") : juce::String()) + look::fromStd (ad.title), in.removeFromTop (14.0f * u), 10.5f, sc.brighter (0.2f), Justification::centredLeft, true);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (9.5f * u));
        g.drawFittedText (look::fromStd (ad.text), in.toNearestInt(), Justification::topLeft, 3, 0.9f);
        addHit (c, A_Coach, ad.tab, RCoach);
    }
}

// ================================================================= vue d'ensemble
void ScopeEditor::drawOverview (juce::Graphics& g, Rectangle<float> area)
{
    const float gap = 8.0f * u;
    const float w = area.getWidth(), h = area.getHeight();
    auto a1 = area.removeFromLeft (w * 0.50f - gap * 0.5f);
    area.removeFromLeft (gap);
    auto a2 = area.removeFromLeft (w * 0.26f - gap);
    area.removeFromLeft (gap);
    auto a3 = area;
    drawModule (g, TabSpectrogram, a1, false);
    drawModule (g, TabSpectrum, a2.removeFromTop (h * 0.52f - gap * 0.5f), false);
    a2.removeFromTop (gap);
    drawModule (g, TabHistory, a2, false);
    drawModule (g, TabLoudness, a3.removeFromTop (h * 0.38f - gap * 0.5f), false);
    a3.removeFromTop (gap);
    drawModule (g, TabSoundField, a3.removeFromTop (h * 0.38f - gap * 0.5f), false);
    a3.removeFromTop (gap);
    drawModule (g, TabLevels, a3, false);
}

void ScopeEditor::drawModule (juce::Graphics& g, int tab, Rectangle<float> r, bool expanded)
{
    switch (tab)
    {
        case TabSpectrogram: drawSpectrogram (g, r, expanded); break;
        case TabSpectrum:    drawSpectrum (g, r, expanded); break;
        case TabSoundField:  drawSoundField (g, r, expanded); break;
        case TabLoudness:    drawLoudness (g, r, expanded); break;
        case TabHistory:     drawHistory (g, r, expanded); break;
        case TabLevels:      drawLevels (g, r, expanded); break;
        default: break;
    }
}

// ================================================================= peinture
void ScopeEditor::paint (juce::Graphics& g)
{
    hits.clear();
    g.fillAll (col::bg);
    auto r = getLocalBounds().toFloat();
    const float hh = 46.0f * u;
    const float ch = ui.tips ? 66.0f * u : 0.0f;
    drawHeader (g, r.removeFromTop (hh));
    auto coachR = r.removeFromBottom (ch);
    auto area = r.reduced (8.0f * u, 8.0f * u);
    if (ui.tab == TabOverview) drawOverview (g, area);
    else if (ui.tab == TabExport) drawExport (g, area);
    else drawModule (g, ui.tab, area, true);
    if (ui.tips) drawCoach (g, coachR);
    drawTooltip (g);
}

// ================================================================= bulles
void ScopeEditor::testHover (juce::Point<float> p, bool force)
{
    mouse = p; mouseInside = true; lastMoveSec = nowSec(); forceBubble = force;
}

void ScopeEditor::drawTooltip (juce::Graphics& g)
{
    if (! ui.tips || ! mouseInside) return;
    if (! forceBubble && nowSec() - lastMoveSec < ui.dwellMs * 0.001) return;
    const Hit* h = hitAt (mouse, false);
    if (! h) return;
    hover = Hover {};
    hover.band = (h->region == RSpBands || h->region == RWidthBands) ? h->arg2 : -1;
    hover.hitSeg = h->action == A_Seg ? h->arg : -1;
    const int region = h->region;
    auto rel = [&] (Rectangle<float> rc) { hover.x01 = juce::jlimit (0.0f, 1.0f, (mouse.x - rc.getX()) / std::max (1.0f, rc.getWidth())); hover.y01 = juce::jlimit (0.0f, 1.0f, (mouse.y - rc.getY()) / std::max (1.0f, rc.getHeight())); };
    if (region == RSpgMain && ! spgRect.isEmpty())
    {
        rel (spgRect);
        hover.freq = look::xToFreq (1.0f - hover.y01, (float) kFMin, spgFmax);
        hover.timeS = (1.0f - hover.x01) * kTimes[juce::jlimit (0, 6, ui.spgTime)];
        const int row = juce::jlimit (0, kSpecRows - 1, (int) ((1.0f - hover.y01) * (kSpecRows - 1) + 0.5f));
        const uint64_t c = spg.columnAt (hover.x01);
        hover.db = SpectrogramStore::dequant (eng.analyzer.spectrogram.column (juce::jlimit (0, 2, ui.spgSource), c)[row]);
        hover.hasData = true;
    }
    else if (region == RSpgBars && haveSb && ! barsRect.isEmpty())
    {
        rel (barsRect);
        hover.freq = look::xToFreq (1.0f - hover.y01, (float) kFMin, spgFmax);
        const int i = juce::jlimit (0, kSpecPts - 1, (int) ((1.0f - hover.y01) * (kSpecPts - 1) + 0.5f));
        hover.db = sb->live[ModeST][i];
        hover.hasData = true;
    }
    else if (region == RSpectrum && haveSb && ! specPlot.isEmpty())
    {
        rel (specPlot);
        hover.freq = look::xToFreq (hover.x01, (float) kFMin, sb->fmax);
        const int i = juce::jlimit (0, kSpecPts - 1, (int) (hover.x01 * (kSpecPts - 1) + 0.5f));
        int mode = ModeST; for (int k = 0; k < kModes; ++k) if (ui.spModes & (1 << k)) { mode = k; break; }
        hover.db = sb->live[mode][i];
        hover.hasData = true;
    }
    else if (region == RHistory && ! histPlot.isEmpty())
    {
        rel (histPlot);
        const float span = histT1 - histT0;
        hover.timeS = (1.0f - hover.x01) * span;
        const uint64_t idx = histLast > (uint64_t) (hover.timeS * 10.0f) ? histLast - (uint64_t) (hover.timeS * 10.0f) : 0;
        if (idx >= histFirst && idx < eng.meters.history.size()) { hover.db = eng.meters.history.at (idx).s; hover.hasData = true; }
    }
    const Tip tip = tipFor (region, cctx, hover);
    if (tip.title.empty()) return;
    // mise en page
    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::byWord);
    as.append (look::fromStd (tip.title) + "\n", juce::FontOptions (12.5f * u, juce::Font::bold), col::mint);
    if (! tip.body.empty()) as.append (look::fromStd (tip.body) + "\n", juce::FontOptions (10.5f * u), col::text);
    if (! tip.live.empty()) as.append ("\n" + look::fromStd (tip.live), juce::FontOptions (10.5f * u, juce::Font::bold), col::white);
    const float bw = 360.0f * u;
    juce::TextLayout tl;
    tl.createLayout (as, bw - 20.0f * u);
    const float bh = tl.getHeight() + 18.0f * u;
    auto box = Rectangle<float> (mouse.x + 16.0f * u, mouse.y + 18.0f * u, bw, bh);
    const auto bounds = getLocalBounds().toFloat().reduced (6.0f);
    if (box.getRight() > bounds.getRight()) box.setX (mouse.x - bw - 12.0f * u);
    if (box.getBottom() > bounds.getBottom()) box.setY (mouse.y - bh - 12.0f * u);
    box.setPosition (juce::jmax (bounds.getX(), box.getX()), juce::jmax (bounds.getY(), box.getY()));
    g.setColour (Colour (0xf0101218));
    g.fillRoundedRectangle (box, 8.0f * u);
    g.setColour (col::mint.withAlpha (0.8f));
    g.drawRoundedRectangle (box.reduced (0.5f), 8.0f * u, 1.2f);
    tl.draw (g, box.reduced (10.0f * u, 9.0f * u));
}

// ================================================================= souris / clavier
void ScopeEditor::mouseMove (const juce::MouseEvent& e)
{
    mouse = e.position; mouseInside = true; lastMoveSec = nowSec(); forceBubble = false;
}

void ScopeEditor::mouseExit (const juce::MouseEvent&) { mouseInside = false; }

void ScopeEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    mouse = e.position; lastMoveSec = nowSec();
    const Hit* h = hitAt (e.position, true);
    if (! h) return;
    const Hit hit = *h;
    switch (hit.action)
    {
        case A_Tab: ui.tab = (hit.arg == ui.tab && hit.arg != TabOverview) ? (int) TabOverview : hit.arg; break;
        case A_Coach: ui.tab = hit.arg; break;
        case A_Tips: ui.tips = ! ui.tips; break;
        case A_Reset: eng.reset(); break;
        case A_Hold: eng.settings.hold.store (! eng.settings.hold.load()); break;
        case A_Settings: showSettingsMenu (localAreaToGlobal (hit.r.toNearestInt())); break;
        case A_Menu: showMenu (hit.arg, localAreaToGlobal (hit.r.toNearestInt())); break;
        case A_Toggle: if (hit.arg == C_SpMode) { const int bit = 1 << hit.arg2; ui.spModes ^= bit; if (ui.spModes == 0) ui.spModes = bit; } break;
        case A_Bpm:
        {
            juce::PopupMenu mnu;
            mnu.addItem (1, juce::String::fromUTF8 ("Suivre Logic (AUTO)"), true, ui.bpmAuto);
            mnu.addItem (2, juce::String::fromUTF8 ("Saisir le tempo…"));
            mnu.addItem (3, juce::String::fromUTF8 ("÷ 2"), bpm > 60);
            mnu.addItem (4, juce::String::fromUTF8 ("× 2"), bpm > 0 && bpm < 150);
            mnu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (hit.r.toNearestInt())), [this] (int r)
            {
                if (r == 1) ui.bpmAuto = true;
                else if (r == 2) { ui.bpmAuto = false; bpmEdit.setText (juce::String (bpm > 30 ? bpm : ui.bpmManual, 1), false); bpmEdit.setVisible (true); bpmEdit.grabKeyboardFocus(); bpmEdit.selectAll(); }
                else if (r == 3) { ui.bpmAuto = false; ui.bpmManual = bpm * 0.5; }
                else if (r == 4) { ui.bpmAuto = false; ui.bpmManual = bpm * 2.0; }
            });
            break;
        }
        default: break;
    }
}

void ScopeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
    {
        if (! it->r.contains (e.position)) continue;
        if (it->action != A_None) return;
        if (it->moduleTab >= 0)
        {
            ui.tab = (ui.tab == it->moduleTab) ? (int) TabOverview : it->moduleTab;
            return;
        }
    }
}

void ScopeEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d)
{
    const Hit* h = hitAt (e.position, false);
    if (h && (h->region == RSpgMain || h->region == RSpgEnv) && d.deltaY != 0.0f)
        ui.spgTime = juce::jlimit (0, 6, ui.spgTime + (d.deltaY > 0 ? -1 : 1));
}

bool ScopeEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey && ui.tab != TabOverview) { ui.tab = TabOverview; return true; }
    return false;
}

void ScopeEditor::textEditorReturnKeyPressed (juce::TextEditor&)
{
    const double v = bpmEdit.getText().getDoubleValue();
    if (v >= 30.0 && v <= 400.0) { ui.bpmManual = v; ui.bpmAuto = false; }
    bpmEdit.setVisible (false);
}

void ScopeEditor::showMenu (int ctl, Rectangle<int> anchor)
{
    juce::PopupMenu mnu;
    const auto opts = ctlOptions (ctl);
    const int cur = getCtl (ctl);
    for (int i = 0; i < opts.size(); ++i) mnu.addItem (i + 1, opts[i], true, i == cur);
    mnu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (anchor), [this, ctl] (int r) { if (r > 0) setCtl (ctl, r - 1); });
}

void ScopeEditor::showSettingsMenu (Rectangle<int> anchor)
{
    juce::PopupMenu mnu;
    auto sub = [&] (const juce::String& title, int ctl)
    {
        juce::PopupMenu s;
        const auto opts = ctlOptions (ctl);
        for (int i = 0; i < opts.size(); ++i) s.addItem (ctl * 100 + i + 1, opts[i], true, i == getCtl (ctl));
        mnu.addSubMenu (title, s);
    };
    sub (look::U ("Délai des bulles"), C_Dwell);
    sub (look::U ("Remise à zéro auto au départ de la lecture"), C_AutoReset);
    sub (look::U ("Seuil de détection des coups (ADSR)"), C_HitThresh);
    sub ("Palette du spectrogramme", C_SpgPalette);
    sub (look::U ("Écoute Spotify"), C_Spot);
    mnu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (anchor), [this] (int r) { if (r > 0) setCtl (r / 100, (r % 100) - 1); });
}

// ================================================================= jauge utilitaire
void ScopeEditor::drawGaugeBar (juce::Graphics& g, Rectangle<float> r, float v, float lo, float hi, float target, Colour c, bool vertical) const
{
    g.setColour (col::panel2);
    g.fillRoundedRectangle (r, 3.0f * u);
    const float t = juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
    g.setColour (c);
    if (vertical) g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() * (1.0f - t)), 3.0f * u);
    else g.fillRoundedRectangle (r.withWidth (r.getWidth() * t), 3.0f * u);
    if (target > lo && target < hi)
    {
        const float tt = (target - lo) / (hi - lo);
        g.setColour (col::white);
        if (vertical) g.drawLine (r.getX() - 2.0f * u, r.getBottom() - tt * r.getHeight(), r.getRight() + 2.0f * u, r.getBottom() - tt * r.getHeight(), 1.5f);
        else g.drawLine (r.getX() + tt * r.getWidth(), r.getY() - 2.0f * u, r.getX() + tt * r.getWidth(), r.getBottom() + 2.0f * u, 1.5f);
    }
}
