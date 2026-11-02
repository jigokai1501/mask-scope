"""Encode avec ffmpeg (libvorbis, aac), décode, aligne et mesure le dépassement de true peak par seconde (comme CodecSim)."""
import numpy as np, subprocess, sys, json, os
from scipy.signal import resample_poly, welch
fs = 48000
def load(path): return np.fromfile(path, dtype='<f4').reshape(-1, 2)
def tp_blocks(x):
    up = np.stack([resample_poly(x[:, c], 4, 1) for c in range(2)], 1)
    n = len(up) // (4 * fs)
    return np.array([20 * np.log10(max(np.max(np.abs(up[i * 4 * fs:(i + 1) * 4 * fs])), 1e-9)) for i in range(n)])
def run(cmd): subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def codec_roundtrip(src, tag, codec, kbps, workdir):
    wav = f'{workdir}/{tag}.wav'
    run(['ffmpeg', '-y', '-f', 'f32le', '-ar', str(fs), '-ac', '2', '-i', src, '-c:a', 'pcm_f32le', wav])
    enc = f'{workdir}/{tag}_{codec}{kbps}.{"ogg" if codec == "libvorbis" else "m4a"}'
    run(['ffmpeg', '-y', '-i', wav, '-c:a', codec, '-b:a', f'{kbps}k', enc])
    dec = f'{workdir}/{tag}_{codec}{kbps}.f32'
    run(['ffmpeg', '-y', '-i', enc, '-f', 'f32le', '-ar', str(fs), '-ac', '2', dec])
    return load(dec)
def align(a, b):
    n = min(len(a), len(b), fs * 8)
    A = np.fft.rfft(a[:n, 0], 2 * n); B = np.fft.rfft(b[:n, 0], 2 * n)
    c = np.fft.irfft(B * np.conj(A))
    lag = int(np.argmax(np.abs(c)));
    if lag > n: lag -= 2 * n
    return lag
def cutoff(orig, dec):
    f, po = welch(orig[:, 0], fs, nperseg=4096); _, pd = welch(dec[:, 0], fs, nperseg=4096)
    ratio = 10 * np.log10((pd + 1e-20) / (po + 1e-20))
    idx = np.where((f > 6000) & (ratio < -25))[0]
    return float(f[idx[0]]) if len(idx) else 24000.0
if __name__ == '__main__':
    d = sys.argv[1]; res = {}
    for kind in ('punk', 'hyper', 'noise', 'techno'):
        src = f'{d}/{kind}.f32'; o = load(src)
        for codec, rates in (('libvorbis', (96, 160, 320)),):
            for k in rates:
                dec = codec_roundtrip(src, kind, codec, k, d)
                lag = align(o, dec)
                if lag > 0: dec = dec[lag:]
                else: o2 = o[-lag:]
                oo = o if lag >= 0 else o[-lag:]
                m = min(len(oo), len(dec)); oo = oo[:m]; dd = dec[:m]
                to, td = tp_blocks(oo), tp_blocks(dd)
                over = td - to
                loud = to > -12
                res[f'{kind}/{codec}{k}'] = dict(max=float(over.max()), avg=float(over[loud].mean()) if loud.any() else 0.0, cutoff=cutoff(oo, dd), lag=lag, tp_orig=float(to.max()))
                print(f'{kind:7s} {codec:9s} {k:4d}k  lag {lag:5d}  TPorig {to.max():6.2f}  overshoot max {over.max():5.2f} avg {over[loud].mean():5.2f}  cutoff {cutoff(oo, dd):7.0f} Hz', flush=True)
    json.dump(res, open(f'{d}/real.json', 'w'), indent=1)
