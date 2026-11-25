#!/usr/bin/env python3
# BELArcade - genera splash.wav, el jingle de arranque (48 kHz, estereo, 16 bits)
import sys, wave
import numpy as np

SR = 48000
rng = np.random.default_rng(7)


def hz(n):                                   # nota MIDI -> frecuencia
    return 440.0 * 2 ** ((n - 69) / 12)


def t_(dur):
    return np.arange(int(dur * SR)) / SR


def env(dur, a=0.005, d=0.3, s=0.6, r=0.2):  # envolvente ADSR simple
    t = t_(dur); e = np.ones_like(t) * s
    e[t < a] = t[t < a] / a
    m = (t >= a) & (t < a + d)
    e[m] = 1 - (1 - s) * (t[m] - a) / d
    rel = t > dur - r
    e[rel] *= (dur - t[rel]) / r
    return e


def saw(f, dur, detune=0.0):
    t = t_(dur)
    return 2 * ((f * (1 + detune) * t) % 1.0) - 1


def supersaw(f, dur):                        # 3 sierras desafinadas = sinte "gordo" de los 90
    return (saw(f, dur) + saw(f, dur, 0.006) + saw(f, dur, -0.006)) / 3


def lowpass(x, cut):                         # filtro paso bajas de un polo (cut puede variar en el tiempo)
    cut = np.broadcast_to(cut, x.shape)
    a = 1 - np.exp(-2 * np.pi * cut / SR)
    y = np.zeros_like(x); acc = 0.0
    for i in range(len(x)):
        acc += a[i] * (x[i] - acc); y[i] = acc
    return y


def fm_bell(f, dur, idx=3.0):                # campana FM tipo teclado Yamaha
    t = t_(dur)
    mod = idx * np.exp(-t * 4) * np.sin(2 * np.pi * f * 3.5 * t)
    return np.sin(2 * np.pi * f * t + mod) * np.exp(-t * 3)


def kick(dur=0.35):
    t = t_(dur)
    f = 50 + 120 * np.exp(-t * 30)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 9)


def snare(dur=0.4):                          # tarola con "gated reverb" ochentera/noventera
    t = t_(dur)
    ruido = rng.uniform(-1, 1, len(t))
    cuerpo = np.sin(2 * np.pi * 190 * t) * np.exp(-t * 25)
    e = np.where(t < 0.22, np.exp(-t * 3), np.exp(-0.66) * np.exp(-(t - 0.22) * 60))
    return (ruido * 0.7 + cuerpo) * e


def hat(dur=0.06):
    return rng.uniform(-1, 1, int(dur * SR)) * np.exp(-t_(dur) * 60)


def reverb(x, mezcla=0.25):                  # reverb sencilla con ecos multiples
    y = x.copy()
    for ms, g in ((37, .5), (53, .4), (71, .33), (97, .27), (131, .2), (173, .14)):
        k = int(SR * ms / 1000)
        y[k:] += x[:-k] * g * mezcla * 2
    return y


class Pista:
    def __init__(self, dur):
        self.L = np.zeros(int(dur * SR)); self.R = np.zeros(int(dur * SR))

    def add(self, x, t0, vol=1.0, pan=0.0):
        i = int(t0 * SR); n = min(len(x), len(self.L) - i)
        self.L[i:i + n] += x[:n] * vol * (1 - max(0, pan))
        self.R[i:i + n] += x[:n] * vol * (1 + min(0, pan))

    def guardar(self, ruta, rev=0.25):
        L, R = reverb(self.L, rev), reverb(self.R, rev)
        pico = max(np.abs(L).max(), np.abs(R).max())
        L, R = np.tanh(L / pico * 2.2), np.tanh(R / pico * 2.2)   # compresion suave para que suene con mas cuerpo
        pico = max(np.abs(L).max(), np.abs(R).max())
        L, R = L / pico * 0.89, R / pico * 0.89
        fade = np.minimum(1, (len(L) - np.arange(len(L))) / (SR * 0.25))
        data = np.stack([L * fade, R * fade], 1)
        with wave.open(ruta, "wb") as w:
            w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
            w.writeframes((data * 32767).astype("<i2").tobytes())


def syndrum(f0=900, dur=0.3):                 # tom electronico "piuu" tipo disco de fin de los 70
    t = t_(dur)
    f = f0 * np.exp(-t * 9) + 80
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 7)


