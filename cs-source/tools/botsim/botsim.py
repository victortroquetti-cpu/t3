#!/usr/bin/env python3
"""Simulador de comparação: bot padrão do HL2MP x bot do Trok.

NÃO é o jogo. Reimplementa, com as mesmas fórmulas e constantes, as partes
de decisão dos dois bots e roda os dois nos mesmos cenários sintéticos,
milhares de vezes, para comparar números:

  1. percepção  - quanto tempo até perceber um inimigo em 4 situações
  2. duelo      - mira contra alvo fazendo ADAD (acerto, tempo até acertar, cabeça)
  3. strafe     - padrão de movimento lateral em combate
  4. rotas      - 8 bots do mesmo time indo do spawn ao mesmo lugar
  5. tático     - nota de "lugar bom para segurar" num mapa com laboratório
                  que só se alcança pulando 3 pilastras

Fontes das constantes:
  padrão: sdk/src/game/server/hl2mp/bot/* e NextBot/* (Valve, SDK 2013)
  Trok:   cs-source/src/game/server/trok/bot/* e game/mod_trok/scripts/trok_bot_perfis.txt

O que fica de fora (só o jogo prova): física real, colisão, nav mesh de
mapas reais, animação, rede, armas do HL2MP com recuo, comportamento
emergente de partidas inteiras.

Uso: python3 -I botsim.py [saida.json]   (sem dependências)
"""

import heapq
import json
import math
import random
import sys

SEED = 20261109
TICK = 1.0 / 66.0          # tick do servidor
UPDATE = 0.1               # nb_update_frequency (NextBotManager.cpp)

LEVELS = ["facil", "normal", "dificil", "expert"]

# ---------------------------------------------------------------- padrão (Valve)
DEF_RECOGNIZE = {"facil": 1.0, "normal": 0.5, "dificil": 0.3, "expert": 0.2}   # CHL2MPBotVision::GetMinRecognizeTime
DEF_TRACK_INTERVAL = {"facil": 1.0, "normal": 0.25, "dificil": 0.1, "expert": 0.05}  # CHL2MPBotBody::GetHeadAimTrackingInterval
DEF_HEAD_SPEED = 1000.0    # nb_saccade_speed
DEF_FOV = 90.0             # FOV do jogador, copiado para a visão a cada update

# ---------------------------------------------------------------- Trok (perfis)
TROK = {
    "facil":   dict(rmin=0.45, rmax=0.70, gain=0.70, fov=100, e0=7.0, emin=2.0, prec=1.00, hl=0.50, track=4.0, head=0.05, turn=400, interval=0.40, smin=0.0, smax=0.0, crouch=0.0, disc=0.0),
    "normal":  dict(rmin=0.30, rmax=0.45, gain=0.85, fov=110, e0=6.0, emin=1.5, prec=0.75, hl=0.40, track=3.0, head=0.15, turn=600, interval=0.20, smin=0.5, smax=1.1, crouch=0.05, disc=0.50),
    "dificil": dict(rmin=0.22, rmax=0.32, gain=1.00, fov=120, e0=4.0, emin=0.9, prec=0.60, hl=0.28, track=2.0, head=0.30, turn=800, interval=0.12, smin=0.35, smax=0.8, crouch=0.12, disc=0.80),
    "expert":  dict(rmin=0.17, rmax=0.25, gain=1.15, fov=130, e0=3.0, emin=0.6, prec=0.50, hl=0.20, track=1.5, head=0.45, turn=950, interval=0.08, smin=0.25, smax=0.6, crouch=0.18, disc=0.95),
}
PERSONALITY = 0.12         # trok_bot_personalidade
PERCEP_BASE = 6.0          # trok_bot_percepcao_base


def jitter(rng, v, a=PERSONALITY):
    return v * rng.uniform(1.0 - a, 1.0 + a)


def trok_profile(rng, level):
    p = dict(TROK[level])
    for k in ("rmin", "gain", "e0", "emin", "prec", "hl", "track", "turn"):
        p[k] = jitter(rng, p[k])
    p["rmax"] = max(p["rmin"], jitter(rng, p["rmax"]))
    p["head"] = min(1.0, max(0.0, jitter(rng, p["head"], PERSONALITY * 2)))
    return p


def trok_reaction(rng, p):
    """Ex-gaussiana: CTrokBotVision::GetMinRecognizeTime."""
    mu = p["rmin"]
    tau = max(0.01, p["rmax"] - p["rmin"])
    return max(0.15, rng.gauss(mu, 0.1 * mu) - tau * math.log(max(1e-4, rng.random())))


# ============================================================== 1. percepção

SCENARIOS = [
    dict(id="A", nome="Correndo de frente, perto", desc="400u, 5° do centro, corpo todo à vista, correndo",
         dist=400, angle=5, exposure=1.0, speed=260, crouch=False, light=1.0),
    dict(id="B", nome="Agachado atrás de caixa", desc="1500u, 25° do centro, só a cabeça para fora, parado",
         dist=1500, angle=25, exposure=0.2, speed=0, crouch=True, light=1.0),
    dict(id="C", nome="Andando no escuro", desc="1000u, 40° do centro, meio corpo à vista, luz 30%",
         dist=1000, angle=40, exposure=0.6, speed=150, crouch=False, light=0.3),
    dict(id="D", nome="Correndo na periferia", desc="600u, 52° do centro (fora do FOV de 90°), correndo",
         dist=600, angle=52, exposure=1.0, speed=260, crouch=False, light=1.0),
]
HORIZON = 6.0


