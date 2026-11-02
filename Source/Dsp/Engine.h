// mask::scope — assemblage : thread audio (mesures exactes) + thread d'analyse (spectre, enveloppe, codec)
#pragma once
#include "Analysis.h"

namespace mask
{
struct HostInfo
{
    bool haveHost = false;     // l'hôte fournit une position / un transport
    bool playing = false;
    double bpm = 0.0;          // 0 = inconnu
    int sigNum = 4, sigDen = 4;
};

class Engine
{
public:
    Engine() : analyzer (ring, settings) {}
    ~Engine() { stop(); }

    Settings settings;
    StereoRing ring;
    MeterEngine meters;
    Analyzer analyzer;

    std::atomic<float> hostBpm { 0.0f };
    std::atomic<int> hostSigNum { 4 }, hostSigDen { 4 };
    std::atomic<bool> hostPlaying { false }, hostKnown { false };

    void start (double sampleRate, bool mono)
    {
        stop();
        fs = sampleRate;
        isMono = mono;
        ring.clear();
        meters.prepare (fs, mono);
        activeCount = 0;
        prevPlaying = false;
        analyzer.start (fs);
        running = true;
    }
    void stop()
    {
        if (running) analyzer.stop();
        running = false;
    }
    void reset()
    {
        meters.requestReset();
        analyzer.resetSession();
    }

    // thread audio : n'altère jamais le signal
    void process (const float* L, const float* R, int n, const HostInfo& h) noexcept
    {
        if (! running || n <= 0) return;
        const float* RR = isMono ? L : R;
        hostBpm.store ((float) h.bpm, std::memory_order_relaxed);
        hostSigNum.store (h.sigNum, std::memory_order_relaxed);
        hostSigDen.store (h.sigDen, std::memory_order_relaxed);
        hostPlaying.store (h.playing, std::memory_order_relaxed);
        hostKnown.store (h.haveHost, std::memory_order_relaxed);
        if (h.haveHost && h.playing && ! prevPlaying && settings.autoReset.load (std::memory_order_relaxed)) reset();
        prevPlaying = h.playing;

        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (RR[i])));
        if (pk > 3.0e-5f || (h.haveHost && h.playing)) activeCount = (int) (fs * 0.6);
        else activeCount = std::max (0, activeCount - n);
        analyzer.active.store (activeCount > 0, std::memory_order_relaxed);

        meters.process (L, RR, n, h.playing, h.haveHost);
        ring.push (L, RR, n);
    }

private:
    double fs = 48000.0;
    bool isMono = false, running = false, prevPlaying = false;
    int activeCount = 0;
};

} // namespace mask
