#include "PluginProcessor.h"
#include "PluginEditor.h"

ScopeProcessor::ScopeProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

ScopeProcessor::~ScopeProcessor() { engine.stop(); }

bool ScopeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& in = l.getMainInputChannelSet();
    const auto& out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void ScopeProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    isMono = getTotalNumInputChannels() < 2;
    engine.start (sr, isMono);
}

void ScopeProcessor::releaseResources() { engine.stop(); }

void ScopeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // l'analyseur ne modifie jamais le signal : on lit seulement
    mask::HostInfo h;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            h.haveHost = true;
            h.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) h.bpm = *bpm;
            if (auto ts = pos->getTimeSignature()) { h.sigNum = ts->numerator; h.sigDen = ts->denominator; }
        }
    }
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() >= 1 && n > 0)
    {
        const float* L = buffer.getReadPointer (0);
        const float* R = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : L;
        engine.process (L, R, n, h);
    }
}

juce::AudioProcessorEditor* ScopeProcessor::createEditor() { return new ScopeEditor (*this); }

void ScopeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement x ("MaskScope");
    x.setAttribute ("version", 1);
    ui.save (x);
    copyXmlToBinary (x, dest);
}

void ScopeProcessor::setStateInformation (const void* data, int size)
{
    if (auto x = getXmlFromBinary (data, size))
        if (x->hasTagName ("MaskScope")) ui.load (*x);
}

#ifndef MASK_TESTING
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ScopeProcessor(); }
#endif
