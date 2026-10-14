// Trok Hitmarker (.asi) -- Victor_Trok
// O dano real: o SA-MP manda ao servidor o dano que o GTA calculou (46.2 na Deagle), mas cada servidor aplica o seu
// (arma configurada, headshot que mata de uma vez, colete). Entao, a cada tiro, o mod guarda a vida + colete que o
// sync do alvo mostrava e espera ela cair: a queda e o dano de verdade. Tiros que chegam juntos dividem a queda pelo
// que cada um costuma tirar (o mod aprende a proporcao de cada arma com os tiros sozinhos). Se a vida nao cair em 1 s
// (servidor que esconde a vida, alvo AFK), vale o numero do GTA, mostrado como "~46".
// Abate: a mensagem de morte com o seu nick como matador, ou a vida do alvo chegando a zero depois de um tiro seu.

#include "logic.h"

#include "samp.h"
#include "sound.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

void HitLog(const char* fmt, ...); // main.cpp

namespace logic {
namespace {

constexpr int MAX_PLAYERS = 1004;
constexpr float RESOLVE_TIMEOUT = 1.0f; // s sem a vida cair: vale o numero do GTA (~)
constexpr float KILL_LINK = 2.0f;       // s: o abate confirmado vale para o tiro mais novo naquele alvo
constexpr float MERGE_WINDOW = 0.6f;    // s: o numero que ainda esta subindo recebe o proximo tiro (somar)
constexpr float SHOT_KEEP = 6.0f;       // s: o maior tempo do local do acerto (5 s) e mais um pouco

struct Shot {
    int target = -1;
    int part = 3, weapon = 0;
    float gta = 0;    // o dano que o GTA calculou
    float time = 0;   // quando o tiro saiu
    float effect = 0; // quando o efeito na mira comecou (o abate recomeca)
    bool resolved = false, shown = false, kill = false;
    hud::Kind kind = hud::HEALTH;  // o que aparece na mira (decidido na hora do tiro)
    hud::Kind under = hud::HEALTH; // no abate: o que o tiro pegou
};

struct Target {
    float health = 0, armour = 0; // base: o sync antes dos tiros que ainda esperam a queda
    float killedAt = -100;
    int drift = 0;
};

struct Taken { // tiro levado esperando a sua vida cair
    int issuer = -1;
    float gta = 0, time = 0;
    int part = 3;
    bool resolved = false;
};

struct LNumber {
    hud::Number n;
    int target = -1, part = 3;
};

struct LIncoming {
    int issuer = -1;
    hud::Incoming in;
};

float g_now = 0;
std::deque<Shot> g_shots;
Target g_targets[MAX_PLAYERS];
float g_ratio[64]; // dano real / dano do GTA, por arma
std::vector<LNumber> g_numbers;
std::vector<hud::Line> g_given, g_taken;
std::vector<LIncoming> g_incoming;
std::deque<Taken> g_takenShots;
float g_localHealth = -1, g_localArmour = 0; // o ultimo quadro (antes do tiro que chega agora)
float g_baseHealth = 0, g_baseArmour = 0;    // base dos tiros levados pendentes
bool g_localAlive = false;
float g_flash = 0;
hud::Kind g_flashKind = hud::HEALTH;
float g_lastTakenSound = -1;
bool g_ready = false;
int g_logged = 0; // o log mostra os primeiros eventos de cada sessao (para conferir o dano real no servidor)

template <class... Args>
void Note(const char* fmt, Args... args) {
    if (g_logged < 40) {
        ++g_logged;
        HitLog(fmt, args...);
    }
}

int Round(float v) {
    return static_cast<int>(std::floor(v + 0.5f));
}

int Priority(hud::Kind k) {
    return k == hud::KILL ? 3 : k == hud::HEADSHOT ? 2 : k == hud::ARMOUR ? 1 : 0;
}

bool ValidId(int id) {
    return id >= 0 && id < MAX_PLAYERS;
}

float Ratio(int weapon) {
    return weapon >= 0 && weapon < 64 ? g_ratio[weapon] : 1.0f;
}

void Init() {
    if (!g_ready) {
        g_ready = true;
        std::fill(g_ratio, g_ratio + 64, 1.0f);
    }
}

// ---------------------------------------------------------------- o que aparece

hud::Line* FindLine(std::vector<hud::Line>& lines, int id) {
    for (hud::Line& l : lines) {
        if (l.id == id) {
            return &l;
        }
    }
    return nullptr;
}

// Informer: a pessoa vai para o topo; o combo (acertos e dano somado) fecha depois de comboWindow s sem acertar.
void AddToLine(std::vector<hud::Line>& lines, int id, int value, hud::Kind kind, bool estimated,
               const opt::Options& o) {
    if (o.informer == 2 || !ValidId(id)) {
        return;
    }
    samp::Player p;
    bool have = samp::GetPlayer(id, &p);
    auto it = std::find_if(lines.begin(), lines.end(), [id](const hud::Line& l) { return l.id == id; });
    hud::Line line;
    if (it != lines.end()) {
        line = std::move(*it);
        lines.erase(it);
    } else {
        line.id = id;
        line.nick = have ? p.nick : "?";
    }
    if (line.age > o.comboWindow) {
        line.hits = 0;
        line.total = 0;
    }
    if (have) {
        line.nick = p.nick;
        line.nickColor = p.color;
        line.afk = o.afk && p.afk;
    }
    line.hits += 1;
    line.total += value;
    line.age = 0;
    hud::Hit h;
    h.value = value;
    h.kind = kind;
    h.estimated = estimated;
    line.recent.push_back(h);
    if (line.recent.size() > 12) {
        line.recent.erase(line.recent.begin());
    }
    lines.insert(lines.begin(), std::move(line));
    if (lines.size() > 6) {
        lines.pop_back();
    }
}

void AddNumber(int target, int part, int value, hud::Kind kind, bool estimated, const opt::Options& o) {
    if (o.numbers == 2) {
        return;
    }
    if (o.numbersSum) {
        for (LNumber& ln : g_numbers) {
            if (ln.target == target && ln.n.age < MERGE_WINDOW && ln.n.estimated == estimated) {
                ln.n.value += value;
                if (Priority(kind) > Priority(ln.n.kind)) {
                    ln.n.kind = kind;
                }
                ln.n.age = std::min(ln.n.age, 0.2f); // segura embaixo enquanto os tiros somam
                ln.n.bump = 0;                       // e da um pulinho
                ln.part = part;
                return;
            }
        }
    }
    static const float drifts[] = {0.0f, 0.7f, -0.7f};
    LNumber ln;
    ln.target = target;
    ln.part = part;
    ln.n.value = value;
    ln.n.kind = kind;
    ln.n.estimated = estimated;
    ln.n.bump = 1.0f;
    ln.n.from = ImVec2(-10000, -10000); // so aparece quando o alvo estiver na tela
    ln.n.drift = ValidId(target) ? drifts[g_targets[target].drift++ % 3] : 0.0f;
    g_numbers.push_back(ln);
    if (g_numbers.size() > 24) {
        g_numbers.erase(g_numbers.begin());
    }
}

void Show(Shot& s, int value, hud::Kind kind, bool estimated, const opt::Options& o) {
    s.shown = true;
    if (s.kill) {
        kind = hud::KILL;
    }
    AddNumber(s.target, s.part, value, kind, estimated, o);
    AddToLine(g_given, s.target, value, kind, estimated, o);
}

void Resolve(Shot& s, int value, hud::Kind kind, bool estimated, const opt::Options& o) {
    s.resolved = true;
    Note(estimated ? "acerto: id %d, parte %d, arma %d, GTA %.1f -> a vida nao caiu em 1 s, vale o do GTA (~%d)"
                   : "acerto: id %d, parte %d, arma %d, GTA %.1f -> real %d",
         s.target, s.part, s.weapon, s.gta, value);
    if (!s.shown) {
        Show(s, value, kind, estimated, o);
    }
}

// Abate: o tiro mais novo naquele alvo vira abate (o efeito recomeca na mira) e o numero e o informer ficam vermelhos.
void ConfirmKill(int id, const char* how, const opt::Options& o) {
    Target& t = g_targets[id];
    if (g_now - t.killedAt < 3.0f) {
        return;
    }
    t.killedAt = g_now;
    Note("abate: id %d (%s)", id, how);
    Shot* last = nullptr;
    for (auto it = g_shots.rbegin(); it != g_shots.rend(); ++it) {
        if (it->target == id && g_now - it->time < KILL_LINK) {
            last = &*it;
            break;
        }
    }
    if (!last) {
        Shot s; // morte com o seu nick sem tiro recente (atropelado, explosao): so o efeito e o som
        s.target = id;
        s.time = s.effect = g_now;
        s.resolved = s.shown = true;
        g_shots.push_back(s);
        last = &g_shots.back();
    }
    if (!last->kill) {
        last->kill = true;
        last->under = last->kind;
        last->kind = hud::KILL;
        last->effect = g_now;
    }
    LNumber* youngest = nullptr;
    for (LNumber& ln : g_numbers) {
        if (ln.target == id && (!youngest || ln.n.age < youngest->n.age)) {
            youngest = &ln;
        }
    }
    if (youngest && youngest->n.age < 1.0f) {
        youngest->n.kind = hud::KILL;
    }
    hud::Line* line = FindLine(g_given, id);
    if (line && !line->recent.empty() && line->age < 1.0f) {
        line->recent.back().kind = hud::KILL;
    }
    if (last->under == hud::HEADSHOT && o.sKillHead) {
        sound::Play(sound::KILL_HEAD);
    } else if (o.sKill) {
        sound::Play(sound::KILL);
    }
}

// Quantos dos tiros pendentes (do mais velho) a queda cobre: a soma do que cada um costuma tirar mais perto dela.
template <class Expected>
size_t PickCount(size_t pending, float drop, Expected expected) {
    size_t best = 1;
    float bestErr = 1e30f, sum = 0;
    for (size_t k = 1; k <= pending; ++k) {
        sum += expected(k - 1);
        float err = std::fabs(drop - sum);
        if (err < bestErr - 0.5f) {
            bestErr = err;
            best = k;
        }
    }
    return best;
}

// ---------------------------------------------------------------- dano dado

void ResolveTargets(const opt::Options& o) {
    std::vector<int> ids;
    for (const Shot& s : g_shots) {
        if (!s.resolved && std::find(ids.begin(), ids.end(), s.target) == ids.end()) {
            ids.push_back(s.target);
        }
    }
    for (int id : ids) {
        std::vector<Shot*> pend;
        for (Shot& s : g_shots) {
            if (s.target == id && !s.resolved) {
                pend.push_back(&s);
            }
        }
        Target& t = g_targets[id];
        float health = 0, armour = 0;
        if (samp::Vitals(id, &health, &armour)) {
            float before = t.health + t.armour, now = health + armour;
            if (now > before + 0.5f) { // curou ou nasceu de novo: a base sobe
                t.health = health;
                t.armour = armour;
            } else if (now < before - 0.5f) {
                float drop = before - now;
                bool dead = health <= 0.0f;
                auto expected = [&pend](size_t i) { return pend[i]->gta * Ratio(pend[i]->weapon); };
                size_t k = dead ? pend.size() : PickCount(pend.size(), drop, expected);
                bool armourHit = t.armour - armour > t.health - health;
                if (k == 1 && pend.size() == 1 && !dead && pend[0]->part != 9 && pend[0]->gta > 1.0f &&
                    pend[0]->weapon >= 0 && pend[0]->weapon < 64) {
                    float r = std::min(10.0f, std::max(0.1f, drop / pend[0]->gta));
                    g_ratio[pend[0]->weapon] = g_ratio[pend[0]->weapon] * 0.7f + r * 0.3f;
                }
                float sum = 0;
                for (size_t i = 0; i < k; ++i) {
                    sum += std::max(0.1f, expected(i));
                }
                int left = Round(drop);
                for (size_t i = 0; i < k; ++i) {
                    int v = i + 1 == k ? left : Round(drop * std::max(0.1f, expected(i)) / sum);
                    left -= v;
                    hud::Kind kind = pend[i]->part == 9 ? hud::HEADSHOT : armourHit ? hud::ARMOUR : hud::HEALTH;
                    Resolve(*pend[i], std::max(0, v), kind, false, o);
                }
                t.health = health;
                t.armour = armour;
                if (dead) {
                    ConfirmKill(id, "a vida zerou", o);
                }
            }
        }
        for (Shot* s : pend) {
            if (!s->resolved && g_now - s->time > RESOLVE_TIMEOUT) {
                Resolve(*s, Round(s->gta), s->kind, true, o);
            }
        }
    }
}

// ---------------------------------------------------------------- dano levado

void ResolveLocal(const game::Local& me, const opt::Options& o) {
    bool any = false;
    for (const Taken& tk : g_takenShots) {
        any = any || !tk.resolved;
    }
    if (!any || !me.ped) {
        return;
    }
    float before = g_baseHealth + g_baseArmour, now = me.health + me.armour;
    if (now > before + 0.5f) {
        g_baseHealth = me.health;
        g_baseArmour = me.armour;
    } else if (now < before - 0.5f) {
        float drop = before - now;
        std::vector<Taken*> pend;
        for (Taken& tk : g_takenShots) {
            if (!tk.resolved) {
                pend.push_back(&tk);
            }
        }
        size_t k = !me.alive ? pend.size() : PickCount(pend.size(), drop, [&pend](size_t i) { return pend[i]->gta; });
        float sum = 0;
        for (size_t i = 0; i < k; ++i) {
            sum += std::max(0.1f, pend[i]->gta);
        }
        int left = Round(drop);
        for (size_t i = 0; i < k; ++i) {
            int v = i + 1 == k ? left : Round(drop * std::max(0.1f, pend[i]->gta) / sum);
            left -= v;
            pend[i]->resolved = true;
            Note("dano levado: id %d, GTA %.1f -> real %d", pend[i]->issuer, pend[i]->gta, v);
            if (o.source == 0) {
                AddToLine(g_taken, pend[i]->issuer, std::max(0, v), hud::HEALTH, false, o);
            }
        }
        g_baseHealth = me.health;
        g_baseArmour = me.armour;
    }
    for (Taken& tk : g_takenShots) {
        if (!tk.resolved && g_now - tk.time > RESOLVE_TIMEOUT) {
            tk.resolved = true;
            Note("dano levado: id %d, GTA %.1f -> a vida nao caiu em 1 s, vale o do GTA (~%d)", tk.issuer, tk.gta,
                 Round(tk.gta));
            if (o.source == 0) {
                AddToLine(g_taken, tk.issuer, Round(tk.gta), hud::HEALTH, true, o);
            }
        }
    }
}

void AddIncoming(int issuer, float damage) {
    for (LIncoming& li : g_incoming) {
        if (li.issuer == issuer && li.in.age < 1.0f) {
            li.in.age = 0;
            li.in.damage += Round(damage);
            return;
        }
    }
    LIncoming li;
    li.issuer = issuer;
    li.in.damage = Round(damage);
    li.in.angle = 0;
    DWORD ped = samp::PedOf(issuer);
    game::Local me = game::LocalPlayer();
    game::Vec3 from;
    if (ped && me.ped && game::PedPos(ped, &from)) {
        li.in.angle = game::AngleFromCamera(me.pos, from);
    }
    g_incoming.push_back(li);
    if (g_incoming.size() > 6) {
        g_incoming.erase(g_incoming.begin());
    }
}

template <class T, class Pred>
void RemoveIf(T& list, Pred pred) {
    list.erase(std::remove_if(list.begin(), list.end(), pred), list.end());
}

} // namespace

void Reset() {
    Init();
    g_shots.clear();
    g_takenShots.clear();
    g_numbers.clear();
    g_given.clear();
    g_taken.clear();
    g_incoming.clear();
    for (Target& t : g_targets) {
        t = Target();
    }
    g_flash = 0;
}

void OnGive(int id, float damage, int weapon, int part) {
    Init();
    const opt::Options& o = opt::Get();
    if (!o.enabled || !ValidId(id)) {
        return;
    }
    Shot s;
    s.target = id;
    s.part = part;
    s.weapon = weapon;
    s.gta = damage;
    s.time = s.effect = g_now;
    float health = 0, armour = 0;
    bool have = samp::Vitals(id, &health, &armour);
    bool waiting = false;
    for (const Shot& old : g_shots) {
        waiting = waiting || (old.target == id && !old.resolved);
    }
    if (have && !waiting) { // primeiro tiro pendente: a base e o que o sync mostra agora
        g_targets[id].health = health;
        g_targets[id].armour = armour;
    }
    s.kind = part == 9 ? hud::HEADSHOT : have && armour > 0.0f ? hud::ARMOUR : hud::HEALTH;
    s.under = s.kind;
    g_shots.push_back(s);
    if (g_shots.size() > 256) { // minimizado (sem quadros) no meio de uma troca de tiros
        g_shots.pop_front();
    }
    Shot& added = g_shots.back();
    if (o.source == 1) {
        Show(added, Round(damage), added.kind, false, o); // "Do GTA": na hora, como o damage informer antigo
    }
    if (!have) {
        Resolve(added, Round(damage), added.kind, o.source == 0, o);
    }
    if (part == 9 && o.sHeadDiff) {
        sound::Play(sound::HEAD);
    } else if (o.sHit) {
        sound::Play(sound::HIT);
    }
}

void OnTake(int id, float damage, int weapon, int part) {
    Init();
    (void)weapon;
    const opt::Options& o = opt::Get();
    if (!o.enabled) {
        return;
    }
    bool waiting = false;
    for (const Taken& tk : g_takenShots) {
        waiting = waiting || !tk.resolved;
    }
    if (!waiting) {
        game::Local me = game::LocalPlayer();
        g_baseHealth = g_localHealth >= 0 ? g_localHealth : me.health;
        g_baseArmour = g_localHealth >= 0 ? g_localArmour : me.armour;
    }
    Taken tk;
    tk.issuer = ValidId(id) ? id : -1; // 65535: queda, explosao, afogamento
    tk.gta = damage;
    tk.time = g_now;
    tk.part = part;
    g_takenShots.push_back(tk);
    if (g_takenShots.size() > 32) {
        g_takenShots.pop_front();
    }
    g_flash = 1.0f;
    g_flashKind = part == 9 ? hud::HEADSHOT : g_localArmour > 0.0f ? hud::ARMOUR : hud::HEALTH;
    if (tk.issuer >= 0) {
        AddIncoming(tk.issuer, damage);
        if (o.source == 1) {
            AddToLine(g_taken, tk.issuer, Round(damage), hud::HEALTH, false, o);
        }
    }
    if (o.sTaken && g_now - g_lastTakenSound > 0.07f) {
        g_lastTakenSound = g_now;
        sound::Play(sound::TAKEN);
    }
}

void OnDeath(const char* killer, const char* victim, int weapon) {
    Init();
    (void)weapon;
    const opt::Options& o = opt::Get();
    if (!o.enabled || !killer || !victim || !killer[0] || !victim[0]) {
        return;
    }
    std::string me = samp::LocalNameRaw();
    if (me.empty() || me != killer || me == victim) {
        return;
    }
    int id = samp::FindByName(victim);
    if (ValidId(id)) {
        ConfirmKill(id, "mensagem de morte", o);
    }
}

void Frame(float dt, ImVec2 screen, const game::Aim& aim, const opt::Options& o, hud::State* st) {
    Init();
    dt = std::max(0.0f, std::min(dt, 0.25f));
    g_now += dt;
    float k = screen.y / 1080.0f;

    // o jogador local: dano levado e a propria morte
    game::Local me = game::LocalPlayer();
    ResolveLocal(me, o);
    if (me.ped) {
        if (g_localAlive && !me.alive && o.enabled) {
            Note("morte do jogador local");
            if (o.sDeath) {
                sound::Play(sound::DEATH);
            }
        }
        g_localAlive = me.alive;
        g_localHealth = me.health;
        g_localArmour = me.armour;
    }
    ResolveTargets(o);

    // o tempo passa
    for (LNumber& ln : g_numbers) {
        ln.n.age += dt;
        ln.n.bump += dt;
    }
    RemoveIf(g_numbers, [](const LNumber& ln) { return ln.n.age > 1.6f; });
    float lineKeep = std::max(3.5f, static_cast<float>(o.comboWindow));
    for (std::vector<hud::Line>* lines : {&g_given, &g_taken}) {
        for (hud::Line& l : *lines) {
            l.age += dt;
            for (hud::Hit& h : l.recent) {
                h.age += dt;
            }
            RemoveIf(l.recent, [](const hud::Hit& h) { return h.age > 2.0f; });
        }
        RemoveIf(*lines, [lineKeep](const hud::Line& l) { return l.age > lineKeep; });
    }
    for (LIncoming& li : g_incoming) {
        li.in.age += dt;
    }
    RemoveIf(g_incoming, [](const LIncoming& li) { return li.in.age > 1.6f; });
    while (!g_shots.empty() && g_shots.front().resolved && g_now - g_shots.front().effect > SHOT_KEEP) {
        g_shots.pop_front();
    }
    while (!g_takenShots.empty() && g_takenShots.front().resolved) {
        g_takenShots.pop_front();
    }
    g_flash = std::max(0.0f, g_flash - dt / 0.6f);

    // o que o hud desenha
    st->crosshair = aim.drawn ? aim.center : game::DefaultAim(screen);
    // Mirando, a do jogo saiu da tela (game.cpp, mesmas condicoes): a do Trok aparece sempre no lugar dela.
    bool own = o.crosshair == 1 && me.ped && !me.inVehicle && !game::ExcludedWeapon(me.weapon);
    st->showCrosshair = own && (aim.drawn || (o.crosshairAlways && me.alive && me.followed && !game::HudHidden()));
    st->markers.clear();
    float bodyKeep = o.hitLocal ? static_cast<float>(o.hitLocalTime) : 0.0f;
    for (const Shot& s : g_shots) {
        float age = g_now - s.effect;
        if (age > std::max(bodyKeep, 1.3f)) { // o abate (1,2 s) e o local do acerto ja acabaram
            continue;
        }
        hud::Marker m;
        m.age = age;
        m.kind = s.kind;
        m.under = s.under;
        DWORD ped = age < bodyKeep ? samp::PedOf(s.target) : 0;
        m.bodyVisible = ped && game::PartOnScreen(ped, s.part, &m.body);
        st->markers.push_back(m);
    }
    st->numbers.clear();
    float ns = k * o.numbersSize / 100.0f;
    for (LNumber& ln : g_numbers) {
        DWORD ped = samp::PedOf(ln.target);
        ImVec2 at;
        if (ped && game::PartOnScreen(ped, ln.part, &at)) {
            ln.n.from = ImVec2(at.x + 44 * ns, at.y - 6 * ns); // ao lado do ponto do tiro: nao cobre a mira
        }
        if (o.numbers == 1 || ln.n.from.x > -5000) {
            st->numbers.push_back(ln.n);
        }
    }
    st->given = g_given;
    st->taken = g_taken;
    st->incoming.clear();
    for (LIncoming& li : g_incoming) {
        DWORD ped = samp::PedOf(li.issuer);
        game::Vec3 from;
        if (ped && me.ped && game::PedPos(ped, &from)) {
            li.in.angle = game::AngleFromCamera(me.pos, from); // segue enquanto voce vira
        }
        st->incoming.push_back(li.in);
    }
    st->flash = g_flash;
    st->flashKind = g_flashKind;
}

} // namespace logic
