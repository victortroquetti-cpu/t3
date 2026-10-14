// Trok Hitmarker (.asi) -- Victor_Trok
// O dano real: o SA-MP manda ao servidor o dano que o GTA calculou (46.2 na Deagle), mas cada servidor aplica o seu
// (arma configurada, headshot que mata de uma vez, colete). Entao, a cada tiro, o mod guarda a vida + colete que o
// sync do alvo mostrava e espera ela cair: a queda e o dano de verdade.
//
// Cada alvo tem a sua conta (o id do SA-MP): tiros em pessoas diferentes nunca se misturam. Dentro do mesmo alvo:
//  - tiros seus que chegam juntos dividem a queda pelo que cada um costuma tirar (o mod aprende a proporcao de cada
//    arma pela mediana dos tiros sozinhos, sem contar alvo disputado);
//  - alvo disputado (a bala de outro jogador acertou ele, pelo bullet sync, ou a vida dele caiu sem tiro seu): a queda
//    pode ter dano dos outros, entao o seu tiro vale o que ele costuma tirar, nunca mais que a queda;
//  - abate: quem decide e o servidor (a mensagem de morte com o seu nick como matador). Servidor que nao manda mensagem
//    de morte: a vida do alvo zerando com o seu tiro, e so se o seu tiro sozinho bastava (alvo disputado);
//  - se a vida nao cair a tempo (servidor que esconde a vida, alvo AFK), vale o numero do GTA, mostrado como "~46". O
//    tempo de espera acompanha o ping medido (0,8 a 2 s).

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
constexpr float KILL_LINK = 2.0f;     // s: o abate confirmado vale para o tiro mais novo naquele alvo
constexpr float MERGE_WINDOW = 0.6f;  // s: um por tiro, o numero que ainda esta subindo recebe o proximo tiro
constexpr float SHOT_KEEP = 6.0f;     // s: o maior tempo do local do acerto (5 s) e mais um pouco
constexpr float WATCH = 15.0f;        // s: depois do ultimo tiro (seu ou de outro), a vida do alvo e lida todo quadro
constexpr float CONTESTED_DROP = 2.0f; // s: disputado por alguem sem bala (soco, explosao): perdeu vida sem tiro seu
constexpr int RATIO_SAMPLES = 5;

struct Shot {
    int target = -1;
    int part = 3, weapon = 0;
    float gta = 0;    // o dano que o GTA calculou
    float time = 0;   // quando o tiro saiu
    float effect = 0; // quando o efeito na mira comecou (o abate recomeca)
    bool resolved = false, shown = false, kill = false;
    float zeroValue = -1;  // na queda que zerou a vida: o que este tiro tirou pela ordem das balas (-1 = nao conta)
    hud::Kind kind = hud::HEALTH;  // o que aparece na mira (decidido na hora do tiro)
    hud::Kind under = hud::HEALTH; // no abate: o que o tiro pegou
};

struct Other { // bala de outro jogador neste alvo, esperando a queda dela
    float time = 0;
    float expected = 0; // o que a arma dele costuma tirar neste servidor
    int weapon = 0;
};

struct Target {
    std::vector<Other> others;    // balas dos outros ainda nao vistas na vida dele
    float health = 0, armour = 0; // base: o ultimo sync lido
    bool based = false;
    float watchUntil = -1;        // ate quando a vida e lida todo quadro
    float contestedUntil = -1;    // outros jogadores tambem estao tirando vida dele
    float killedAt = -100;        // abate seu confirmado
    float bornAt = -50;           // a vida dele voltou (nasceu de novo): uma morte por vida
    float otherKillAt = -100;     // o servidor deu a morte para outro (ou sem matador)
    float zeroAt = -1;            // a vida zerou com tiro seu no ar: esperando o servidor dizer quem matou
    float zeroDrop = 0;           // aquela queda (os tiros seus continuam esperando ate a decisao)
    float zeroBefore = 0;         // vida + colete antes dela
    bool zeroArmour = false;      // ela tirou mais colete que vida
    bool zeroMixed = false;       // tinha bala de outro (ou alguem sem bala) no ar
    bool zeroOrdered = false;     // a ordem das balas explicou a queda (zeroValue de cada tiro vale)
    bool zeroOurs = false;        // pela ordem, a bala que zerou foi sua (vale quando o servidor nao manda mensagem)
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
    int target = -1, part = 3; // somando na tela: target -1 (um so para todos os alvos)
    int slot = 0;              // somando: 0 corpo, 1 cabeca
    bool sum = false;          // somando (ainda pode crescer ate closeAt)
    float closeAt = 0;
};

