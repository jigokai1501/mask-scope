// mask::scope — simulation indicative d'un codec à perte (quantification spectrale par bandes)
// But : estimer de combien l'encodage fait « dépasser » les crêtes (true peak) et quelles aigus sont perdus.
// Ce n'est PAS un vrai encodeur Vorbis/AAC : modèle calibré sur des encodages réels (voir tools/calibrate_codec.py).
#pragma once
#include "Fft.h"
#include "Loudness.h"

namespace mask
{
struct CodecProfile
{
    const char* name;     // affichage
    int kbps;
    float snrDb;          // rapport signal/bruit visé par bande à 200 Hz
    float tiltDbPerOct;   // le SNR baisse avec la fréquence (allocation de bits)
    float cutoffHz;       // passe-bas du codec
};

// Valeurs ajustées sur de vrais encodages libvorbis (tools/measure_real_codecs.py + calibrate_codec.py).
// Incertitude constatée entre signaux : ≈ ±1,5 dB à 96 kbps, ±0,7 dB à 160 kbps, ±0,1 dB à 320 kbps.
inline const CodecProfile* codecProfiles (int& n)
{
    static const CodecProfile p[] = {
        { "OGG 96",  96,  21.0f, 1.5f, 16700.0f },
        { "OGG 160", 160, 18.0f, 1.0f, 21200.0f },
        { "OGG 320", 320, 33.0f, 0.5f, 23500.0f },
    };
    n = 3;
    return p;
}

struct CodecResult
{
    float overshootMaxDb = 0.0f;   // pire écart de true peak (décodé - original) sur une seconde
    float overshootAvgDb = 0.0f;   // moyenne sur les secondes « fortes »
    float hfLossDb = 0.0f;         // énergie perdue au-dessus de la coupure (dB, négatif)
    uint32_t seconds = 0;
    uint8_t valid = 0;
};

class CodecSim
{
public:
    static constexpr int kN = 2048, kH = 1024;

    void prepare (double sampleRate, const CodecProfile& prof)
    {
        fs = sampleRate;
        profile = prof;
        fft.init (kN);
        win.resize (kN);
        for (int i = 0; i < kN; ++i) win[(size_t) i] = (float) std::sin (kPi * (i + 0.5) / kN);
        re.assign (kN, 0.0f); im.assign (kN, 0.0f);
        accL.assign (kN, 0.0f); accR.assign (kN, 0.0f);
        tpOL.prepare (fs); tpOR.prepare (fs); tpRL.prepare (fs); tpRR.prepare (fs);
        // groupes de bins ≈ tiers d'octave, au moins 4 bins
        groups.clear();
        const double binHz = fs / kN;
        int k = 1;
        while (k < kN / 2)
        {
            int w = std::max (4, (int) std::lround (k * 0.26));
            const int e = std::min (kN / 2, k + w);
            groups.push_back ({ k, e, (float) (0.5 * (k + e) * binHz) });
            k = e;
        }
        cutBin = (int) std::min ((double) kN / 2, profile.cutoffHz / binHz);
        reset();
    }
    void reset()
    {
        pos = 0; secSamples = 0; blkO = blkR = 0.0f; res = CodecResult {};
        nLoud = 0; sumOver = 0.0;
        eIn = eCut = 0.0;
        std::fill (accL.begin(), accL.end(), 0.0f); std::fill (accR.begin(), accR.end(), 0.0f);
        tpOL.reset(); tpOR.reset(); tpRL.reset(); tpRR.reset();
        inL.assign (kN, 0.0f); inR.assign (kN, 0.0f); fill = 0;
        started = true;
    }
    const CodecResult& result() const noexcept { return res; }

    // Entrée en flux : original (stéréo). Les mesures utilisent des blocs d'environ 1 s.
    void process (const float* L, const float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            inL[(size_t) fill] = L[i]; inR[(size_t) fill] = R[i];
            ++fill;
            if (fill == kN)
            {
                processFrame();
                // glissement de kH
                std::memmove (inL.data(), inL.data() + kH, sizeof (float) * kH);
                std::memmove (inR.data(), inR.data() + kH, sizeof (float) * kH);
                fill = kH;
            }
        }
    }

private:
    struct Group { int b0, b1; float fc; };

