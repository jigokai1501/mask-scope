#pragma once
#include "PluginProcessor.h"
#include "SpgRenderer.h"
#include "Dsp/Tips.h"

class ScopeEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::TextEditor::Listener
{
public:
    explicit ScopeEditor (ScopeProcessor&);
    ~ScopeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override { mouseMove (e); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // utilisés par les tests : force un état de survol / une action sans fenêtre native
    void testHover (juce::Point<float> p, bool forceBubble);
    void testSetTab (int t) { ui.tab = t; }
    void testTick() { timerCallback(); }

private:
    ScopeProcessor& proc;
    mask::Engine& eng;
    UiState& ui;
    friend struct EditorAccess;
    enum Action { A_None = 0, A_Tab, A_Tips, A_Reset, A_Hold, A_Settings, A_Menu, A_Bpm, A_BpmAuto, A_Coach, A_Toggle, A_Seg };
    enum Ctl { C_SpgSource, C_SpgTime, C_SpgRange, C_SpgTilt, C_SpgPalette, C_SpgRes, C_SpFft, C_SpSmooth, C_SpSpeed, C_SpTilt, C_SpRange,
               C_SpAvg, C_SpPeak, C_SpMode, C_SfMode, C_HistSpan, C_Spot, C_ExCodec, C_Target, C_Style, C_Dwell, C_AutoReset, C_HitThresh, C_Count };

    struct Hit
    {
        juce::Rectangle<float> r;
        int action = A_None, arg = 0, arg2 = 0, region = mask::RNone, moduleTab = -1;
    };

    // ---- cadre commun
    float u = 1.0f;                              // facteur d'échelle de l'interface
    std::vector<Hit> hits;
    void addHit (juce::Rectangle<float> r, int action, int arg, int region, int moduleTab = -1, int arg2 = 0);
    const Hit* hitAt (juce::Point<float> p, bool needAction) const;
    void txt (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Colour, juce::Justification = juce::Justification::centredLeft, bool bold = false) const;
    void txt (juce::Graphics& g, const char* s, juce::Rectangle<float> r, float size, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false) const { txt (g, juce::String::fromUTF8 (s), r, size, c, j, bold); }
    void chip (juce::Graphics&, juce::Rectangle<float>, int ctl, int region, const char* label = nullptr, juce::Colour c = juce::Colour());
    void button (juce::Graphics&, juce::Rectangle<float>, const juce::String& label, bool on, int action, int arg, int region, juce::Colour onColour);
    void button (juce::Graphics& g, juce::Rectangle<float> r, const char* label, bool on, int action, int arg, int region, juce::Colour onColour) { button (g, r, juce::String::fromUTF8 (label), on, action, arg, region, onColour); }
    juce::Rectangle<float> panel (juce::Graphics&, juce::Rectangle<float>, const juce::String& title, juce::Colour c, int tab, bool expanded);
    void para (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float size, juce::Colour c, int maxLines) const { g.setColour (c); g.setFont (juce::FontOptions (size * u)); g.drawFittedText (s, r.toNearestInt(), juce::Justification::topLeft, maxLines, 0.9f); }
    void rule (juce::Graphics&, float x1, float y1, float x2, float y2, juce::Colour c, float th = 1.0f) const;

    // ---- contrôles
    int getCtl (int id) const;
    void setCtl (int id, int v);
    juce::StringArray ctlOptions (int id) const;
    juce::String ctlName (int id) const;
    void showMenu (int ctl, juce::Rectangle<int> anchor);
    void showSettingsMenu (juce::Rectangle<int> anchor);

    // ---- instantané par image
    mask::MeterBlock m;
    mask::AnalysisBlock a;
    mask::SpectrumBlock* sb = nullptr;           // alloué (61 Ko)
    std::unique_ptr<mask::SpectrumBlock> sbOwner;
    bool haveSb = false;
    mask::CoachCtx cctx;
    std::vector<mask::Advice> advice;
    mask::CoachEngine coach;
    double bpm = 0.0;
    bool bpmFromHost = false;
    double lastCoachSec = 0.0;
    int learnIndex = 0;
    double lastLearnSec = 0.0;
    mask::HitDetail hitDetail;
    bool haveHitDetail = false;
    uint32_t lastHitSerial = 0;
    void timerCallback() override;
    void takeSnapshot();

    // ---- dessin
    void drawHeader (juce::Graphics&, juce::Rectangle<float>);
    void drawCoach (juce::Graphics&, juce::Rectangle<float>);
    void drawOverview (juce::Graphics&, juce::Rectangle<float>);
    void drawTooltip (juce::Graphics&);
    void drawModule (juce::Graphics&, int tab, juce::Rectangle<float> r, bool expanded);
    void drawSpectrogram (juce::Graphics&, juce::Rectangle<float>, bool expanded);
    void drawSpectrum (juce::Graphics&, juce::Rectangle<float>, bool expanded);
    void drawSoundField (juce::Graphics&, juce::Rectangle<float>, bool expanded);
    void drawLoudness (juce::Graphics&, juce::Rectangle<float>, bool expanded);
    void drawHistory (juce::Graphics&, juce::Rectangle<float>, bool expanded);
    void drawLevels (juce::Graphics&, juce::Rectangle<float>, bool expanded);
    void drawExport (juce::Graphics&, juce::Rectangle<float>);
    void drawHitPanel (juce::Graphics&, juce::Rectangle<float>, bool expanded);
    void drawBandBars (juce::Graphics&, juce::Rectangle<float>, bool vertical);
    void drawGaugeBar (juce::Graphics&, juce::Rectangle<float> r, float v, float lo, float hi, float target, juce::Colour c, bool vertical) const;
    void tile (juce::Graphics&, juce::Rectangle<float>, const juce::String& label, const juce::String& value, const juce::String& unit, juce::Colour c, int region);

    // ---- état de la souris / bulles
    juce::Point<float> mouse;
    bool mouseInside = false;
    double lastMoveSec = 0.0;
    juce::Point<float> bubbleAnchor;
    bool forceBubble = false;
    mask::Hover hover;
    int hoverRegion = mask::RNone;
    int hoverHitSeg = -1;
    juce::TextEditor bpmEdit;
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { bpmEdit.setVisible (false); }
    void textEditorFocusLost (juce::TextEditor&) override { bpmEdit.setVisible (false); }

    // ---- spectrogramme
    SpgRenderer spg;
    juce::Rectangle<float> spgRect, barsRect, envRect, hitRect;   // géométrie du dernier dessin (pour le survol)
    float spgFmax = 20000.0f;
    float specFloor = -90.0f, specCeil = -10.0f;
    juce::Rectangle<float> specPlot, histPlot, scopeRect, histBars;
    float histT0 = 0.0f, histT1 = 0.0f, histLo = -40.0f, histHi = 0.0f;
    uint64_t histFirst = 0, histLast = 0;
    int histStride = 1;
    // vectorscope
    juce::Image scopeImg;
    uint64_t scopeLast = 0;
    float polarHist[181] {};
    std::vector<float> corrTrail;
    float dispPk[2] { -120, -120 }, dispTp[2] { -120, -120 };
    double lastDispSec = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopeEditor)
};