def laser(dur=0.25, f0=2400, f1=300):         # efecto "laser" espacial
    t = t_(dur)
    f = f1 + (f0 - f1) * np.exp(-t * 14)
    fase = np.cumsum(f) / SR
    return (2 * (fase % 1) - 1) * np.exp(-t * 8)


def phaser(x, vel=0.6):                         # barrido tipo phaser para las cuerdas
    t = np.arange(len(x)) / SR
    d = (0.0015 + 0.0012 * np.sin(2 * np.pi * vel * t)) * SR
    idx = np.clip(np.arange(len(x)) - d, 0, len(x) - 1)
    return 0.5 * (x + np.interp(idx, np.arange(len(x)), x))


def pulso(f, dur, ancho=0.25):                 # onda de pulso tipo chip de consola (NES/arcade)
    t = t_(dur)
    return np.where((f * t) % 1.0 < ancho, 1.0, -1.0)


def moneda(p, t0, vol=0.3):                    # "cling" de ficha: dos notas rapidas
    p.add(pulso(hz(83), 0.07, .5) * env(0.07, .001, .03, .7, .01), t0, vol)
    p.add(pulso(hz(88), 0.35, .5) * np.exp(-t_(0.35) * 9), t0 + 0.07, vol)


def arcadedisco(ruta):
    """Ficha de arcade, ritmo disco con secuenciador de chip y power-up final."""
    p = Pista(4.2)
    moneda(p, 0.0)
    beat = 60 / 128
    sem = beat / 4
    inicio = 0.45
    prog = [(57, (69, 72, 76)), (53, (65, 69, 72)), (55, (67, 71, 74))]      # Am F G (propia)
    melodia = [76, None, 79, 76, 81, None, 79, None]                          # linea arcade propia
    t0 = inicio
    for j, (raiz, ac) in enumerate(prog):
        dur_ac = beat * 2
        for k in range(8):                                     # secuenciador de chip (pulso 12.5%)
            n = (ac + tuple(x + 12 for x in ac))[[0, 2, 1, 3, 2, 4, 3, 5][k]]
            p.add(pulso(hz(n), sem * .7, .125) * env(sem * .7, .001, .04, .5, .02), t0 + k * sem, 0.13, (-.4, .4)[k % 2])
        for k in range(4):                                     # bajo disco en octavas
            n = raiz - 24 if k % 2 == 0 else raiz - 12
            p.add(lowpass(saw(hz(n), beat / 2 * .9), 800) * env(beat / 2 * .9, .003, .1, .6, .05), t0 + k * beat / 2, 0.5)
            p.add(hat(0.14), t0 + k * beat / 2 + beat / 4, 0.17, 0.3)
        p.add(kick(), t0, 0.8); p.add(kick(), t0 + beat, 0.8)
        cuerdas = sum(supersaw(hz(n - 12), dur_ac) for n in ac) / 3
        p.add(phaser(lowpass(cuerdas, 1800)) * env(dur_ac, .15, .3, .7, .2), t0, 0.22)
        if j == 1:                                             # melodia de chip en el segundo acorde
            for k, n in enumerate(melodia):
                if n: p.add(pulso(hz(n), sem * 1.8, .25) * env(sem * 1.8, .002, .1, .6, .05), t0 + k * sem * 2 - beat, 0.2)
        t0 += dur_ac
    p.add(laser(), inicio, 0.22, -0.6)
    for k, n in enumerate((69, 72, 76, 81, 84, 88)):           # power-up: arpegio rapido hacia arriba
        p.add(pulso(hz(n), 0.06, .25) * env(0.06, .001, .03, .7, .01), t0 - 0.36 + k * 0.06, 0.22)
    fin = (69, 72, 76, 81)
    p.add(phaser(lowpass(sum(supersaw(hz(n), 1.3) for n in fin) / 4, 2400)) * env(1.3, .01, .4, .6, .6), t0, 0.42)
    p.add(pulso(hz(93), 1.0, .5) * np.exp(-t_(1.0) * 4), t0, 0.12)
    p.add(lowpass(saw(hz(33), 1.3), 300) * env(1.3, .005, .3, .7, .5), t0, 0.55)
    p.add(kick(0.6), t0, 1.0); p.add(snare(), t0, 0.45); p.add(laser(0.5, 3500, 150), t0 + 0.05, 0.25, 0.5)
    p.guardar(ruta, 0.28)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    arcadedisco(f"{out}/splash.wav")
    print("listo")
