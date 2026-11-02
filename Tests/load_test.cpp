// Charge le .vst3 construit comme le ferait un hôte : instanciation, traitement audio, état, éditeur.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) return 2;
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile (descs, argv[1]);
    std::printf ("types trouvés : %d\n", descs.size());
    if (descs.isEmpty()) return 3;
    std::printf ("nom : %s · fabricant : %s · catégorie : %s · entrées %d sorties %d\n", descs[0]->name.toRawUTF8(), descs[0]->manufacturerName.toRawUTF8(), descs[0]->category.toRawUTF8(), descs[0]->numInputChannels, descs[0]->numOutputChannels);
    juce::String err;
    auto inst = fmt.createInstanceFromDescription (*descs[0], 48000.0, 512, err);
    if (! inst) { std::printf ("échec : %s\n", err.toRawUTF8()); return 4; }
    inst->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double maxDiff = 0;
    for (int b = 0; b < 400; ++b)
    {
        for (int i = 0; i < 512; ++i) { const float v = 0.4f * std::sin (2 * juce::MathConstants<float>::pi * 440.0f * (float) (b * 512 + i) / 48000.0f); buf.setSample (0, i, v); buf.setSample (1, i, 0.5f * v); }
        juce::AudioBuffer<float> ref (buf);
        inst->processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) maxDiff = std::max (maxDiff, (double) std::abs (buf.getSample (c, i) - ref.getSample (c, i)));
    }
    std::printf ("signal modifié par l'analyseur : écart max %.3g (doit être 0)\n", maxDiff);
    juce::MemoryBlock st;
    inst->getStateInformation (st);
    inst->setStateInformation (st.getData(), (int) st.getSize());
    std::printf ("état : %zu octets, aller-retour OK\n", st.getSize());
    if (auto* ed = inst->createEditorIfNeeded())
    {
        ed->setSize (1280, 800);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        std::printf ("éditeur créé : %dx%d, image %dx%d\n", ed->getWidth(), ed->getHeight(), img.getWidth(), img.getHeight());
        inst->editorBeingDeleted (ed);
        delete ed;
    }
    inst->releaseResources();
    inst.reset();
    std::printf ("OK\n");
    return maxDiff == 0.0 ? 0 : 5;
}