def default_detect(rng, level, sc):
    """Padrão: no FOV + linha de visão (qualquer ponto) => reconhece após o tempo fixo."""
    if sc["angle"] > DEF_FOV / 2 or sc["exposure"] <= 0:
        return None
    first_update = rng.uniform(0, UPDATE)
    return first_update + DEF_RECOGNIZE[level]


def trok_gain(p, sc, rng_noise):
    half = 0.5 * p["fov"]
    a = sc["angle"]
    if a > half:
        return 0.0
    af = 1.0
    if a > 10.0:
        t = (a - 10.0) / max(1.0, half - 10.0)
        af = 1.0 - 0.85 * t * (2.0 - t)
    d = sc["dist"]
    rf = 1.0
    if d > 256.0:
        x = (d - 256.0) / 900.0
        rf = 1.0 / (1.0 + x * math.sqrt(x))
    ef = 0.2 + 0.8 * sc["exposure"]
    s = sc["speed"]
    mf = 1.35 if s > 200 else (1.0 if s > 80 else 0.6)
    if sc["crouch"]:
        mf *= 0.75
    lf = 0.45 + 0.55 * sc["light"]
    return PERCEP_BASE * p["gain"] * af * rf * ef * mf * lf * rng_noise


def trok_detect(rng, level, sc):
    p = trok_profile(rng, level)
    t = rng.uniform(0, UPDATE)
    value = 0.0
    noise, noise_until = 1.0, -1.0
    while t < HORIZON:
        if t >= noise_until:
            noise = rng.uniform(0.7, 1.3)
            noise_until = t + rng.uniform(0.3, 0.7)
        value += trok_gain(p, sc, noise) * UPDATE
        if value >= 1.0:
            return t + trok_reaction(rng, p)
        t += UPDATE
    return None


