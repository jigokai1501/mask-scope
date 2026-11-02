"""Génère des signaux « master fort » synthétiques (48 kHz stéréo f32) pour calibrer la simulation de codec."""
import numpy as np, sys, os
rng = np.random.default_rng(7)
fs = 48000
def saw(f, n, detune=0.0):
    t = np.arange(n) / fs
    ph = (f * (1 + detune) * t) % 1.0
    return 2 * ph - 1
def kick_train(n, bpm=170):
    out = np.zeros(n); step = int(fs * 60 / bpm)
    t = np.arange(int(fs * 0.4)) / fs
    k = np.sin(2 * np.pi * np.cumsum(50 + 110 * np.exp(-t / 0.03)) / fs) * np.exp(-t / 0.12)
    for s in range(0, n - len(k), step): out[s:s + len(k)] += k
    return out
def hats(n, bpm=170):
    out = np.zeros(n); step = int(fs * 60 / bpm / 2)
    t = np.arange(int(fs * 0.05)) / fs
    h = rng.standard_normal(len(t)) * np.exp(-t / 0.01)
    h = np.diff(h, prepend=0)
    for s in range(step // 2, n - len(h), step): out[s:s + len(h)] += h
    return out
def pinkish(n):
    w = rng.standard_normal(n); f = np.fft.rfft(w); k = np.arange(len(f)); k[0] = 1
    return np.fft.irfft(f / np.sqrt(k), n)
def limit(x, ceil=0.89, drive=3.0, hard=0.0):
    y = np.tanh(drive * x) / np.tanh(drive)
    if hard > 0: y = np.clip(y * (1 + hard), -1, 1)
    pk = np.max(np.abs(y)); return y * (ceil / pk)
def render(kind, dur=14.0):
    n = int(fs * dur); t = np.arange(n) / fs
    if kind == 'punk':       # guitares saturées + batterie
        g = sum(saw(f, n, d) for f in (110, 164.8, 220, 329.6) for d in (-0.003, 0.004))
        g = np.tanh(3 * g / 4)
        L = 0.9 * kick_train(n) + 0.45 * g + 0.25 * hats(n)
        R = 0.9 * kick_train(n) + 0.45 * np.roll(g, 37) + 0.25 * np.roll(hats(n), 11)
        return limit(L, 0.89, 2.5, 0.15), limit(R, 0.89, 2.5, 0.15)
    if kind == 'hyper':      # supersaw + 808 + clippeur dur
        lead = sum(saw(440 * 2 ** (k / 12), n, d) for k in (0, 4, 7) for d in np.linspace(-0.01, 0.01, 5))
        sub = np.sin(2 * np.pi * 45 * t) * (0.5 + 0.5 * np.sin(2 * np.pi * 0.5 * t))
        L = 0.5 * lead / 15 + 0.8 * sub + 0.6 * kick_train(n, 150) + 0.3 * hats(n, 150)
        R = 0.5 * np.roll(lead, 29) / 15 + 0.8 * sub + 0.6 * kick_train(n, 150) + 0.3 * np.roll(hats(n, 150), 7)
        return limit(L, 0.89, 4.0, 0.5), limit(R, 0.89, 4.0, 0.5)
    if kind == 'noise':      # bruit rose très écrasé
        p = pinkish(n); q = pinkish(n)
        return limit(p / np.std(p), 0.89, 3.0, 0.3), limit(q / np.std(q), 0.89, 3.0, 0.3)
    if kind == 'techno':
        L = 0.9 * kick_train(n, 138) + 0.4 * saw(55, n) * np.exp(-((t * 138 / 60) % 1) * 3) + 0.25 * hats(n, 138)
        return limit(L, 0.89, 2.0, 0.1), limit(np.roll(L, 5), 0.89, 2.0, 0.1)
from scipy.signal import resample_poly
def normalize_tp(L, R, tp_db=-1.0):
    up = np.stack([resample_poly(L, 4, 1), resample_poly(R, 4, 1)])
    g = 10 ** (tp_db / 20) / np.max(np.abs(up))
    return L * g, R * g
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
for kind in ('punk', 'hyper', 'noise', 'techno'):
    L, R = render(kind)
    L, R = normalize_tp(L, R)
    np.stack([L, R], 1).astype('<f4').tofile(f'{out}/{kind}.f32')
print('ok')
