// Rend l'éditeur hors écran avec un signal synthétique et enregistre des captures PNG.
// usage : editor_test <dossier_sortie>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <random>

struct EditorAccess
{
    static juce::Rectangle<float> spg (ScopeEditor& e) { return e.spgRect; }
    static juce::Rectangle<float> bars (ScopeEditor& e) { return e.barsRect; }
    static juce::Rectangle<float> env (ScopeEditor& e) { return e.envRect; }
    static juce::Rectangle<float> spectrum (ScopeEditor& e) { return e.specPlot; }
    static juce::Rectangle<float> hitPanel (ScopeEditor& e) { return e.hitRect; }
    static juce::Rectangle<float> hist (ScopeEditor& e) { return e.histPlot; }
    static juce::Rectangle<float> scope (ScopeEditor& e) { return e.scopeRect; }
    static void setTarget (ScopeEditor& e, int t) { e.ui.target = t; }
};

static void saveShot (ScopeEditor& ed, const juce::File& dir, const juce::String& name)
{
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    juce::File f = dir.getChildFile (name + ".png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("  capture : %s\n", f.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? argv[1] : "/tmp/shots");
    outDir.createDirectory();

    const double fs = 48000.0;
    ScopeProcessor proc;
    proc.setPlayConfigDetails (2, 2, fs, 512);
    proc.prepareToPlay (fs, 512);
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<ScopeEditor*> (base.get());
    ed->setSize (1280, 800);

    // --- signal : kick (tombe de 160 à 52 Hz) + 808 sinus + guitares saturées G/D + hats + voix « formantique », ~168 BPM, limité
    const double bpm = 168.0;
    const int total = (int) (fs * 24.0);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    std::mt19937 rng (3);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    const int beat = (int) (fs * 60.0 / bpm);
    std::vector<double> kick ((size_t) (fs * 0.35));
    { double ph = 0; for (size_t i = 0; i < kick.size(); ++i) { const double t = (double) i / fs; ph += 2 * juce::MathConstants<double>::pi * (52.0 + 110.0 * std::exp (-t / 0.025)) / fs; kick[i] = std::tanh (1.6 * std::sin (ph)) * std::exp (-t / 0.11) * std::min (1.0, t / 0.002) ; } }
    double gph[6] {}; const double gf[6] = { 98.0, 147.0, 196.0, 293.7, 98.5, 196.9 };
    float hp = 0, lpv = 0;
    for (int i = 0; i < total; ++i)
    {
        const int inBeat = i % beat;
        const int bar = (i / beat) % 4;
        double k = (inBeat < (int) kick.size()) ? kick[(size_t) inBeat] : 0.0;
        double g1 = 0, g2 = 0;
        for (int v = 0; v < 6; ++v) { gph[v] += gf[v] / fs; gph[v] -= std::floor (gph[v]); const double s = 2 * gph[v] - 1; if (v < 3) g1 += s; else g2 += s; }
        g1 = std::tanh (1.4 * g1 / 3.0); g2 = std::tanh (1.4 * g2 / 3.0);
        // hats sur les contretemps
        const int hb = (i + beat / 2) % beat;
        float hat = 0; if (hb < (int) (fs * 0.04)) { const float x = nd (rng); hat = (x - hp) * (float) std::exp (-(double) hb / (fs * 0.008)); hp = x; }
        // « voix » : bruit filtré autour de 3 kHz, portée par bars
        lpv += 0.2f * (nd (rng) - lpv);
        const double voiceEnv = (bar % 2 == 0) ? 0.5 + 0.5 * std::sin (2 * juce::MathConstants<double>::pi * 0.7 * i / fs) : 0.1;
        const float voice = (float) (voiceEnv * 0.12 * std::sin (2 * juce::MathConstants<double>::pi * 440.0 * i / fs) * (1.0 + 0.3 * lpv));
        const double sub = 0.25 * std::sin (2 * juce::MathConstants<double>::pi * 52.0 * i / fs) * (bar < 3 ? 1.0 : 0.0);
        double l = 0.85 * k + sub + 0.30 * g1 + 0.06 * g2 + 0.14 * hat + voice;
        double r = 0.85 * k + sub + 0.06 * g1 + 0.30 * g2 + 0.12 * hat + voice;
        L[(size_t) i] = (float) std::tanh (1.9 * l);
        R[(size_t) i] = (float) std::tanh (1.9 * r);
    }
    for (int i = 0; i < total; ++i) { L[(size_t) i] *= 0.98f; R[(size_t) i] *= 0.98f; }

    mask::HostInfo h; h.haveHost = true; h.playing = true; h.bpm = bpm;
    const int B = 480;
    for (int i = 0; i < total; i += B)
    {
        proc.engine.process (L.data() + i, R.data() + i, std::min (B, total - i), h);
        if ((i / B) % 100 == 99) { std::this_thread::sleep_for (std::chrono::milliseconds (60)); ed->testTick(); }
    }
    std::this_thread::sleep_for (std::chrono::milliseconds (500));
    for (int k = 0; k < 4; ++k) { ed->testTick(); std::this_thread::sleep_for (std::chrono::milliseconds (80)); }

    std::printf ("rendu des captures dans %s\n", outDir.getFullPathName().toRawUTF8());
    ed->testHover ({ -10, -10 }, false);
    const char* names[] = { "overview", "spectrogram", "spectrum", "soundfield", "loudness", "history", "levels", "export" };
    for (int t = 0; t < mask::kTabs; ++t)
    {
        ed->testSetTab (t);
        ed->testTick();
        saveShot (*ed, outDir, juce::String (t) + "_" + names[t]);
    }
    // bulles
    ed->testSetTab (mask::TabSpectrogram);
    ed->testTick(); saveShot (*ed, outDir, "tmp");
    auto sr = EditorAccess::spg (*ed);
    ed->testHover ({ sr.getX() + sr.getWidth() * 0.92f, sr.getBottom() - sr.getHeight() * 0.18f }, true);
    saveShot (*ed, outDir, "9_tip_spectrogram");
    auto hr = EditorAccess::hitPanel (*ed);
    ed->testHover ({ hr.getX() + hr.getWidth() * 0.5f, hr.getY() + hr.getHeight() * 0.5f }, true);
    saveShot (*ed, outDir, "9_tip_adsr");
    ed->testSetTab (mask::TabOverview);
    ed->testTick(); saveShot (*ed, outDir, "tmp");
    auto er = EditorAccess::env (*ed);
    ed->testHover ({ er.getX() + er.getWidth() * 0.5f, er.getCentreY() }, true);
    saveShot (*ed, outDir, "9_tip_overview_env");
    // export vers d'autres supports
    for (int tg : { mask::TargetConcert, mask::TargetCd })
    {
        EditorAccess::setTarget (*ed, tg);
        ed->testSetTab (mask::TabExport);
        ed->testHover ({ -10, -10 }, false);
        ed->testTick();
        saveShot (*ed, outDir, juce::String ("8_export_") + mask::targetName (tg));
    }
    // --- phase 2 : kick seul (pour vérifier l'analyse A D S R) après remise à zéro
    proc.engine.reset();
    {
        const int n2 = (int) (fs * 8.0);
        std::vector<float> K ((size_t) n2, 0.0f);
        for (int b = 0; b * beat < n2; ++b)
            for (size_t i = 0; i < kick.size() && (size_t) (b * beat) + i < (size_t) n2; ++i) K[(size_t) (b * beat) + i] = (float) (0.9 * kick[i]);
        for (int i = 0; i < n2; i += B)
        {
            proc.engine.process (K.data() + i, K.data() + i, std::min (B, n2 - i), h);
            if ((i / B) % 100 == 99) { std::this_thread::sleep_for (std::chrono::milliseconds (60)); ed->testTick(); }
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (500));
        for (int k = 0; k < 4; ++k) { ed->testTick(); std::this_thread::sleep_for (std::chrono::milliseconds (80)); }
        EditorAccess::setTarget (*ed, mask::TargetSpotify);
        ed->testSetTab (mask::TabSpectrogram);
        ed->testHover ({ -10, -10 }, false);
        ed->testTick();
        saveShot (*ed, outDir, "10_kick_spectrogram");
        auto hr2 = EditorAccess::hitPanel (*ed);
        ed->testHover ({ hr2.getX() + hr2.getWidth() * 0.15f, hr2.getBottom() - 40.0f }, true);
        saveShot (*ed, outDir, "10_kick_tip_attack");
        ed->testSetTab (mask::TabOverview);
        ed->testHover ({ -10, -10 }, false);
        ed->testTick();
        saveShot (*ed, outDir, "10_kick_overview");
    }
    // --- tailles extrêmes
    for (auto sz : { std::pair<int, int> { 1100, 680 }, { 1920, 1080 } })
    {
        ed->setSize (sz.first, sz.second);
        for (int t : { (int) mask::TabOverview, (int) mask::TabExport })
        {
            ed->testSetTab (t); ed->testHover ({ -10, -10 }, false); ed->testTick();
            saveShot (*ed, outDir, juce::String ("11_size") + juce::String (sz.first) + "_tab" + juce::String (t));
        }
    }
    ed->setSize (1280, 800);
    // --- temps de rendu par onglet
    for (int t = 0; t < mask::kTabs; ++t)
    {
        ed->testSetTab (t);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int k = 0; k < 10; ++k) { ed->testTick(); auto img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f); }
        std::printf ("  rendu logiciel (Linux) onglet %d : %.1f ms / image\n", t, (juce::Time::getMillisecondCounterHiRes() - t0) / 10.0);
    }
    proc.engine.stop();
    return 0;
}
