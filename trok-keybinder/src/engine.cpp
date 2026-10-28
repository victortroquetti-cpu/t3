// Trok KeyBinder -- o motor. Ver engine.h.

#include "engine.h"

#include <algorithm>

namespace kb {

void Engine::Reset() {
    keys_.clear();
    pending_.clear();
    queue_.clear();
    chatFired_.clear();
    prevValid_ = false;
}

void Engine::ResetKeys() {
    for (KeyState& k : keys_) {
        k.fired = true; // uma tecla ja segura nao dispara por causa da troca
        k.lastTap = 0;
    }
}

void Engine::Note(const std::string& title, const std::string& text) {
    if (notices_.size() < 8) {
        notices_.push_back({title, text});
    }
}

std::vector<Notice> Engine::TakeNotices() {
    std::vector<Notice> out;
    out.swap(notices_);
    return out;
}

void Engine::Enqueue(const std::vector<std::string>& lines) {
    for (const std::string& l : lines) {
        if (queue_.size() >= MAX_QUEUE) {
            Note("Fila cheia", "Muitas mensagens de uma vez: o resto foi descartado.");
            return;
        }
        queue_.push_back(l);
    }
}

bool Engine::Next(unsigned now, int flood, std::string* out) {
    if (queue_.empty()) {
        return false;
    }
    if (sentAny_ && now - lastSend_ < static_cast<unsigned>(flood)) {
        return false;
    }
    *out = queue_.front();
    queue_.pop_front();
    lastSend_ = now;
    sentAny_ = true;
    return true;
}

bool Engine::Run(const Bind& b, const World& w, const Player* target) {
    std::vector<std::string> lines = Lines(b.text);
    std::string title = b.name.empty() ? std::string(ClassName(b.cls)) : b.name;
    if (lines.empty()) {
        Note(title, "Esta bind n\xC3\xA3o tem texto.");
        return false;
    }
    if (!w.sendReady) {
        Note(title, "O chat do SA-MP ainda n\xC3\xA3o est\xC3\xA1 pronto.");
        return false;
    }
    std::vector<Player> targets;
    if (b.cls == CLASS_VIEW) {
        if (target) {
            targets.push_back(*target);
        } else if (w.hasTarget) {
            targets.push_back(w.target);
        } else {
            Note(title, "Ningu\xC3\xA9m na sua mira.");
            return false;
        }
    } else if (b.cls == CLASS_AROUND) {
        for (const Player& p : w.players) {
            if (p.distance > 0 && p.distance <= static_cast<float>(b.radius) && !Ignored(b, p.nick)) {
                targets.push_back(p);
            }
        }
        std::sort(targets.begin(), targets.end(), [](const Player& a, const Player& c) { return a.distance < c.distance; });
        if (targets.empty()) {
            Note(title, "Ningu\xC3\xA9m a at\xC3\xA9 " + std::to_string(b.radius) + " m.");
            return false;
        }
        size_t keep = b.target == TARGET_ALL ? static_cast<size_t>(MAX_AROUND) : 1;
        if (targets.size() > keep) {
            targets.resize(keep);
        }
    }
    if (targets.empty()) {
        Enqueue(lines);
        return true;
    }
    for (const Player& p : targets) {
        std::vector<std::string> filled;
        for (const std::string& l : lines) {
            filled.push_back(Fill(l, p.id, p.nick));
        }
        Enqueue(filled);
    }
    return true;
}

void Engine::Schedule(const Bind& b, unsigned now, const Player* who) {
    Job s;
    s.due = now + static_cast<unsigned>(b.delay);
    s.cond = b.cond;
    for (const std::string& l : Lines(b.text)) {
        s.lines.push_back(who ? Fill(l, who->id, who->nick) : l);
    }
    if (!s.lines.empty()) {
        pending_.push_back(s);
    }
}

bool Engine::Edge(const Bind& b, const Me& prev, const Me& me) const {
    bool alive = me.spawned && !me.dead && me.health > 0;
    float v = static_cast<float>(b.value);
    switch (b.cond) {
    case C_ENTER_VEHICLE:
        return !prev.inVehicle && me.inVehicle &&
               (b.seat == SEAT_ANY || (b.seat == SEAT_DRIVER) == me.driver);
    case C_EXIT_VEHICLE:
        return prev.inVehicle && !me.inVehicle && !me.dead;
    case C_HEALTH:
        return alive && prev.spawned && prev.health >= v && me.health < v;
    case C_ARMOUR:
        return alive && prev.spawned && prev.armour >= v && me.armour < v;
    case C_VEHICLE_HEALTH:
        return prev.inVehicle && me.inVehicle && prev.vehicle == me.vehicle && prev.vehicleHealth >= v &&
               me.vehicleHealth < v;
    case C_SPAWN:
        return !prev.spawned && me.spawned;
    case C_DEATH:
        return !prev.dead && me.dead;
    case C_WEAPON:
        return prev.spawned && me.spawned && prev.weapon != me.weapon &&
               (b.value == WEAPON_ANY ? me.weapon != 0 : me.weapon == b.value);
    case C_ENTER_INTERIOR:
        return prev.spawned && me.spawned && prev.interior == 0 && me.interior != 0;
    case C_EXIT_INTERIOR:
        return prev.spawned && me.spawned && prev.interior != 0 && me.interior == 0;
    case C_CONNECT:
        return !prev.connected && me.connected;
    default:
        return false;
    }
}

bool Engine::StillValid(int cond, const Me& me) {
    switch (cond) {
    case C_ENTER_VEHICLE: return me.inVehicle;
    case C_EXIT_VEHICLE: return !me.inVehicle;
    case C_HEALTH:
    case C_ARMOUR:
    case C_VEHICLE_HEALTH:
    case C_WEAPON: return !me.dead;
    case C_SPAWN: return me.spawned;
    case C_ENTER_INTERIOR: return me.interior != 0;
    case C_EXIT_INTERIOR: return me.interior == 0;
    case C_CONNECT: return me.connected;
    default: return true;
    }
}

void Engine::Frame(const Preset& preset, const Settings& s, const World& w, const Keys* keys) {
    const std::vector<Bind>& binds = preset.binds;
    if (keys_.size() != binds.size()) {
        keys_.resize(binds.size());
        ResetKeys();
    }
    if (!w.me.connected) {
        queue_.clear();
        pending_.clear();
    }
    bool live = s.enabled && w.sendReady;

    // Automaticas: a borda de cada condicao (so com um quadro anterior valido).
    if (live && prevValid_) {
        for (const Bind& b : binds) {
            if (b.cls == CLASS_AUTO && b.enabled && b.cond != C_CHAT && Edge(b, prev_, w.me)) {
                Schedule(b, w.now, nullptr);
            }
        }
    }
    if (live) {
        for (const ChatLine& line : w.chat) {
            for (const Bind& b : binds) {
                if (b.cls != CLASS_AUTO || !b.enabled || b.cond != C_CHAT || !ChatMatches(b.trigger, line.text)) {
                    continue;
                }
                std::string key = Normalize(b.trigger) + '\x1f' + b.text;
                auto last = chatFired_.find(key);
                if (last != chatFired_.end() && w.now - last->second < static_cast<unsigned>(b.cooldown)) {
                    continue;
                }
                Player who;
                bool found = FindPlayer(line.text, line.author, w.players, &who);
                if (UsesTarget(b.text) && !found) {
                    Note(b.name.empty() ? std::string("Mensagem no chat") : b.name,
                         "O gatilho apareceu, mas o jogador da mensagem n\xC3\xA3o foi achado.");
                    continue;
                }
                chatFired_[key] = w.now;
                Schedule(b, w.now, found ? &who : nullptr);
            }
        }
    }
    prev_ = w.me;
    prevValid_ = true;

    // Agendadas: na hora, se nada estiver aberto; somem se a condicao deixou de valer.
    for (size_t i = 0; i < pending_.size();) {
        Job& p = pending_[i];
        bool due = static_cast<int>(w.now - p.due) >= 0;
        if (!live || !StillValid(p.cond, w.me) || (due && w.now - p.due > WAIT_LIMIT)) {
            pending_.erase(pending_.begin() + i);
            continue;
        }
        if (due && !w.blocked) {
            Enqueue(p.lines);
            pending_.erase(pending_.begin() + i);
            continue;
        }
        ++i;
    }

    // Teclas.
    for (size_t i = 0; i < binds.size(); ++i) {
        const Bind& b = binds[i];
        KeyState& k = keys_[i];
        bool down = keys && b.cls != CLASS_AUTO && !b.key.Empty() && keys->Down(b.key.key);
        if (!live || w.blocked || !b.enabled || b.cls == CLASS_AUTO || b.key.Empty() || !keys) {
            // Fechou o chat com a tecla ainda segura: so dispara num novo aperto.
            k.down = down;
            k.fired = true;
            k.lastTap = 0;
            continue;
        }
        bool rising = down && !k.down;
        k.down = down;
        bool fire = false;
        if (rising) {
            k.since = w.now;
            k.fired = false;
            if (!keys->ModsOk(b.key)) {
                k.fired = true; // outros modificadores: este aperto nao e desta bind
                continue;
            }
            if (b.mode == MODE_PRESS) {
                fire = true;
                k.fired = true;
            } else if (b.mode == MODE_DOUBLE) {
                if (k.lastTap && w.now - k.lastTap <= static_cast<unsigned>(s.doubleTap)) {
                    fire = true;
                    k.lastTap = 0;
                } else {
                    k.lastTap = w.now;
                }
                k.fired = true;
            }
        } else if (down && b.mode == MODE_HOLD && !k.fired) {
            if (!keys->ModsOk(b.key)) {
                k.fired = true;
            } else if (w.now - k.since >= static_cast<unsigned>(s.hold)) {
                fire = true;
                k.fired = true;
            }
        }
        if (fire) {
            Run(b, w);
        }
    }
}

} // namespace kb