def run_perception(rng, n=3000):
    out = []
    for sc in SCENARIOS:
        entry = dict(sc)
        entry["curvas"] = {}
        for level in LEVELS:
            for bot, fn in (("padrao", default_detect), ("trok", trok_detect)):
                times = [fn(rng, level, sc) for _ in range(n)]
                seen = sorted(x for x in times if x is not None)
                grid = [round(0.1 * i, 2) for i in range(int(HORIZON * 10) + 1)]
                curve, j = [], 0
                for g in grid:
                    while j < len(seen) and seen[j] <= g:
                        j += 1
                    curve.append(round(j / n, 4))
                med = seen[len(seen) // 2] if len(seen) >= n / 2 else None
                entry["curvas"][f"{bot}_{level}"] = dict(
                    t=grid, p=curve,
                    p1s=round(sum(1 for x in seen if x <= 1.0) / n, 3),
                    p3s=round(sum(1 for x in seen if x <= 3.0) / n, 3),
                    p6s=round(len(seen) / n, 3),
                    mediana=round(med, 3) if med is not None else None,
                )
        out.append(entry)
    return out


# ============================================================== 2. duelo de mira

TARGET_SPEED = 190.0     # hl2_normspeed
BODY_HALF_W = 13.0       # meia largura do tronco (u)
BODY_HALF_H = 30.0       # tronco + quadril, em volta do centro
HEAD_R = 5.0             # raio da cabeça
HEAD_ABOVE = 28.0        # cabeça acima do WorldSpaceCenter (EyePosition - 2 ~ centro + 28)
FIRE_INTERVAL = 0.1      # 10 tiros/s
SPREAD_HALF = 1.0        # graus (cone de 2°, fuzil parado)
FLICK = 20.0             # alvo aparece 20° fora da mira
DUEL_TIME = 3.0


class Target:
    def __init__(self, rng, dist, strafe):
        self.rng = rng
        self.dist = dist
        self.y = 0.0
        self.v = TARGET_SPEED if strafe else 0.0
        self.strafe = strafe
        self.dir = rng.choice((-1, 1))
        self.next_flip = rng.uniform(0.3, 0.9)
        self.vel = self.dir * self.v

    def step(self, t, dt):
        if self.strafe and t >= self.next_flip:
            self.dir = -self.dir
            self.next_flip = t + self.rng.uniform(0.3, 0.9)
        target_v = self.dir * self.v
        # aceleração do jogador (~sv_accelerate 10 x 190)
        acc = 1900.0 * dt
        self.vel += max(-acc, min(acc, target_v - self.vel))
        self.y += self.vel * dt


def yaw_of(y, dist):
    return math.degrees(math.atan2(y, dist))


def ang_diff(a, b):
    return (a - b + 180.0) % 360.0 - 180.0


def simulate_duel(rng, bot, level, dist, strafe):
    tgt = Target(rng, dist, strafe)
    if bot == "trok":
        p = trok_profile(rng, level)
        head_speed, track_iv = p["turn"], p["interval"]
        aim_head = rng.random() < p["head"] * max(0.3, min(1.0, 1.0 - (dist - 600.0) / 1400.0 * 0.7))
        e0 = p["e0"] * rng.uniform(0.7, 1.3) * (1.0 + FLICK / 90.0)
        phase, wobble = rng.uniform(0, 2 * math.pi), rng.uniform(0, 2 * math.pi)
    else:
        head_speed, track_iv = DEF_HEAD_SPEED, DEF_TRACK_INTERVAL[level]
        aim_head = False

    head_yaw = -FLICK                       # cabeça começa 20° fora
    look_y, look_v = tgt.y, 0.0             # ponto de mira rastreado (lateral, u)
    look_z = 0.0                            # vertical do ponto (relativo ao centro)
    next_track = 0.0
    t = 0.0
    next_fire = 0.0
    shots = hits = head_hits = 0
    first_hit = None
    timeline = []

    while t < DUEL_TIME:
        tgt.step(t, TICK)

        # ponto que o bot quer (SelectTargetPoint), reamostrado no intervalo de rastreio
        if t >= next_track:
            if bot == "trok":
                half_w = math.degrees(math.atan(BODY_HALF_W / dist))
                floor = min(p["emin"], p["prec"] * half_w)
                hl = max(0.05, p["hl"]) * (0.6 + 0.2 * math.log2(1.0 + e0 / max(0.05, half_w)))
                err = floor + (e0 - floor) * 0.5 ** (t / hl)
                ang_speed = math.degrees(abs(tgt.vel) / dist)
                err += p["track"] * ang_speed / 100.0
                phi = phase + 0.9 * math.sin(1.3 * t + wobble)
                off = math.tan(math.radians(err)) * dist
                want_y = tgt.y + off * math.cos(phi)
                look_z = (HEAD_ABOVE if aim_head else 6.0) + 0.6 * off * math.sin(phi)
            else:
                want_y = tgt.y
                look_z = 0.0                # WorldSpaceCenter
            iv = max(track_iv, TICK)
            look_v = (want_y - look_y) / iv + tgt.vel
            next_track = t + rng.uniform(0.8, 1.2) * iv
        look_y += look_v * TICK

        # cabeça gira para o ponto (PlayerBody::Upkeep): taxa máx com ease-out e ease-in
        desired = yaw_of(look_y, dist)
        diff = ang_diff(desired, head_yaw)
        dot = math.cos(math.radians(diff))
        rate = head_speed
        if dot > 0.7:
            k = 1.0 + (dot - 0.7) / 0.3 * (0.02 - 1.0)
            rate *= math.sin(1.57 * k)
        if t < 0.25:
            rate *= t / 0.25
        step = rate * TICK
        head_yaw += max(-step, min(step, diff))

        # atira quando a cabeça "chegou" no ponto (dot > 0.98)
        on_target = math.cos(math.radians(ang_diff(desired, head_yaw))) > 0.98
        if on_target and t >= next_fire:
            next_fire = t + FIRE_INTERVAL
            shots += 1
            r = SPREAD_HALF * math.sqrt(rng.random())
            th = rng.uniform(0, 2 * math.pi)
            shot_yaw = head_yaw + r * math.cos(th)
            shot_pitch_u = dist * math.tan(math.radians(r * math.sin(th)))
            lateral = dist * math.tan(math.radians(shot_yaw - yaw_of(tgt.y, dist)))
            vertical = look_z + shot_pitch_u            # relativo ao centro do alvo
            hit_head = (lateral ** 2 + (vertical - HEAD_ABOVE) ** 2) <= HEAD_R ** 2
            hit_body = abs(lateral) <= BODY_HALF_W and abs(vertical) <= BODY_HALF_H
            if hit_head or hit_body:
                hits += 1
                head_hits += 1 if hit_head else 0
                if first_hit is None:
                    first_hit = t
            timeline.append((round(t, 2), 1 if (hit_head or hit_body) else 0))
        t += TICK

    return shots, hits, head_hits, first_hit, timeline


def run_duel(rng, n=600):
    out = []
    for strafe in (True, False):
        for dist in (300, 800, 1500):
            row = dict(dist=dist, strafe=strafe, res={})
            for level in LEVELS:
                for bot in ("padrao", "trok"):
                    S = H = HH = 0
                    firsts = []
                    per_bin = [0] * 30
                    per_bin_n = 0
                    for _ in range(n):
                        s, h, hh, fh, tl = simulate_duel(rng, bot, level, dist, strafe)
                        S += s; H += h; HH += hh
                        if fh is not None:
                            firsts.append(fh)
                        for (tt, hit) in tl:
                            b = min(29, int(tt * 10))
                            per_bin[b] += hit
                        per_bin_n += 1
                    firsts.sort()
                    row["res"][f"{bot}_{level}"] = dict(
                        acerto=round(H / S, 3) if S else 0.0,
                        cabeca=round(HH / H, 3) if H else 0.0,
                        primeiro=round(firsts[len(firsts) // 2], 3) if firsts else None,
                        sem_acerto=round(1 - len(firsts) / n, 3),
                        acertos_por_s=[round(10 * x / per_bin_n, 3) for x in per_bin],
                    )
            out.append(row)
    return out


# ============================================================== 2b. disciplina de tiro

# Armas no estilo do CS (como as do mod vão ser): andando, a precisão cai muito,
# e tiros seguidos abrem a mira (recuo). As do HL2MP não têm isso; por isso este
# teste é separado do duelo acima.
CS_SPREAD_STILL = 0.4      # graus (meio cone) parado
CS_SPREAD_MOVING = 3.0     # graus extras correndo (a partir de 150u/s)
CS_RECOIL_STEP = 0.35      # graus por tiro seguido
CS_RECOIL_MAX = 3.0
CS_RECOIL_TAU = 0.25       # s para o recuo assentar


BODY_DAMAGE = 34.0         # 3 tiros no corpo matam
HEAD_DAMAGE = 100.0        # cabeça mata
MAX_FIGHT = 6.0


class Shooter:
    """Um lado do duelo. mode: padrao | trok_antigo (spray com strafe) | trok_novo (contra-strafe + rajada)."""

    def __init__(self, rng, mode, level, dist):
        self.rng, self.mode, self.dist = rng, mode, dist
        self.trok = mode != "padrao"
        if self.trok:
            self.p = trok_profile(rng, level)
            self.p["disc"] = TROK[level]["disc"] if mode == "trok_novo" else 0.0
            self.head_speed, self.track_iv = self.p["turn"], self.p["interval"]
            self.aim_head = rng.random() < self.p["head"] * max(0.3, min(1.0, 1.0 - (dist - 600.0) / 1400.0 * 0.7))
            self.e0 = self.p["e0"] * rng.uniform(0.7, 1.3) * (1.0 + FLICK / 90.0)
            self.phase, self.wobble = rng.uniform(0, 2 * math.pi), rng.uniform(0, 2 * math.pi)
        else:
            self.head_speed, self.track_iv = DEF_HEAD_SPEED, DEF_TRACK_INTERVAL[level]
            self.aim_head = False
        self.react = (trok_reaction(rng, self.p) if self.trok else DEF_RECOGNIZE[level])
        self.head_yaw = -FLICK
        self.look_y = self.look_v = self.look_z = 0.0
        self.next_track = self.next_fire = self.next_update = 0.0
        self.y = rng.uniform(-60, 60)
        self.v = 0.0
        self.desire = rng.choice((-1, 1))
        self.last_dir = self.desire
        self.combat = "move"
        self.phase_until = 0.0
        self.disciplined = True
        self.fire_open = True
        self.recoil = 0.0
        self.hp = 100.0
        self.shots = self.hits = self.moving_shots = 0

    def think(self, t):
        if t < self.next_update:
            return
        self.next_update = t + UPDATE
        rng, p = self.rng, getattr(self, "p", None)
        if self.mode == "padrao":
            r = rng.randint(0, 100)
            self.desire = -1 if r < 33 else (1 if r > 66 else 0)
            self.fire_open = True
            return
        rhythm = self.mode == "trok_novo" and self.dist >= 450 and p["disc"] > 0
        if t >= self.phase_until:
            if self.combat == "move":
                if rhythm and self.disciplined:
                    self.combat, self.phase_until, self.desire = "stop", t + 0.08, 0
                else:
                    self.desire = -1 if self.desire == 1 else 1
                    if rng.random() < 0.2 or p["smax"] <= 0:
                        self.desire = 0
                    self.phase_until = t + (rng.uniform(p["smin"], max(p["smin"], p["smax"])) if p["smax"] > 0 else 1.0)
                    self.disciplined = rng.random() < p["disc"]
            elif self.combat == "stop":
                self.combat = "burst"
                self.phase_until = t + (rng.uniform(BURST_MID[0], BURST_MID[1]) if self.dist < 1200 else rng.uniform(BURST_LONG[0], BURST_LONG[1]))
            elif self.combat == "burst":
                self.combat = "recover"
                self.phase_until = t + (rng.uniform(RECOVER_MID[0], RECOVER_MID[1]) if self.dist < 1200 else rng.uniform(RECOVER_LONG[0], RECOVER_LONG[1]))
            else:
                self.combat = "move"
                self.disciplined = rng.random() < p["disc"]
                self.desire = -1 if self.last_dir == 1 else 1
                self.phase_until = t + rng.uniform(p["smin"], max(p["smin"], p["smax"])) * MOVE_SCALE
        if self.desire != 0:
            self.last_dir = self.desire
        self.fire_open = (not rhythm) or (not self.disciplined) or self.combat == "burst"

    def move(self):
        if self.mode != "padrao" and self.combat == "stop":
            target_v = -self.last_dir * 190.0
        elif self.mode == "padrao" or self.combat == "move":
            target_v = self.desire * 190.0
        else:
            target_v = 0.0
        acc = 1900.0 * TICK
        if target_v == 0.0:
            self.v *= max(0.0, 1.0 - 4.0 * TICK)
            self.v -= math.copysign(min(abs(self.v), 400.0 * TICK), self.v)
        else:
            self.v += max(-acc, min(acc, target_v - self.v))
        if self.mode != "padrao" and self.combat in ("stop", "burst", "recover") and abs(self.v) < 25.0:
            self.v = 0.0
        self.y += self.v * TICK

    def aim_and_fire(self, t, other):
        dist, rng = self.dist, self.rng
        rel_y, rel_v = other.y - self.y, other.v - self.v
        if t >= self.next_track:
            if self.trok:
                p = self.p
                half_w = math.degrees(math.atan(BODY_HALF_W / dist))
                floor = min(p["emin"], p["prec"] * half_w)
                hl = max(0.05, p["hl"]) * (0.6 + 0.2 * math.log2(1.0 + self.e0 / max(0.05, half_w)))
                err = floor + (self.e0 - floor) * 0.5 ** (t / hl)
                err += p["track"] * math.degrees(abs(rel_v) / dist) / 100.0
                if abs(self.v) > 120.0:
                    err *= 1.25
                phi = self.phase + 0.9 * math.sin(1.3 * t + self.wobble)
                off = math.tan(math.radians(err)) * dist
                want = rel_y + off * math.cos(phi)
                self.look_z = (HEAD_ABOVE if self.aim_head else 6.0) + 0.6 * off * math.sin(phi)
            else:
                want, self.look_z = rel_y, 0.0
            iv = max(self.track_iv, TICK)
            self.look_v = (want - self.look_y) / iv + rel_v
            self.next_track = t + rng.uniform(0.8, 1.2) * iv
        self.look_y += self.look_v * TICK
        desired = yaw_of(self.look_y, dist)
        diff = ang_diff(desired, self.head_yaw)
        dot = math.cos(math.radians(diff))
        rate = self.head_speed
        if dot > 0.7:
            k = 1.0 + (dot - 0.7) / 0.3 * (0.02 - 1.0)
            rate *= math.sin(1.57 * k)
        if t < 0.25:
            rate *= t / 0.25
        self.head_yaw += max(-rate * TICK, min(rate * TICK, diff))
        self.recoil *= math.exp(-TICK / CS_RECOIL_TAU)

        if t < self.react:
            return
        on_target = math.cos(math.radians(ang_diff(desired, self.head_yaw))) > 0.98
        if not (on_target and self.fire_open and t >= self.next_fire):
            return
        self.next_fire = t + FIRE_INTERVAL
        self.shots += 1
        self.moving_shots += 1 if abs(self.v) > 30.0 else 0
        spread = CS_SPREAD_STILL + CS_SPREAD_MOVING * min(1.0, abs(self.v) / 150.0) + self.recoil
        self.recoil = min(CS_RECOIL_MAX, self.recoil + CS_RECOIL_STEP)
        r = spread * math.sqrt(rng.random())
        th = rng.uniform(0, 2 * math.pi)
        lateral = dist * math.tan(math.radians(self.head_yaw + r * math.cos(th) - yaw_of(rel_y, dist)))
        vertical = self.look_z + dist * math.tan(math.radians(r * math.sin(th)))
        if (lateral ** 2 + (vertical - HEAD_ABOVE) ** 2) <= HEAD_R ** 2:
            other.hp -= HEAD_DAMAGE
            self.hits += 1
        elif abs(lateral) <= BODY_HALF_W and abs(vertical) <= BODY_HALF_H:
            other.hp -= BODY_DAMAGE
            self.hits += 1


# Ritmo do Trok (iguais aos de CTrokBotMainAction::Dodge)
BURST_MID, BURST_LONG = (0.6, 1.0), (0.3, 0.5)
RECOVER_MID, RECOVER_LONG = (0.05, 0.1), (0.1, 0.2)
MOVE_SCALE = 0.3


def fight(rng, mode, level, dist, ref_level="normal"):
    """Bot (mode, level) contra o jogador de referência: spray fazendo ADAD, mira de nível normal."""
    a = Shooter(rng, mode, level, dist)
    b = Shooter(rng, "trok_antigo", ref_level, dist)
    t = 0.0
    while t < MAX_FIGHT:
        for s in (a, b):
            s.think(t)
            s.move()
        a.aim_and_fire(t, b)
        b.aim_and_fire(t, a)
        if a.hp <= 0 or b.hp <= 0:
            break
        t += TICK
    if b.hp <= 0 and a.hp > 0:
        result = "vitoria"
    elif a.hp <= 0 and b.hp > 0:
        result = "derrota"
    else:
        result = "empate"
    return result, t, a


def run_discipline(rng, n=1500):
    out = []
    for dist in (800, 1500):
        for level in ("normal", "expert"):
            row = dict(dist=dist, nivel=level, res={})
            for mode in ("padrao", "trok_antigo", "trok_novo"):
                wins = losses = 0
                ttk = []
                S = H = M = 0
                for _ in range(n):
                    res, t, a = fight(rng, mode, level, dist)
                    wins += res == "vitoria"
                    losses += res == "derrota"
                    if res == "vitoria":
                        ttk.append(t)
                    S += a.shots; H += a.hits; M += a.moving_shots
                ttk.sort()
                row["res"][mode] = dict(
                    vitoria=round(wins / n, 3), derrota=round(losses / n, 3),
                    ttk=round(ttk[len(ttk) // 2], 2) if ttk else None,
                    acerto=round(H / S, 3) if S else 0.0,
                    tiros_andando=round(M / S, 3) if S else 0.0,
                )
            out.append(row)
    return out


# ============================================================== 3. strafe

def strafe_trace(rng, bot, level, seconds=6.0):
    """Posição lateral do bot em combate (u) a cada tick."""
    t, y, v = 0.0, 0.0, 0.0
    desire = 0
    next_update = 0.0
    seg_until = 0.0
    trace, changes = [], 0
    last_desire = 0
    p = trok_profile(rng, level) if bot == "trok" else None
    while t < seconds:
        if t >= next_update:
            next_update = t + UPDATE
            if bot == "padrao":
                # CHL2MPBotMainAction::Dodge: a cada update, 33% esquerda, 33% direita, 33% nada
                if level == "facil":
                    desire = 0
                else:
                    r = rng.randint(0, 100)
                    desire = -1 if r < 33 else (1 if r > 66 else 0)
            else:
                if p["smax"] <= 0:
                    desire = 0
                elif t >= seg_until:
                    roll = rng.random()
                    if roll < 0.2:
                        desire = 0
                    elif desire == 0 or roll < 0.85:
                        desire = -1 if desire == 1 else 1
                    dur = rng.uniform(p["smin"], p["smax"])
                    if desire == 0:
                        dur *= 0.5
                    seg_until = t + dur
        if desire != last_desire and desire != 0:
            changes += 1
        if desire != 0:
            last_desire = desire
        target_v = desire * 190.0
        acc = 1900.0 * TICK
        v += max(-acc, min(acc, target_v - v))
        y += v * TICK
        trace.append(round(y, 1))
        t += TICK
    return trace, changes / seconds


def run_strafe(rng):
    out = {}
    for level in ("normal", "expert"):
        for bot in ("padrao", "trok"):
            tr, _ = strafe_trace(random.Random(SEED + 7), bot, level)
            rates, amps = [], []
            for k in range(300):
                trk, rate = strafe_trace(rng, bot, level)
                rates.append(rate)
                amps.append(max(trk) - min(trk))
            out[f"{bot}_{level}"] = dict(
                trace=tr[::2],
                trocas_por_s=round(sum(rates) / len(rates), 2),
                amplitude_u=round(sum(amps) / len(amps), 1),
            )
    return out


# ============================================================== 4. rotas

ROUTE_MAP = [
    "########################################",
    "#......................................#",
    "#......................................#",
    "#..###############....###############..#",
    "#..#.............#....#.............#..#",
    "#..#.............#....#.............#..#",
    "#..#..########################.....#...#",
    "#..#..#......................#.....#...#",
    "#.SS..#......................#.........#",
    "#.SS................................GG.#",
    "#.SS..#......................#......GG.#",
    "#..#..#......................#.........#",
    "#..#..########################.....#...#",
    "#..#.............#....#.............#..#",
    "#..#.............#....#.............#..#",
    "#..###############....###############..#",
    "#......................................#",
    "#......................................#",
    "########################################",
]
CELL = 128.0
ROUTE_CELL = 512.0


def grid_cells():
    cells = {}
    for y, row in enumerate(ROUTE_MAP):
        for x, ch in enumerate(row):
            if ch != "#":
                cells[(x, y)] = ch
    return cells


def neighbors(cells, c):
    x, y = c
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
        n = (x + dx, y + dy)
        if n not in cells:
            continue
        if dx and dy and ((x + dx, y) not in cells or (x, y + dy) not in cells):
            continue
        yield n, math.hypot(dx, dy) * CELL


def route_pref(seed, c):
    cx = int(math.floor(c[0] * CELL / ROUTE_CELL))
    cy = int(math.floor(c[1] * CELL / ROUTE_CELL))
    h = (seed ^ ((cx * 73856093) & 0xFFFFFFFF) ^ ((cy * 19349663) & 0xFFFFFFFF)) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 0x5BD1E995) & 0xFFFFFFFF
    h ^= h >> 15
    return (h & 0xFFFF) / 65535.0


def dijkstra(cells, start, goal, mult):
    dist = {start: 0.0}
    prev = {}
    pq = [(0.0, start)]
    while pq:
        d, c = heapq.heappop(pq)
        if c == goal:
            break
        if d > dist.get(c, 1e18):
            continue
        for n, w in neighbors(cells, c):
            nd = d + w * mult(n)
            if nd < dist.get(n, 1e18):
                dist[n] = nd
                prev[n] = c
                heapq.heappush(pq, (nd, n))
    path, c = [goal], goal
    while c != start:
        c = prev[c]
        path.append(c)
    return path[::-1]


def lane_of(path):
    ys = [c[1] for c in path if 8 <= c[0] <= 28]
    if not ys:
        return "?"
    m = sum(ys) / len(ys)
    return "cima" if m < 6 else ("baixo" if m > 12 else "meio")


STYLES = [("equilibrado", 30, 1.0, 1.0), ("entry", 20, 0.6, 0.5), ("lurker", 15, 1.6, 3.0),
          ("ancora", 20, 1.0, 1.0), ("suporte", 15, 1.0, 0.2)]


def pick_style(rng):
    total = sum(s[1] for s in STYLES)
    r = rng.uniform(0, total)
    for s in STYLES:
        r -= s[1]
        if r <= 0:
            return s
    return STYLES[0]


def run_routes(rng, rounds=300, bots=8):
    cells = grid_cells()
    spawns = [c for c, ch in cells.items() if ch == "S"]
    goal = sorted(c for c, ch in cells.items() if ch == "G")[0]
    result = {}
    for bot in ("padrao", "trok"):
        lanes = {"cima": 0, "meio": 0, "baixo": 0, "?": 0}
        heat = {}
        overlaps, majority, arrival_spread = [], [], []
        example = None
        for rnd in range(rounds):
            used = set()
            paths, starts = [], []
            order = list(range(bots))
            hes = [0.0] * bots
            if bot == "trok":
                hes = [rng.uniform(0.1, 1.0) * (1.5 - rng.uniform(0.2, 0.9)) for _ in range(bots)]
                order.sort(key=lambda i: hes[i])
            for i in order:
                start = spawns[i % len(spawns)]
                if bot == "padrao":
                    path = dijkstra(cells, start, goal, lambda n: 1.0)   # SAFEST_ROUTE: sem preferência
                else:
                    style = pick_style(rng)
                    seed = rng.randint(1, 0x7FFFFFFF)
                    var, team = 1.0 * style[2], 0.6 * style[3]
                    path = dijkstra(cells, start, goal,
                                    lambda n, s=seed, v=var, tm=team: (1.0 + v * route_pref(s, n)) * (1.0 + (tm if n in used else 0.0)))
                    used.update(path[:40])
                paths.append(path)
                length = sum(math.hypot(a[0] - b[0], a[1] - b[1]) * CELL for a, b in zip(path, path[1:]))
                starts.append(hes[i] + length / 190.0)
            ln = [lane_of(p) for p in paths]
            for l in ln:
                lanes[l] += 1
            majority.append(max(ln.count(x) for x in set(ln)) / bots)
            sets = [set(p) for p in paths]
            ov = []
            for a in range(bots):
                for b in range(a + 1, bots):
                    ov.append(len(sets[a] & sets[b]) / len(sets[a] | sets[b]))
            overlaps.append(sum(ov) / len(ov))
            arrival_spread.append(max(starts) - min(starts))
            for p in paths:
                for c in p:
                    heat[c] = heat.get(c, 0) + 1
            if example is None:
                example = [[list(c) for c in p] for p in paths]
        total = sum(lanes.values())
        result[bot] = dict(
            corredores={k: round(v / total, 3) for k, v in lanes.items() if k != "?"},
            sobreposicao=round(sum(overlaps) / len(overlaps), 3),
            maioria_mesmo_corredor=round(sum(majority) / len(majority), 3),
            espalhamento_chegada_s=round(sum(arrival_spread) / len(arrival_spread), 2),
            calor={f"{c[0]},{c[1]}": round(v / (rounds * bots), 3) for c, v in heat.items()},
            exemplo=example,
        )
    result["mapa"] = ROUTE_MAP
    return result


# ============================================================== 5. mapa tático

# Altura por célula: "." chão 0, "#" parede, "c" caixa 48, "1" "2" "3" pilastras 50/100/150,
# "L" laboratório 200, "p" plataforma de sniper 64 com escada de degraus "s" (16, 32, 48).
TACT_MAP = [
    "##############################",
    "#............................#",
    "#..cc.......####......LLLLL..#",
    "#..cc.......#..#......LLLLL..#",
    "#...........#..#......LLLLL..#",
    "#.....................LLLLL..#",
    "#..........###..........3....#",
    "#..........#.#...............#",
    "#..........#.#........2......#",
    "#............................#",
    "#...cc.............1.........#",
    "#...cc.......................#",
    "#..................sssppp....#",
    "#........######.......ppp....#",
    "#........#....#..............#",
    "#........#....#.....cc.......#",
    "#........##.###.....cc.......#",
    "#............................#",
    "##############################",
]
HEIGHTS = {".": 0, "c": 48, "1": 50, "2": 100, "3": 150, "L": 200, "p": 64, "s": 16}
STEP = 18.0 + 2.0          # TACT_WALK_HEIGHT
JUMP = 56.0                # GetMaxJumpHeight (HL2MP)


def tact_cells():
    cells = {}
    for y, row in enumerate(TACT_MAP):
        for x, ch in enumerate(row):
            if ch == "#":
                continue
            z = HEIGHTS[ch]
            if ch == "s":
                # degraus subindo até a plataforma
                z = 16 * (1 + [i for i, cc in enumerate(row) if cc == "s"].index(x))
            cells[(x, y)] = z
    return cells


def tact_links(cells):
    """Ligações entre vizinhas (como o nav): pulo até 56u para cima; queda qualquer.
    As pilastras ficam separadas por vão (não adjacentes no grid); ligamos por "pulo de vão"
    as pilastras em sequência, como um link de pulo do nav."""
    links = {c: [] for c in cells}
    for (x, y), z in cells.items():
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n not in cells:
                continue
            dz = cells[n] - z
            if dz > JUMP:
                continue
            links[(x, y)].append(n)
    # vãos entre pilastras (ida e volta) e da última pilastra ao laboratório
    def find(ch):
        return next((x, y) for y, row in enumerate(TACT_MAP) for x, cc in enumerate(row) if cc == ch)
    p3 = find("3")
    chain = [find("1"), find("2"), p3, (p3[0], p3[1] - 1)]
    for a, b in zip(chain, chain[1:]):
        links[a].append(b)
        links[b].append(a)
    return links


def link_type(cells, a, b):
    dz = cells[b] - cells[a]
    if dz > STEP:
        return "pulo"
    if dz < -STEP:
        return "queda"
    return "pe"


def los(a, b):
    """Linha de visão 2D (paredes bloqueiam), Bresenham."""
    x0, y0 = a
    x1, y1 = b
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while (x0, y0) != (x1, y1):
        if TACT_MAP[y0][x0] == "#":
            return False
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy
    return True


def run_tactical():
    cells = tact_cells()
    links = tact_links(cells)

    # 1) regiões: flood fill por ligações a pé
    region = {}
    regions = []
    for c in cells:
        if c in region:
            continue
        rid = len(regions)
        regions.append(dict(areas=[], walk=0, jump=0, drop=0, entrances=set(), cost=99))
        stack = [c]
        region[c] = rid
        while stack:
            cur = stack.pop()
            regions[rid]["areas"].append(cur)
            for n in links[cur]:
                if link_type(cells, cur, n) != "pe" or n in region:
                    continue
                if cur in links[n] and link_type(cells, n, cur) != "pe":
                    continue
                region[n] = rid
                stack.append(n)

    # 2) entradas
    for a in cells:
        for b in links[a]:
            ra, rb = region[a], region[b]
            if ra == rb:
                continue
            regions[rb]["entrances"].add(b)
            t = link_type(cells, a, b)
            regions[rb]["walk" if t == "pe" else ("jump" if t == "pulo" else "drop")] += 1

    # 3) custo de acesso (pulos) desde a maior região
    main = max(range(len(regions)), key=lambda r: len(regions[r]["areas"]))
    regions[main]["cost"] = 0
    changed = True
    while changed:
        changed = False
        for a in cells:
            for b in links[a]:
                ra, rb = region[a], region[b]
                if ra == rb or regions[ra]["cost"] >= 99:
                    continue
                c = regions[ra]["cost"] + (1 if link_type(cells, a, b) == "pulo" else 0)
                if c < regions[rb]["cost"]:
                    regions[rb]["cost"] = c
                    changed = True

    for r, reg in enumerate(regions):
        ent = reg["walk"] * 1.0 + reg["drop"] * 0.7 + reg["jump"] * 0.3
        few = 1.0 / (1.0 + 0.5 * ent)
        hard = min(reg["cost"], 4) / 4.0
        size = len(reg["areas"]) * CELL * CELL
        sf = 1.0 if size <= 150000 else (0.5 if size >= 600000 else 1.0 - 0.5 * (size - 150000) / 450000)
        reg["defense"] = 0.0 if (r == main or reg["cost"] >= 99) else max(0.0, min(1.0, (0.6 * few + 0.4 * hard) * sf))

    # 4) nota por área
    keys = list(cells)
    vis = {c: [o for o in keys if o != c and los(c, o)] for c in keys}
    maxvis = max(len(v) for v in vis.values())
    scores = {}
    for c in keys:
        reg = regions[region[c]]
        exposure = len(vis[c]) / maxvis
        avgz = sum(cells[o] for o in vis[c]) / max(1, len(vis[c]))
        height = max(0.0, min(1.0, 0.5 + (cells[c] - avgz) / 256.0))
        ents = [e for e in reg["entrances"]]
        watch = (sum(1 for e in ents if e == c or los(c, e)) / len(ents)) if ents else 0.5
        cover = 0.0
        score = 0.45 * reg["defense"] + 0.25 * (1 - exposure) + 0.12 * height + 0.08 * cover + 0.10 * watch
        scores[f"{c[0]},{c[1]}"] = round(score if reg["cost"] < 99 else 0.0, 3)

    named = {"L": "Laboratório (só por 3 pilastras)", "c": "Topo de caixa", "p": "Plataforma (degraus)", ".": "Chão"}
    summary = {}
    for (x, y), ch_z in cells.items():
        ch = TACT_MAP[y][x]
        key = named.get(ch, "Pilastra" if ch in "123" else ("Degrau" if ch == "s" else "Chão"))
        summary.setdefault(key, []).append(scores[f"{x},{y}"])
    resumo = {k: dict(media=round(sum(v) / len(v), 3), max=round(max(v), 3)) for k, v in summary.items()}
    lab_region = region[next((x, y) for y, row in enumerate(TACT_MAP) for x, cc in enumerate(row) if cc == "L")]
    return dict(
        mapa=TACT_MAP, alturas={f"{x},{y}": z for (x, y), z in cells.items()}, notas=scores, resumo=resumo,
        laboratorio=dict(acesso=regions[lab_region]["cost"], entradas_pe=regions[lab_region]["walk"],
                         entradas_pulo=regions[lab_region]["jump"], entradas_queda=regions[lab_region]["drop"],
                         defesa=round(regions[lab_region]["defense"], 3)),
        padrao="O bot padrão do HL2MP não avalia lugares: só caça (SeekAndDestroy). Não há nota para comparar.",
    )


# ============================================================== main

def main():
    rng = random.Random(SEED)
    out = dict(
        semente=SEED,
        percepcao=run_perception(rng),
        duelo=run_duel(rng),
        disciplina=run_discipline(rng),
        strafe=run_strafe(rng),
        rotas=run_routes(rng),
        tatico=run_tactical(),
    )
    path = sys.argv[1] if len(sys.argv) > 1 else "botsim_result.json"
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, separators=(",", ":"))
    print("ok:", path)


if __name__ == "__main__":
    main()
