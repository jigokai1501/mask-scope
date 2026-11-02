"""Ajuste (snr, tilt) de CodecSim sur les mesures réelles libvorbis (real.json). Usage : calibrate_codec.py /tmp/cal"""
import json, subprocess, sys, itertools, numpy as np
d = sys.argv[1]
real = json.load(open(f'{d}/real.json'))
kinds = ('punk', 'hyper', 'noise', 'techno')
cuts = {96: 16700.0, 160: 21200.0, 320: 23500.0}
def sim(kind, snr, tilt, cut):
    out = subprocess.run(['/tmp/codec_cli', f'{d}/{kind}.f32', str(snr), str(tilt), str(cut)], capture_output=True, text=True).stdout.split()
    return float(out[0]), float(out[1])
for k in (96, 160, 320):
    tm = np.mean([real[f'{x}/libvorbis{k}']['max'] for x in kinds]); ta = np.mean([real[f'{x}/libvorbis{k}']['avg'] for x in kinds])
    best = None
    for snr, tilt in itertools.product(np.arange(6, 42, 3.0), (0.0, 0.5, 1.0, 1.5, 2.0)):
        r = [sim(x, snr, tilt, cuts[k]) for x in kinds]
        sm = np.mean([a for a, _ in r]); sa = np.mean([b for _, b in r])
        # erreur sur la moyenne + sur chaque signal (max)
        per = np.mean([abs(r[i][0] - real[f'{x}/libvorbis{k}']['max']) for i, x in enumerate(kinds)])
        err = abs(sm - tm) + abs(sa - ta) + 0.5 * per
        if best is None or err < best[0]: best = (err, snr, tilt, sm, sa, [round(a, 2) for a, _ in r])
    print(f'{k:3d} kbps  cible max {tm:.2f} avg {ta:.2f}  ->  snr {best[1]} tilt {best[2]}  sim max {best[3]:.2f} avg {best[4]:.2f}  par signal {best[5]}  (réel {[round(real[f"{x}/libvorbis{k}"]["max"],2) for x in kinds]})')