    void processFrame()
    {
        // analyse : fenêtre sinus, L + iR
        for (int i = 0; i < kN; ++i) { re[(size_t) i] = inL[(size_t) i] * win[(size_t) i]; im[(size_t) i] = inR[(size_t) i] * win[(size_t) i]; }
        fft.forward (re.data(), im.data());
        // quantification par groupe sur L et R séparément (via extraction)
        std::vector<float>& zr = re; std::vector<float>& zi = im;
        // extraction XL, XR sur 0..N/2 puis reconstruction hermitienne
        xl_r.assign (kN / 2 + 1, 0.0f); xl_i = xr_r = xr_i = xl_r;
        for (int k = 0; k <= kN / 2; ++k)
        {
            const int k2 = (kN - k) & (kN - 1);
            const float cr = zr[(size_t) k2], ci = -zi[(size_t) k2];
            const float lr = 0.5f * (zr[(size_t) k] + cr), li = 0.5f * (zi[(size_t) k] + ci);
            const float dr = zr[(size_t) k] - cr, di = zi[(size_t) k] - ci;
            xl_r[(size_t) k] = lr; xl_i[(size_t) k] = li;
            xr_r[(size_t) k] = 0.5f * di; xr_i[(size_t) k] = -0.5f * dr;
        }
        for (int k = 0; k <= kN / 2; ++k) { eIn += (double) xl_r[(size_t) k] * xl_r[(size_t) k] + xl_i[(size_t) k] * xl_i[(size_t) k] + xr_r[(size_t) k] * xr_r[(size_t) k] + xr_i[(size_t) k] * xr_i[(size_t) k]; }
        for (auto& g : groups)
        {
            const float f = g.fc;
            const bool cut = g.b0 >= cutBin;
            double eL = 0, eR = 0; const int cnt = g.b1 - g.b0;
            for (int k = g.b0; k < g.b1; ++k)
            {
                eL += (double) xl_r[(size_t) k] * xl_r[(size_t) k] + (double) xl_i[(size_t) k] * xl_i[(size_t) k];
                eR += (double) xr_r[(size_t) k] * xr_r[(size_t) k] + (double) xr_i[(size_t) k] * xr_i[(size_t) k];
            }
            const float rmsL = (float) std::sqrt (eL / (2.0 * cnt)), rmsR = (float) std::sqrt (eR / (2.0 * cnt));
            const float octaves = std::max (0.0f, std::log2 (std::max (f, 1.0f) / 200.0f));
            const float snr = profile.snrDb - profile.tiltDbPerOct * octaves;
            const float stepK = 3.4641016f * std::pow (10.0f, -snr / 20.0f); // sqrt(12)
            const float dL = std::max (1e-9f, rmsL * stepK), dR = std::max (1e-9f, rmsR * stepK);
            for (int k = g.b0; k < g.b1; ++k)
            {
                auto q = [] (float v, float d) { return d * std::round (v / d); };
                if (cut) { eCut += (double) xl_r[(size_t) k] * xl_r[(size_t) k] + xl_i[(size_t) k] * xl_i[(size_t) k] + xr_r[(size_t) k] * xr_r[(size_t) k] + xr_i[(size_t) k] * xr_i[(size_t) k]; xl_r[(size_t) k] = xl_i[(size_t) k] = xr_r[(size_t) k] = xr_i[(size_t) k] = 0; continue; }
                xl_r[(size_t) k] = q (xl_r[(size_t) k], dL); xl_i[(size_t) k] = q (xl_i[(size_t) k], dL);
                xr_r[(size_t) k] = q (xr_r[(size_t) k], dR); xr_i[(size_t) k] = q (xr_i[(size_t) k], dR);
            }
        }
        // recomposition Z = XL + i·XR (hermitien pour L et R réels)
        for (int k = 0; k <= kN / 2; ++k)
        {
            const float lr = xl_r[(size_t) k], li = xl_i[(size_t) k], rr = xr_r[(size_t) k], ri = xr_i[(size_t) k];
            re[(size_t) k] = lr - ri; im[(size_t) k] = li + rr;
            if (k > 0 && k < kN / 2) { re[(size_t) (kN - k)] = lr + ri; im[(size_t) (kN - k)] = -li + rr; }
        }
        fft.inverse (re.data(), im.data());
        const float inv = 1.0f / kN;
        // synthèse + recouvrement-addition
        for (int i = 0; i < kN; ++i)
        {
            accL[(size_t) i] += re[(size_t) i] * inv * win[(size_t) i];
            accR[(size_t) i] += im[(size_t) i] * inv * win[(size_t) i];
        }
        // sortie : les kH premiers échantillons sont complets
        // original [mH, mH+kH) = inL/inR[0..kH) du cadre courant ; décodé = accL/accR[0..kH)
        for (int i = 0; i < kH; ++i)
        {
            blkO = std::max (blkO, std::max (tpOL.process (inL[(size_t) i]), tpOR.process (inR[(size_t) i])));
            blkR = std::max (blkR, std::max (tpRL.process (accL[(size_t) i]), tpRR.process (accR[(size_t) i])));
        }
        secSamples += kH;
        if (secSamples >= (int) fs) finishSecond();
        // décalage des accumulateurs
        std::memmove (accL.data(), accL.data() + kH, sizeof (float) * kH);
        std::memmove (accR.data(), accR.data() + kH, sizeof (float) * kH);
        std::fill (accL.begin() + kH, accL.end(), 0.0f);
        std::fill (accR.begin() + kH, accR.end(), 0.0f);
        pos += kH;
    }

    void finishSecond()
    {
        secSamples = 0;
        const float o = toDbAmp (blkO), r = toDbAmp (blkR);
        blkO = blkR = 0.0f;
        if (o > -30.0f)
        {
            const float d = r - o;
            res.overshootMaxDb = res.seconds == 0 ? d : std::max (res.overshootMaxDb, d);
            if (o > -12.0f) { ++nLoud; sumOver += d; res.overshootAvgDb = (float) (sumOver / nLoud); }
            ++res.seconds;
            res.valid = 1;
        }
        res.hfLossDb = eIn > 1e-12 ? (float) (10.0 * std::log10 (std::max (1e-9, eCut / eIn))) : 0.0f;
    }

    double fs = 48000.0;
    CodecProfile profile { "", 0, 20, 1, 20000 };
    Fft fft;
    std::vector<float> win, re, im, accL, accR, inL, inR, xl_r, xl_i, xr_r, xr_i;
    std::vector<Group> groups;
    int cutBin = kN / 2, fill = 0, secSamples = 0, nLoud = 0;
    uint64_t pos = 0;
    bool started = false;
    float blkO = 0, blkR = 0;
    double sumOver = 0, eIn = 0, eCut = 0;
    TruePeak tpOL, tpOR, tpRL, tpRR;
    CodecResult res;
};

} // namespace mask