struct LIncoming {
    int issuer = -1;
    hud::Incoming in;
};

float g_now = 0;
std::deque<Shot> g_shots;
Target g_targets[MAX_PLAYERS];
std::vector<int> g_watch; // alvos cuja vida e lida todo quadro
float g_samples[64][RATIO_SAMPLES]; // dano real / dano do GTA dos tiros sozinhos, por arma
int g_sampleCount[64];
float g_latency = 0.3f;   // s do seu tiro ate a queda chegar (media)
float g_otherLatency = 0.2f; // s da bala de outro jogador (o bullet sync chegando) ate a queda dela
int g_headKills[64], g_headLives[64]; // headshots sozinhos que mataram / que nao mataram, por arma
bool g_deathMessages = false; // o servidor manda mensagem de morte
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
int g_logged = 0; // sem "anotar todos", o log mostra so os primeiros eventos de cada sessao

template <class... Args>
void Note(const char* fmt, Args... args) {
    if (g_logged < 40 || opt::Get().logAll) {
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

// Mediana dos tiros sozinhos (1 sem amostra): um tiro misturado nao estraga a conta.
float Ratio(int weapon) {
    if (weapon < 0 || weapon >= 64 || g_sampleCount[weapon] == 0) {
        return 1.0f;
    }
    int n = std::min(g_sampleCount[weapon], RATIO_SAMPLES);
    float v[RATIO_SAMPLES];
    for (int i = 0; i < n; ++i) { // ordena as ate 5 amostras (insercao)
        float x = g_samples[weapon][i];
        int j = i;
        for (; j > 0 && v[j - 1] > x; --j) {
            v[j] = v[j - 1];
        }
        v[j] = x;
    }
    return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) * 0.5f;
}

// O headshot desta arma mata de uma vez neste servidor? (aprende com os seus headshots sozinhos)
bool HeadLethal(int weapon) {
    return weapon >= 0 && weapon < 64 && g_headKills[weapon] > 0 && g_headKills[weapon] > g_headLives[weapon];
}

// O que um tiro seu costuma tirar; headshot que mata de uma vez tira tudo o que o alvo tinha.
float Expected(const Shot& s, float before) {
    float e = std::max(0.1f, s.gta * Ratio(s.weapon));
    return s.part == 9 && HeadLethal(s.weapon) ? std::max(e, before) : e;
}

void Learn(int weapon, float ratio) {
    if (weapon < 0 || weapon >= 64) {
        return;
    }
    g_samples[weapon][g_sampleCount[weapon] % RATIO_SAMPLES] = std::min(10.0f, std::max(0.1f, ratio));
    ++g_sampleCount[weapon];
}

// O dano padrao do SA-MP por arma (o que o GTA calcula), para as balas dos outros jogadores: vezes a proporcao que o
// mod aprendeu com os seus tiros naquela arma, e o que ela costuma tirar neste servidor.
float BaseDamage(int weapon) {
    static const float table[47] = {
        1.32f, 1.32f, 1.32f, 1.32f, 1.32f, 1.32f, 1.32f, 1.32f, 1.32f, 27.06f, // 0-9: soco e brancas, motosserra
        1.32f, 1.32f, 1.32f, 1.32f, 1.32f, 1.32f, 82.5f, 0.33f, 0.1f, 0.0f,    // 10-19
        0.0f, 0.0f, 8.25f, 13.2f, 46.2f, 30.0f, 30.0f, 30.0f, 6.6f, 8.25f,     // 20-29: pistolas, escopetas, uzi, mp5
        9.9f, 9.9f, 6.6f, 24.75f, 41.25f, 82.5f, 82.5f, 0.33f, 46.2f, 82.5f,  // 30-39: ak, m4, tec9, rifles, rpg
        0.0f, 0.33f, 0.33f, 0.0f, 0.0f, 0.0f, 0.0f};
    return weapon >= 0 && weapon < 47 ? table[weapon] : 10.0f;
}

// Quanto esperar a vida cair: acompanha o ping (o tiro vai ao servidor, ao alvo e o sync dele volta).
float Timeout() {
    return std::min(2.0f, std::max(0.8f, g_latency * 2.5f + 0.3f));
}

void Watch(int id) {
    g_targets[id].watchUntil = g_now + WATCH;
    if (std::find(g_watch.begin(), g_watch.end(), id) == g_watch.end()) {
        g_watch.push_back(id);
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
void AddToLine(std::vector<hud::Line>& lines, int id, int value, hud::Kind kind, bool estimated, bool head, float life,
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
        line.head = 0;
        line.estimated = false;
    }
    if (have) {
        line.nick = p.nick;
        line.nickColor = p.color;
        line.afk = o.afk && p.afk;
    }
    line.hits += 1;
    line.total += value;
    line.head += head ? value : 0;
    line.estimated = line.estimated || estimated;
    if (line.last != hud::KILL || line.hits == 1) {
        line.last = kind;
    }
    line.age = 0;
    line.bump = 0;
    line.life = life;
    hud::Hit h;
    h.value = value;
    h.kind = kind;
    h.estimated = estimated;
    h.life = life;
    line.recent.push_back(h);
    if (line.recent.size() > 12) {
        line.recent.erase(line.recent.begin());
    }
    lines.insert(lines.begin(), std::move(line));
    if (lines.size() > 6) {
        lines.pop_back();
    }
}

void AddNumber(int target, int part, int value, hud::Kind kind, bool estimated, bool head, float life,
               const opt::Options& o) {
    if (o.numbers == 2) {
        return;
    }
    if (o.numbersCount == 1) {
        // Somando: um numero por alvo (na tela, um so para todos) que cresce enquanto voce acerta; com a cabeca
        // separada, o dano na cabeca cresce num numero proprio em cima do outro.
        int key = o.numbers == 1 ? -1 : target;
        int slot = o.splitHead && head ? 1 : 0;
        float window = static_cast<float>(o.sumWindow);
        for (LNumber& ln : g_numbers) {
            if (ln.sum && ln.target == key && ln.slot == slot && g_now < ln.closeAt) {
                ln.n.value += value;
                ln.n.estimated = ln.n.estimated || estimated;
                if (ln.n.kind != hud::KILL) {
                    ln.n.kind = slot == 1 ? hud::HEADSHOT : kind;
                }
                ln.n.bump = 0;
                ln.n.life = life;
                ln.n.stay = ln.n.age + window;
                ln.closeAt = g_now + window;
                return;
            }
        }
        LNumber ln;
        ln.sum = true;
        ln.target = key;
        ln.slot = slot;
        ln.part = 3; // ao lado do peito; o da cabeca empilhado em cima (lift)
        ln.n.value = value;
        ln.n.kind = slot == 1 ? hud::HEADSHOT : kind;
        ln.n.estimated = estimated;
        ln.n.life = life;
        ln.n.bump = 1.0f;
        ln.n.stay = window;
        ln.n.lift = slot == 1 ? 40.0f : 0.0f;
        ln.closeAt = g_now + window;
        ln.n.from = ImVec2(-10000, -10000);
        g_numbers.push_back(ln);
        return;
    }
    if (o.numbersSum) {
        for (LNumber& ln : g_numbers) {
            if (!ln.sum && ln.target == target && ln.n.age < MERGE_WINDOW && ln.n.estimated == estimated) {
                ln.n.value += value;
                if (Priority(kind) > Priority(ln.n.kind)) {
                    ln.n.kind = kind;
                }
                ln.n.age = std::min(ln.n.age, 0.2f); // segura embaixo enquanto a rajada soma
                ln.n.bump = 0;                       // e da um pulinho
                ln.n.life = life;
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
    ln.n.life = life;
    ln.n.bump = 1.0f;
    ln.n.from = ImVec2(-10000, -10000); // so aparece quando o alvo estiver na tela
    ln.n.drift = ValidId(target) ? drifts[g_targets[target].drift++ % 3] : 0.0f;
    g_numbers.push_back(ln);
    if (g_numbers.size() > 32) {
        g_numbers.erase(g_numbers.begin());
    }
}

// A vida que sobrou ao alvo (0..1, cor pela vida): o sync de agora; no "Do GTA" o numero sai antes da queda, entao
// tira o dano dele (o colete segura primeiro).
float LifeAfter(int target, int value, bool beforeDrop) {
    float health = 0, armour = 0;
    if (!samp::Vitals(target, &health, &armour)) {
        return -1;
    }
    if (beforeDrop) {
        health -= std::max(0.0f, value - armour);
    }
    return std::min(1.0f, std::max(0.0f, health / 100.0f));
}

void Show(Shot& s, int value, hud::Kind kind, bool estimated, const opt::Options& o) {
    s.shown = true;
    if (s.kill) {
        kind = hud::KILL;
    }
    float life = LifeAfter(s.target, value, !s.resolved);
    AddNumber(s.target, s.part, value, kind, estimated, s.part == 9, life, o);
    AddToLine(g_given, s.target, value, kind, estimated, s.part == 9, life, o);
}

// O tiro chegou com o alvo ja morto por outra pessoa: nao tirou nada, nao vira numero.
void ResolveDead(Shot& s) {
    s.resolved = true;
    s.shown = true;
    Note("acerto: id %d, parte %d, arma %d, GTA %.1f -> 0 (o alvo ja tinha morrido, abate de outro jogador)", s.target,
         s.part, s.weapon, s.gta);
}

void Resolve(Shot& s, int value, hud::Kind kind, bool estimated, float drop, bool contested, const opt::Options& o) {
    s.resolved = true;
    if (estimated) {
        Note("acerto: id %d, parte %d, arma %d, GTA %.1f -> a vida nao caiu em %.1f s, vale o do GTA (~%d)", s.target,
             s.part, s.weapon, s.gta, Timeout(), value);
    } else if (contested) {
        Note("acerto: id %d, parte %d, arma %d, GTA %.1f -> %d (alvo disputado: a queda foi %.0f, o seu e o que a arma "
             "costuma tirar)",
             s.target, s.part, s.weapon, s.gta, value, drop);
    } else {
        Note("acerto: id %d, parte %d, arma %d, GTA %.1f -> real %d", s.target, s.part, s.weapon, s.gta, value);
    }
    if (!s.shown) {
        Show(s, value, kind, estimated, o);
    }
}

// Abate: o tiro mais novo naquele alvo vira abate (o efeito recomeca na mira) e o numero e o informer ficam vermelhos.
void ConfirmKill(int id, const char* how, const opt::Options& o) {
    Target& t = g_targets[id];
    t.zeroAt = -1;
    if (t.killedAt > t.bornAt) {
        return; // esta vida ja deu abate (a mensagem e a vida zerada da mesma morte)
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
    // o numero que recebeu o ultimo acerto neste alvo (somando na tela: o de todos)
    LNumber* youngest = nullptr;
    auto fresh = [](const LNumber& ln) { return std::min(ln.n.age, ln.n.bump); };
    for (LNumber& ln : g_numbers) {
        bool mine = ln.target == id || (ln.sum && ln.target == -1);
        if (mine && (!youngest || fresh(ln) < fresh(*youngest))) {
            youngest = &ln;
        }
    }
    if (youngest && fresh(*youngest) < 1.0f) {
        youngest->n.kind = hud::KILL;
    }
    hud::Line* line = FindLine(g_given, id);
    if (line && line->age < 1.0f) {
        line->last = hud::KILL;
        if (!line->recent.empty()) {
            line->recent.back().kind = hud::KILL;
        }
    }
    if (last->under == hud::HEADSHOT && o.sKillHead) {
        sound::Play(sound::KILL_HEAD);
    } else if (o.sKill) {
        sound::Play(sound::KILL);
    }
}

std::vector<Shot*> Pending(int id) {
    std::vector<Shot*> pend;
    for (Shot& s : g_shots) {
        if (s.target == id && !s.resolved) {
            pend.push_back(&s);
        }
    }
    return pend;
}

// A queda que zerou a vida, dividida entre os seus tiros no ar: so seu, a queda toda; disputado, cada tiro vale o que
// costuma tirar, sem passar da queda.
void SplitDeath(int id, bool contested, const opt::Options& o) {
    Target& t = g_targets[id];
    std::vector<Shot*> pend = Pending(id);
    if (pend.empty()) {
        return;
    }
    float sumE = 0;
    for (Shot* s : pend) {
        sumE += Expected(*s, t.zeroBefore);
    }
    float scale = contested ? std::min(1.0f, t.zeroDrop / sumE) : t.zeroDrop / sumE;
    int left = Round(t.zeroDrop);
    for (size_t i = 0; i < pend.size(); ++i) {
        float e = Expected(*pend[i], t.zeroBefore);
        int v = !contested && i + 1 == pend.size() ? left : Round(e * scale);
        left -= v;
        hud::Kind kind = pend[i]->part == 9 ? hud::HEADSHOT : t.zeroArmour ? hud::ARMOUR : hud::HEALTH;
        Resolve(*pend[i], std::max(0, v), kind, false, t.zeroDrop, contested, o);
    }
}

// Os tiros seus no ar quando o alvo morreu pelas maos de outro: os que chegaram antes da bala que matou valem o que a
// ordem das balas diz; os outros chegaram com ele morto.
void ResolveBeforeKill(int id, const opt::Options& o) {
    Target& t = g_targets[id];
    for (Shot* s : Pending(id)) {
        if (t.zeroOrdered && s->zeroValue > 0.5f) {
            hud::Kind kind = s->part == 9 ? hud::HEADSHOT : t.zeroArmour ? hud::ARMOUR : hud::HEALTH;
            Resolve(*s, Round(s->zeroValue), kind, false, t.zeroDrop, true, o);
        } else {
            ResolveDead(*s);
        }
    }
}

// O servidor decidiu a morte que estava esperando (ou o tempo acabou).
void DecideDeath(int id, int verdict, const opt::Options& o) { // 1 sua, 0 de outro, -1 sem mensagem
    Target& t = g_targets[id];
    if (t.zeroAt < 0) {
        return;
    }
    t.zeroAt = -1;
    if (verdict == 0) {
        ResolveBeforeKill(id, o);
        Note("morte do id %d: o servidor deu o abate para outro jogador", id);
        return;
    }
    if (verdict == 1 || t.zeroOurs) {
        std::vector<Shot*> pend = Pending(id);
        if (!t.zeroMixed && pend.size() == 1 && pend[0]->part == 9 && pend[0]->weapon >= 0 && pend[0]->weapon < 64) {
            ++g_headKills[pend[0]->weapon]; // headshot sozinho que matou: neste servidor ele mata de uma vez
        }
        if (t.zeroMixed && t.zeroOrdered && t.zeroOurs) {
            ResolveBeforeKill(id, o); // a ordem das balas bate com quem matou: cada tiro vale o que ela diz
        } else {
            SplitDeath(id, t.zeroMixed, o);
        }
        // sem mensagem: pela ordem das balas, a que zerou foi a sua
        ConfirmKill(id, verdict == 1 ? "mensagem de morte" : "a vida zerou", o);
    } else {
        ResolveBeforeKill(id, o);
        Note("morte do id %d sem mensagem de morte: pela ordem das balas, quem matou foi outro jogador", id);
    }
}

// A vida do alvo zerou com tiro seu no ar. Quem matou quem decide e o servidor, pela mensagem de morte (ela chega
// junto da vida zerada, um pouco antes ou depois): ate la os tiros esperam.
void OnZero(int id, float drop, float before, bool armourHit, const opt::Options& o) {
    Target& t = g_targets[id];
    t.zeroAt = g_now;
    t.zeroDrop = drop;
    t.zeroBefore = before;
    t.zeroArmour = armourHit;
    if (t.killedAt > t.bornAt) {
        DecideDeath(id, 1, o); // a mensagem com o seu nick chegou antes da vida zerada
    } else if (t.otherKillAt > t.bornAt) {
        DecideDeath(id, 0, o); // a mensagem com outro matador chegou antes
    }
}

void ZeroTimeout(int id, const opt::Options& o) {
    Target& t = g_targets[id];
    float wait = g_deathMessages ? 1.0f : 0.35f;
    if (t.zeroAt >= 0 && g_now - t.zeroAt >= wait) {
        DecideDeath(id, -1, o);
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

// Uma queda de vida do alvo com tiros seus esperando. Sem ninguem mais acertando o alvo, a queda e toda sua e divide
// entre os seus tiros pelo que cada um costuma tirar. Com balas de outros jogadores no ar (bullet sync), a queda e
// comparada com as combinacoes dos tiros esperando (seus e deles, do mais velho para o mais novo): a que mais bate
// leva a queda, e o seu tiro vale o que costuma tirar, sem passar dela.
struct Event {
    float due;      // quando a queda dele deve chegar
    float expected;
    Shot* shot;     // seu; nullptr = de outro jogador
    size_t other;   // indice em Target::others
};

void Attribute(int id, std::vector<Shot*>& pend, float drop, float health, float armour, const opt::Options& o) {
    Target& t = g_targets[id];
    bool armourHit = t.armour - armour > t.health - health;
    // os eventos esperando, na ordem em que as quedas devem chegar (a sua bala vai ao servidor e ao alvo antes; a
    // dos outros voce ja ve chegando no alvo)
    std::vector<Event> ev;
    for (Shot* s : pend) {
        ev.push_back({s->time + g_latency, Expected(*s, t.health + t.armour), s, 0});
    }
    for (size_t i = 0; i < t.others.size(); ++i) {
        ev.push_back({t.others[i].time + g_otherLatency, t.others[i].expected, nullptr, i});
    }
    std::stable_sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) { return a.due < b.due; });
    if (health <= 0.0f) {
        // Zerou: pela ordem das balas, o que cada uma tirou e qual zerou a vida. Quem matou de verdade o servidor diz
        // (mensagem de morte); a ordem vale quando ele nao diz.
        float before = t.health + t.armour, remaining = before;
        bool found = false;
        t.zeroOurs = false;
        for (size_t i = 0; i < ev.size(); ++i) {
            const Event& e = ev[i];
            if (e.shot) {
                e.shot->zeroValue = found ? 0.0f : std::min(e.expected, std::max(0.0f, remaining));
            }
            float left = remaining; // o que faltava antes desta bala
            remaining -= e.expected;
            if (!found && remaining <= 0.5f) {
                found = true;
                // A sua zerou. Na duvida (balas dos outros chegando quase junto que tambem dariam para matar, se
                // tivessem chegado antes), nao e abate seu.
                float close = 0;
                for (size_t j = i + 1; j < ev.size(); ++j) {
                    if (!ev[j].shot && ev[j].due < e.due + 0.08f) {
                        close += ev[j].expected;
                    }
                }
                t.zeroOurs = e.shot != nullptr && close < left - 0.5f;
            }
        }
        t.zeroOrdered = found;
        t.zeroMixed = !t.others.empty() || g_now < t.contestedUntil;
        if (!found) { // as balas conhecidas nao bastavam: a sua leva se ninguem mais estava acertando
            t.zeroOurs = !t.zeroMixed;
        }
        t.others.clear();
        OnZero(id, drop, before, armourHit, o);
        return;
    }
    size_t n = std::min<size_t>(ev.size(), 10);

    // A combinacao que mais bate com a queda; pular um evento mais velho custa um pouco (a ordem costuma valer).
    unsigned best = 0;
    float bestCost = 1e30f;
    for (unsigned mask = 1; mask < (1u << n); ++mask) {
        float sum = 0;
        int skipped = 0, highest = 0;
        for (size_t i = 0; i < n; ++i) {
            if (mask & (1u << i)) {
                sum += ev[i].expected;
                highest = static_cast<int>(i);
            }
        }
        for (int i = 0; i < highest; ++i) {
            skipped += (mask & (1u << i)) ? 0 : 1;
        }
        float cost = std::fabs(drop - sum) + 1.5f * skipped;
        if (cost < bestCost - 0.01f) {
            bestCost = cost;
            best = mask;
        }
    }
    float sumE = 0, sumOurs = 0;
    bool othersIn = false;
    std::vector<size_t> usedOthers;
    std::vector<Shot*> ours;
    for (size_t i = 0; i < n; ++i) {
        if (!(best & (1u << i))) {
            continue;
        }
        sumE += ev[i].expected;
        if (ev[i].shot) {
            ours.push_back(ev[i].shot);
            sumOurs += ev[i].expected;
        } else {
            othersIn = true;
            usedOthers.push_back(ev[i].other);
        }
    }
    std::sort(usedOthers.rbegin(), usedOthers.rend());
    for (size_t i : usedOthers) {
        t.others.erase(t.others.begin() + i);
    }
    if (ours.empty()) {
        return; // a queda era dos outros: os seus continuam esperando
    }
    // Disputado (balas dos outros na conta, ou alguem tirando vida sem bala): o seu vale o que costuma tirar, sem passar
    // da queda. So seu: a queda inteira.
    bool mixed = othersIn || g_now < t.contestedUntil;
    float scale = mixed ? std::min(1.0f, drop / sumE) : drop / sumOurs;
    int left = Round(drop);
    for (size_t i = 0; i < ours.size(); ++i) {
        float e = Expected(*ours[i], t.health + t.armour);
        int v = !mixed && i + 1 == ours.size() ? left : Round(e * scale);
        left -= v;
        hud::Kind kind = ours[i]->part == 9 ? hud::HEADSHOT : armourHit ? hud::ARMOUR : hud::HEALTH;
        Resolve(*ours[i], std::max(0, v), kind, false, drop, mixed, o);
    }
    if (!mixed) {
        if (ours.size() == 1 && pend.size() == 1 && ours[0]->part != 9 && ours[0]->gta > 1.0f) {
            Learn(ours[0]->weapon, drop / ours[0]->gta);
        }
        if (ours.size() == 1 && pend.size() == 1 && ours[0]->part == 9 && ours[0]->weapon >= 0 &&
            ours[0]->weapon < 64) {
            ++g_headLives[ours[0]->weapon]; // headshot sozinho que nao matou
        }
        float delay = g_now - ours.back()->time;
        if (delay > 0.0f && delay < 2.5f) {
            g_latency = g_latency * 0.8f + delay * 0.2f;
        }
    }
}

// A vida de cada alvo vigiado, todo quadro.
void Track(int id, const opt::Options& o) {
    Target& t = g_targets[id];
    std::vector<Shot*> pend = Pending(id);
    float health = 0, armour = 0;
    bool have = samp::Vitals(id, &health, &armour);
    if (have) {
        if (!t.based) {
            t.based = true;
            t.health = health;
            t.armour = armour;
        }
        float before = t.health + t.armour, now = health + armour;
        if (now < before - 0.5f && t.zeroAt < 0) {
            if (pend.empty()) {
                // perdeu vida sem tiro seu: era uma bala de outro (sai da fila dele) ou alguem sem bala (soco, explosao)
                if (!t.others.empty()) {
                    size_t k = PickCount(t.others.size(), before - now,
                                         [&t](size_t i) { return t.others[i].expected; });
                    // uma bala so de outro jogador, sem ninguem mais: ensina quanto a arma dele tira neste servidor
                    // (as armas sao as mesmas para todo mundo)
                    if (k == 1 && t.others.size() == 1 && health > 0.0f && g_now >= t.contestedUntil &&
                        BaseDamage(t.others[0].weapon) > 1.0f) {
                        Learn(t.others[0].weapon, (before - now) / BaseDamage(t.others[0].weapon));
                        float delay = g_now - t.others[0].time; // e quanto a bala dos outros demora a aparecer
                        if (delay > 0.0f && delay < 2.0f) {
                            g_otherLatency = g_otherLatency * 0.8f + delay * 0.2f;
                        }
                    }
                    t.others.erase(t.others.begin(), t.others.begin() + k);
                } else {
                    t.contestedUntil = g_now + CONTESTED_DROP;
                }
                if (health <= 0.0f) {
                    t.others.clear();
                }
            } else {
                Attribute(id, pend, before - now, health, armour, o);
            }
        }
        if (t.health <= 0.0f && health > 0.0f) {
            t.bornAt = g_now; // nasceu de novo
        }
        t.health = health; // caiu (ja contado) ou subiu (curou, nasceu de novo)
        t.armour = armour;
    }
    while (!t.others.empty() && g_now - t.others.front().time > 1.5f) {
        t.others.erase(t.others.begin()); // bala de outro que nunca apareceu na vida dele
    }
    ZeroTimeout(id, o);
    if (t.zeroAt >= 0) {
        return; // esperando quem matou
    }
    for (Shot* s : Pending(id)) {
        if (g_now - s->time > Timeout()) {
            if (have && health <= 0.0f) {
                ResolveDead(*s); // chegou com o alvo morto
            } else {
                Resolve(*s, Round(s->gta), s->kind, true, 0, false, o);
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
        // O seu GTA aplica o dano que ele mesmo calculou: cada atacante tem o evento dele e a queda divide pelo GTA.
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
                AddToLine(g_taken, pend[i]->issuer, std::max(0, v), hud::HEALTH, false, pend[i]->part == 9,
                          std::min(1.0f, std::max(0.0f, me.health / 100.0f)), o);
            }
        }
        g_baseHealth = me.health;
        g_baseArmour = me.armour;
    }
    for (Taken& tk : g_takenShots) {
        if (!tk.resolved && g_now - tk.time > Timeout()) {
            tk.resolved = true;
            Note("dano levado: id %d, GTA %.1f -> a vida nao caiu a tempo, vale o do GTA (~%d)", tk.issuer, tk.gta,
                 Round(tk.gta));
            if (o.source == 0) {
                AddToLine(g_taken, tk.issuer, Round(tk.gta), hud::HEALTH, true, tk.part == 9,
                          std::min(1.0f, std::max(0.0f, me.health / 100.0f)), o);
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

void Init() {
    if (!g_ready) {
        g_ready = true;
        std::fill(g_sampleCount, g_sampleCount + 64, 0);
    }
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
    g_watch.clear();
    for (Target& t : g_targets) {
        t = Target();
    }
    std::fill(g_sampleCount, g_sampleCount + 64, 0); // outro servidor, outras armas
    std::fill(g_headKills, g_headKills + 64, 0);
    std::fill(g_headLives, g_headLives + 64, 0);
    g_latency = 0.3f;
    g_otherLatency = 0.2f;
    g_deathMessages = false;
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
    Target& t = g_targets[id];
    if (have && g_now >= t.watchUntil) { // nao vigiado: a base e o que o sync mostra agora
        t.health = health;
        t.armour = armour;
        t.based = true;
    }
    Watch(id);
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
        Resolve(added, Round(damage), added.kind, o.source == 0, 0, false, o);
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
            float health = g_localHealth - std::max(0.0f, damage - g_localArmour); // "Do GTA": antes da queda
            AddToLine(g_taken, tk.issuer, Round(damage), hud::HEALTH, false, part == 9,
                      std::min(1.0f, std::max(0.0f, health / 100.0f)), o);
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
    g_deathMessages = true;
    const opt::Options& o = opt::Get();
    if (!o.enabled || !victim || !victim[0]) {
        return;
    }
    std::string me = samp::LocalNameRaw();
    if (me.empty() || me == victim) {
        return;
    }
    int id = samp::FindByName(victim);
    if (!ValidId(id)) {
        return;
    }
    Target& t = g_targets[id];
    bool mine = killer && me == killer;
    if (t.zeroAt >= 0) {
        DecideDeath(id, mine ? 1 : 0, o); // a vida ja tinha zerado: decide agora
    } else if (mine) {
        ConfirmKill(id, "mensagem de morte", o);
    } else {
        t.otherKillAt = g_now; // o servidor deu a morte para outra pessoa (ou ninguem)
    }
}

void OnOtherHit(int target, int weapon) {
    Init();
    if (!ValidId(target) || !opt::Get().enabled) {
        return;
    }
    Target& t = g_targets[target];
    if (g_now >= t.watchUntil) {
        return; // so interessa quem voce esta acertando
    }
    Watch(target);
    Other other;
    other.time = g_now;
    other.expected = std::max(0.1f, BaseDamage(weapon) * Ratio(weapon));
    other.weapon = weapon;
    t.others.push_back(other);
    if (t.others.size() > 16) {
        t.others.erase(t.others.begin());
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

    // os alvos: a vida de cada um, separada
    for (size_t i = 0; i < g_watch.size(); ++i) {
        Track(g_watch[i], o);
    }
    RemoveIf(g_watch, [](int id) {
        const Target& t = g_targets[id];
        bool pending = std::any_of(g_shots.begin(), g_shots.end(),
                                   [id](const Shot& s) { return s.target == id && !s.resolved; });
        return g_now > t.watchUntil && !pending && t.zeroAt < 0;
    });

    // o tempo passa
    for (LNumber& ln : g_numbers) {
        ln.n.age += dt;
        ln.n.bump += dt;
    }
    RemoveIf(g_numbers, [](const LNumber& ln) { return ln.n.age > ln.n.stay + 1.6f; });
    float lineKeep = std::max(3.5f, static_cast<float>(o.comboWindow) + 0.6f);
    for (std::vector<hud::Line>* lines : {&g_given, &g_taken}) {
        for (hud::Line& l : *lines) {
            l.age += dt;
            l.bump += dt;
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
        if (o.numbers == 0 && ln.target >= 0) {
            DWORD ped = samp::PedOf(ln.target);
            ImVec2 at;
            if (ped && game::PartOnScreen(ped, ln.part, &at)) {
                ln.n.from = ImVec2(at.x + 44 * ns, at.y - 6 * ns); // ao lado do ponto do tiro: nao cobre a mira
            }
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
