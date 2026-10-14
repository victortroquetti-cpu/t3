# Os sons padrao do Trok Hitmarker (a pasta "Trok Hitmarker\sons" que vai junto do .asi), sintetizados aqui: nada
# copiado de outro jogo. Quem quiser outro som poe um .mp3 ou .wav com o mesmo nome na pasta.
# Uso: python3 -I make_sounds.py <pasta de saida>
import math
import os
import struct
import sys

import numpy as np

RATE = 44100


def env(n, attack, decay):
    t = np.arange(n) / RATE
    e = np.minimum(1.0, t / max(attack, 1e-4)) * np.exp(-t / decay)
    return e


def tone(freq, dur, decay, attack=0.002, sweep=0.0):
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    f = freq * np.exp(sweep * t)  # sweep < 0: o tom cai
    phase = 2 * np.pi * np.cumsum(f) / RATE
    return np.sin(phase) * env(n, attack, decay)


def noise(dur, decay, color=0.6, seed=1):
    n = int(RATE * dur)
    rng = np.random.default_rng(seed)
    x = rng.standard_normal(n)
    y = np.zeros(n)
    for i in range(1, n):  # passa-baixa simples: ruido mais grave
        y[i] = color * y[i - 1] + (1 - color) * x[i]
    return y * env(n, 0.001, decay)


def mix(*parts):
    n = max(len(p) for p in parts)
    out = np.zeros(n)
    for p in parts:
        out[:len(p)] += p
    return out


def save(name, x, out, gain=0.85):
    x = x / max(1e-9, np.max(np.abs(x))) * gain
    fade = min(len(x), int(RATE * 0.004))
    x[-fade:] *= np.linspace(1, 0, fade)
    data = (np.clip(x, -1, 1) * 32767).astype('<i2').tobytes()
    with open(os.path.join(out, name + '.wav'), 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(b'data' + struct.pack('<I', len(data)) + data)


def main(out):
    os.makedirs(out, exist_ok=True)
    # acerto: um "tic" curto e seco
    save('acerto', mix(tone(1900, 0.06, 0.012), 0.5 * tone(3800, 0.04, 0.006), 0.25 * noise(0.03, 0.004, 0.2)), out, 0.7)
    # cabeca: um "ding" metalico, mais agudo e mais longo
    save('cabeca', mix(tone(2350, 0.32, 0.09), 0.6 * tone(3520, 0.28, 0.06), 0.35 * tone(4710, 0.2, 0.04),
                       0.3 * noise(0.02, 0.003, 0.1, 2)), out, 0.8)
    # dano levado: um baque grave
    save('dano_levado', mix(tone(150, 0.18, 0.05, sweep=-3.0), 0.6 * noise(0.12, 0.03, 0.85, 3)), out, 0.9)
    # abate: baque + acorde que sobe
    save('abate', mix(tone(110, 0.25, 0.07, sweep=-2.0), 0.7 * tone(880, 0.45, 0.14), 0.6 * tone(1320, 0.45, 0.12),
                      0.4 * noise(0.05, 0.01, 0.5, 4)), out, 0.85)
    # abate na cabeca: o ding da cabeca dobrado, mais brilhante
    a = mix(tone(2350, 0.3, 0.08), 0.6 * tone(3520, 0.25, 0.05))
    b = np.concatenate([np.zeros(int(RATE * 0.09)), mix(tone(3140, 0.4, 0.12), 0.6 * tone(4700, 0.3, 0.07))])
    save('abate_cabeca', mix(a, b, 0.5 * tone(110, 0.2, 0.06, sweep=-2.0)), out, 0.85)
    # morte: um tom que cai
    save('morte', mix(tone(420, 0.75, 0.3, attack=0.01, sweep=-1.6), 0.5 * tone(210, 0.75, 0.3, attack=0.01, sweep=-1.6)),
         out, 0.8)


if __name__ == '__main__':
    main(sys.argv[1])
