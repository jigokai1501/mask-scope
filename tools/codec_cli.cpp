// Outil de calibration : lit un fichier f32le stéréo (48 kHz), applique CodecSim avec un profil donné.
// usage : codec_cli fichier.f32 snr tilt cutoff   → "max avg hfloss seconds"
#include "../Source/Dsp/Codec.h"
#include <cstdio>
using namespace mask;
int main (int argc, char** argv)
{
    if (argc < 5) return 1;
    FILE* f = std::fopen (argv[1], "rb");
    if (! f) return 2;
    std::vector<float> buf;
    float tmp[8192];
    size_t n;
    while ((n = std::fread (tmp, sizeof (float), 8192, f)) > 0) buf.insert (buf.end(), tmp, tmp + n);
    std::fclose (f);
    const size_t frames = buf.size() / 2;
    std::vector<float> L (frames), R (frames);
    for (size_t i = 0; i < frames; ++i) { L[i] = buf[2 * i]; R[i] = buf[2 * i + 1]; }
    CodecProfile p { "cal", 0, (float) std::atof (argv[2]), (float) std::atof (argv[3]), (float) std::atof (argv[4]) };
    CodecSim sim;
    sim.prepare (48000.0, p);
    for (size_t i = 0; i < frames; i += 480) sim.process (L.data() + i, R.data() + i, (int) std::min<size_t> (480, frames - i));
    const auto& r = sim.result();
    std::printf ("%.3f %.3f %.2f %u\n", r.overshootMaxDb, r.overshootAvgDb, r.hfLossDb, r.seconds);
    return 0;
}
