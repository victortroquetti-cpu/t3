// Trok KeyBinder -- teste do Linux: arquivo, presets por servidor, conflitos, mensagem no chat e o motor (teclas,
// automaticas, fila com anti-flood), sem o jogo.
#include "binds.h"
#include "engine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace kb;

static int failures = 0;

static void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHOU", what);
    if (!ok) {
        ++failures;
    }
}

struct FakeKeys : Keys {
    std::set<int> down;
    int mods = 0;
    bool Down(int key) const override { return down.count(key) != 0; }
    bool ModsOk(const Hotkey& h) const override { return h.mods == mods; }
};

static std::vector<std::string> Drain(Engine& e, unsigned& now, int flood, unsigned until) {
    std::vector<std::string> out;
    for (; now <= until; now += 50) {
        std::string m;
        if (e.Next(now, flood, &m)) {
            out.push_back(m);
        }
    }
    return out;
}

static World Base(unsigned now) {
    World w;
    w.now = now;
    w.sendReady = true;
    w.me.connected = true;
    w.me.spawned = true;
    w.me.health = 100;
    w.me.armour = 0;
    return w;
}

static void TestFile() {
    Data d;
    d.settings.radial = {546, HK_CTRL};
    Preset p;
    p.name = "hzrp policia";
    p.flood = 1500;
    Bind a;
    a.cls = CLASS_KEY;
    a.name = "Sauda\xC3\xA7\xC3\xA3o";
    a.text = "oi galera, tudo bem?\n/me acena";
    a.key = {560, HK_CTRL | HK_SHIFT};
    a.mode = MODE_DOUBLE;
    a.inMenu = true;
    a.icon = "hand";
    p.binds.push_back(a);
    Bind r;
    r.cls = CLASS_AROUND;
    r.text = "/algemar (id)";
    r.radius = 8;
    r.target = TARGET_ALL;
    r.ignore = {"Lucas_Mendes", "Catharina"};
    p.binds.push_back(r);
    Bind c;
    c.cls = CLASS_AUTO;
    c.cond = C_CHAT;
    c.trigger = "est\xC3\xA1 precisando de atendimento";
    c.text = "/atender (id)";
    c.delay = 1500;
    p.binds.push_back(c);
    d.presets.push_back(p);
    d.servers.push_back({"10.0.0.1:7777", "Horizonte RP | Servidor 1", "hzrp policia"});
    d.lastPreset = "hzrp policia";

    std::string text = Serialize(d);
    Data back;
    Check(Parse(text, &back), "arquivo lido de volta");
    Check(Serialize(back) == text, "gravar -> ler -> gravar da o mesmo arquivo");
    Check(back.presets.size() == 1 && back.presets[0].binds.size() == 3, "preset com as 3 binds");
    const Bind& b0 = back.presets[0].binds[0];
    Check(b0.text == a.text && b0.name == a.name && b0.key == a.key && b0.mode == MODE_DOUBLE && b0.inMenu &&
              b0.icon == "hand",
          "bind de tecla: texto com 2 linhas, nome com acento, Ctrl+Shift, duas vezes");
    Check(back.presets[0].binds[1].ignore.size() == 2 && back.presets[0].binds[1].ignore[1] == "Catharina",
          "lista de ignorados");
    Check(back.presets[0].binds[2].trigger == c.trigger && back.presets[0].binds[2].delay == 1500, "gatilho do chat");
    Check(back.settings.radial == d.settings.radial && back.presets[0].flood == 1500, "geral e o anti-flood do preset");
    Check(back.servers.size() == 1 && back.servers[0].preset == "hzrp policia", "servidor ligado ao preset");

    Data junk;
    Check(!Parse("lixo sem secao\n", &junk) && junk.presets.empty(), "arquivo sem secoes nao vira nada");
    Data dup;
    Parse("[preset]\nnome=A\n[preset]\nnome=a\n[preset]\n", &dup);
    Check(dup.presets.size() == 3 && dup.presets[1].name == "a (2)" && dup.presets[2].name == "Preset",
          "presets repetidos ou sem nome ganham nome unico");
}

static void TestPresets() {
    Data d;
    int p1 = LinkServer(d, "1.1.1.1:7777", "{FFFFFF}Horizonte RP | Servidor 1");
    Check(p1 == 0 && d.presets[0].name == "Horizonte RP | Servidor 1", "servidor novo cria o preset com o nome dele");
    Check(LinkServer(d, "1.1.1.1:7777", "") == 0 && d.presets.size() == 1, "o mesmo servidor reusa o preset");
    int p2 = LinkServer(d, "1.1.1.2:7777", "Horizonte RP | Servidor 2");
    Check(p2 == 1 && d.presets.size() == 2, "outro servidor, outro preset");
    // Servidor 2 passa a usar o preset do 1: o preset vazio do 2 some.
    UsePreset(d, "1.1.1.2:7777", 0);
    Check(d.presets.size() == 1 && LinkServer(d, "1.1.1.2:7777", "") == 0, "compartilhar: o 2 usa o preset do 1");
    Check(ServersOf(d, d.presets[0].name).size() == 2, "o preset mostra os 2 servidores");
    Check(RenamePreset(d, 0, "hzrp ladrao") && d.servers[0].preset == "hzrp ladrao" && d.servers[1].preset == "hzrp ladrao",
          "renomear leva os servidores junto");
    int pol = NewPreset(d, "hzrp policia", false, -1);
    Check(!RenamePreset(d, pol, "HZRP LADRAO") && !RenamePreset(d, pol, "  "), "nome repetido ou vazio recusado");
    Bind b;
    b.text = "/cinto";
    d.presets[pol].binds.push_back(b);
    UsePreset(d, "1.1.1.2:7777", pol);
    Check(d.presets.size() == 2 && LinkServer(d, "1.1.1.2:7777", "") == pol, "o 2 vira policia, o 1 continua ladrao");
    DeletePreset(d, pol);
    Check(d.presets.size() == 1 && FindServer(d, "1.1.1.2:7777") < 0, "apagar o preset solta o servidor");
    Check(LinkServer(d, "1.1.1.2:7777", "Horizonte RP | Servidor 2") == 1 && d.presets[1].name == "Horizonte RP | Servidor 2",
          "na volta o servidor ganha preset proprio de novo");
    Check(AutoName("  Um servidor com um nome bem comprido demais para caber, sim  ").size() <= 40 &&
              AutoName("{FF0000}").compare("Servidor") == 0,
          "nome automatico cortado e sem cores");
    std::string acc = AutoName("Servidor Paulist\xC3\xA3o \xC3\xA9 o melhor de S\xC3\xA3o Paulo ok");
    Check((static_cast<unsigned char>(acc.back()) & 0xC0) != 0x80 || acc.size() < 40, "corte nao parte um acento");
}

static void TestConflicts() {
    std::vector<Bind> binds;
    Bind tranca;
    tranca.cls = CLASS_AUTO;
    tranca.cond = C_ENTER_VEHICLE;
    tranca.text = "/tranca";
    binds.push_back(tranca);
    Bind cinto = tranca;
    cinto.text = "/cinto";
    Check(Conflicts(binds, cinto, -1).size() == 1, "/cinto com o mesmo tempo do /tranca: conflito");
    cinto.delay = 1000;
    Check(Conflicts(binds, cinto, -1).empty(), "/cinto 1 s depois: ok");
    Bind driver = tranca;
    driver.seat = SEAT_DRIVER;
    Bind passenger = tranca;
    passenger.seat = SEAT_PASSENGER;
    std::vector<Bind> seats = {driver};
    Check(Conflicts(seats, passenger, -1).empty() && Conflicts(binds, driver, -1).size() == 1,
          "motorista x passageiro nao conflitam; qualquer x motorista sim");
    Bind off = tranca;
    off.enabled = false;
    Check(Conflicts(binds, off, -1).empty(), "bind desligada nao conflita");
    Check(Conflicts(binds, tranca, 0).empty(), "a propria bind nao conta");
    Bind c1, c2;
    c1.cls = c2.cls = CLASS_AUTO;
    c1.cond = c2.cond = C_CHAT;
    c1.trigger = "precisando de atendimento";
    c2.trigger = "Est\xC3\xA1 PRECISANDO de atendimento";
    std::vector<Bind> chats = {c1};
    Check(Conflicts(chats, c2, -1).size() == 1, "trechos do chat que se cobrem conflitam");
    c2.trigger = "pediu reforco";
    Check(Conflicts(chats, c2, -1).empty(), "trechos diferentes nao conflitam");
    Bind k1, k2;
    k1.key = k2.key = {546, 0};
    k2.cls = CLASS_AROUND; // outra aba
    std::vector<Bind> keys = {k1};
    Check(KeyConflicts(keys, k2, -1).size() == 1, "mesma tecla e modo em abas diferentes: conflito");
    k2.mode = MODE_HOLD;
    Check(KeyConflicts(keys, k2, -1).size() == 1, "pressionar x segurar na mesma tecla: conflito");
    keys[0].mode = MODE_DOUBLE;
    Check(KeyConflicts(keys, k2, -1).empty(), "segurar x 2x na mesma tecla: convivem");
    k2.key.mods = HK_CTRL;
    k2.mode = MODE_DOUBLE;
    Check(KeyConflicts(keys, k2, -1).empty(), "Ctrl+tecla x tecla: nao conflitam");
    Bind auto1;
    auto1.cls = CLASS_AUTO;
    auto1.key = {546, 0};
    Check(KeyConflicts(keys, auto1, -1).empty(), "automatica nao tem tecla");
    Settings st;
    st.radial = {546, 0};
    Check(RadialConflict(st, keys[0]) && RadialConflicts(keys, st.radial).size() == 1, "tecla do menu circular: conflito");
}

static void TestText() {
    Check(Fill("/atender (id) {nick} (ID) (Nick)", 12, "Fulano_Silva") == "/atender 12 Fulano_Silva 12 Fulano_Silva",
          "(id) e (nick) trocados");
    Check(UsesTarget("/pm (id) oi") && UsesTarget("/x {nick}") && !UsesTarget("/tranca (motor)"), "texto usa o alvo");
    Check(Lines("a\r\n\n  \nb\n").size() == 2, "linhas vazias ignoradas");
    Check(Normalize("{FF0000}[ATENDIMENTO]  Fulano Est\xC3\xA1 Precisando") == "[atendimento] fulano esta precisando",
          "normalizar: sem cor, minusculo, sem acento");
    Check(ChatMatches("est\xC3\xA1 precisando de atendimento",
                      "{33AA33}[Atendimento]{FFFFFF} Fulano_Silva (12) esta precisando de ATENDIMENTO."),
          "gatilho do chat com e sem acento");
    Check(!ChatMatches("", "qualquer coisa"), "gatilho vazio nunca dispara");
    Check(ChatMatches("est\xC3\xA1 precisando de atendimento",
                      "Fulano est{FF0000}\xC3\xA1 precis{00ff00}ando de aten{FFFFFFAA}dimento#00FF00!"),
          "cores no meio das palavras ({RRGGBB}, {RRGGBBAA}, #RRGGBB) nao atrapalham");
    Check(Normalize("oi{FF0000}gale{00FF00}ra #12") == "oigalera #12", "cor some; #12 (ID) fica");

    std::vector<Player> ps = {{12, "Fulano_Silva", 0}, {7, "Ana", 0}, {33, "Ana_Clara", 0}, {5, "Bob_Bob", 0}};
    Player p;
    Check(FindPlayer("[Atendimento] Fulano_Silva esta precisando de atendimento", ps, &p) && p.id == 12,
          "nick na mensagem");
    Check(FindPlayer("[Atendimento] Fulano Silva esta precisando", ps, &p) && p.id == 12, "nick com espaco");
    Check(FindPlayer("Ana_Clara pediu ajuda", ps, &p) && p.id == 33, "o nick mais comprido no mesmo lugar");
    Check(FindPlayer("Banana pediu ajuda (ID 5)", ps, &p) && p.id == 5, "Ana dentro de Banana nao conta; ID 5");
    Check(FindPlayer("Alguem [7] precisa", ps, &p) && p.id == 7, "ID entre colchetes");
    Check(FindPlayer("Novo pedido de #33", ps, &p) && p.id == 33, "ID com #");
    Check(!FindPlayer("ID 99 pediu", ps, &p) && !FindPlayer("ninguem (123) aqui", ps, &p),
          "ID de ninguem conectado nao conta");
}

static void TestEngineKeys() {
    Engine e;
    Settings s;
    Preset p;
    Bind press;
    press.text = "oi galera, tudo bem?";
    press.key = {600, 0};
    Bind combo;
    combo.text = "/lanterna";
    combo.key = {601, HK_CTRL};
    Bind hold;
    hold.text = "/reparar";
    hold.key = {602, 0};
    hold.mode = MODE_HOLD;
    Bind twice;
    twice.text = "/motor";
    twice.key = {603, 0};
    twice.mode = MODE_DOUBLE;
    p.binds = {press, combo, hold, twice};
    FakeKeys k;
    unsigned now = 1000;
    auto frame = [&](bool blocked = false) {
        World w = Base(now);
        w.blocked = blocked;
        e.Frame(p, s, w, &k);
        now += 16;
    };
    frame();
    k.down = {600};
    frame();
    frame();
    k.down.clear();
    frame();
    Check(e.Queued() == 1, "pressionar: 1 vez por aperto");
    k.down = {601};
    frame();
    k.down.clear();
    frame();
    Check(e.Queued() == 1, "Ctrl+L sem o Ctrl nao dispara");
    k.mods = HK_CTRL;
    k.down = {601};
    frame();
    k.down.clear();
    k.mods = 0;
    frame();
    Check(e.Queued() == 2, "Ctrl+L dispara");
    k.down = {602};
    for (int i = 0; i < 60; ++i) frame(); // ~1 s
    Check(e.Queued() == 2, "segurar: nada antes de 2 s");
    for (int i = 0; i < 70; ++i) frame();
    Check(e.Queued() == 3, "segurar: dispara aos 2 s, uma vez");
    k.down.clear();
    frame();
    k.down = {603};
    frame();
    k.down.clear();
    for (int i = 0; i < 30; ++i) frame(); // ~0,5 s
    k.down = {603};
    frame();
    k.down.clear();
    frame();
    Check(e.Queued() == 3, "duas vezes: toques longe demais nao disparam");
    k.down = {603};
    frame();
    k.down.clear();
    frame();
    k.down = {603};
    frame();
    k.down.clear();
    frame();
    Check(e.Queued() == 4, "duas vezes: dois toques rapidos disparam");
    // Chat aberto: a tecla segura nao dispara nem ao fechar.
    k.down = {600};
    frame(true);
    frame(true);
    frame(false);
    k.down.clear();
    frame();
    Check(e.Queued() == 4, "com o chat aberto nada dispara (nem quando fecha com a tecla segura)");
    s.enabled = false;
    k.down = {600};
    frame();
    k.down.clear();
    frame();
    s.enabled = true;
    Check(e.Queued() == 4, "mod desligado: nada dispara");

    std::vector<std::string> sent = Drain(e, now, 1200, now + 5000);
    Check(sent.size() == 4 && sent[0] == "oi galera, tudo bem?" && sent[3] == "/motor", "fila na ordem");
    Engine f;
    Preset multi;
    Bind m;
    m.text = "/a\n/b\n/c";
    m.key = {600, 0};
    multi.binds = {m};
    FakeKeys k2;
    k2.down = {600};
    World w = Base(0);
    f.Frame(multi, s, w, &k2);
    unsigned t = 0;
    std::string out;
    int got = 0;
    unsigned first = 0, last = 0;
    for (; t < 5000; t += 10) {
        if (f.Next(t, 1200, &out)) {
            if (!got) first = t;
            last = t;
            ++got;
        }
    }
    Check(got == 3 && last - first == 2400, "anti-flood: 3 linhas, 1,2 s entre cada");
}

static void TestEngineAuto() {
    Engine e;
    Settings s;
    Preset p;
    Bind tranca;
    tranca.cls = CLASS_AUTO;
    tranca.cond = C_ENTER_VEHICLE;
    tranca.text = "/tranca";
    Bind cinto = tranca;
    cinto.text = "/cinto";
    cinto.delay = 1000;
    Bind life;
    life.cls = CLASS_AUTO;
    life.cond = C_HEALTH;
    life.value = 50;
    life.text = "/me sente dor";
    Bind gun;
    gun.cls = CLASS_AUTO;
    gun.cond = C_WEAPON;
    gun.value = 24;
    gun.text = "/me saca a Desert Eagle";
    gun.delay = 200;
    p.binds = {tranca, cinto, life, gun};
    unsigned now = 0;
    World w = Base(now);
    std::vector<std::string> sent;
    auto step = [&](int frames) {
        for (int i = 0; i < frames; ++i) {
            w.now = now;
            e.Frame(p, s, w, nullptr);
            std::string m;
            if (e.Next(now, 1200, &m)) sent.push_back(m);
            now += 16;
        }
    };
    w.me.inVehicle = true; // ja comeca no carro: nao e "entrar"
    step(5);
    Check(sent.empty(), "comecar dentro do carro nao dispara");
    w.me.inVehicle = false;
    step(5);
    w.me.inVehicle = true;
    w.me.driver = true;
    step(150);
    Check(sent.size() == 2 && sent[0] == "/tranca" && sent[1] == "/cinto", "entrar: /tranca e /cinto 1 s depois");
    w.me.inVehicle = false;
    step(5);
    w.me.inVehicle = true;
    step(5);
    w.me.inVehicle = false; // saiu antes do /cinto
    step(150);
    Check(sent.size() == 3 && sent[2] == "/tranca", "saiu antes do tempo: o /cinto nao vai");
    w.me.health = 60;
    step(2);
    w.me.health = 40;
    step(2);
    w.me.health = 30;
    step(100);
    Check(sent.size() == 4 && sent[3] == "/me sente dor", "vida abaixo de 50: uma vez na descida");
    w.me.health = 100;
    step(2);
    w.me.health = 0;
    w.me.dead = true;
    w.me.spawned = false;
    step(100);
    Check(sent.size() == 4, "morrer nao conta como vida abaixo");
    w.me.dead = false;
    w.me.spawned = true;
    w.me.health = 100;
    w.me.weapon = 24; // nasceu com a arma: nao e sacar
    step(100);
    Check(sent.size() == 4, "nascer com a arma nao dispara");
    w.me.weapon = 0;
    step(2);
    w.me.weapon = 24;
    w.blocked = true; // dialogo aberto na hora
    step(60);
    Check(sent.size() == 4, "com um dialogo aberto a automatica espera");
    w.blocked = false;
    step(100);
    Check(sent.size() == 5 && sent[4] == "/me saca a Desert Eagle", "fechou o dialogo: vai");

    // Mensagem no chat
    Engine c;
    Preset adm;
    Bind atender;
    atender.cls = CLASS_AUTO;
    atender.cond = C_CHAT;
    atender.trigger = "est\xC3\xA1 precisando de atendimento";
    atender.text = "/atender (id)";
    atender.cooldown = 5000;
    adm.binds = {atender};
    World cw = Base(1000);
    cw.players = {{12, "Fulano_Silva", 0}, {40, "Beltrano", 0}};
    cw.chat = {{"{FF9900}[SOS] Fulano_Silva (12) est\xC3\xA1 precisando de atendimento"}};
    c.Frame(adm, s, cw, nullptr);
    cw.chat = {{"[SOS] Beltrano (40) esta precisando de atendimento"}};
    cw.now = 2000;
    c.Frame(adm, s, cw, nullptr);
    cw.chat.clear();
    std::vector<std::string> got;
    for (unsigned t = 2000; t < 9000; t += 50) {
        cw.now = t;
        c.Frame(adm, s, cw, nullptr);
        std::string m;
        if (c.Next(t, 1200, &m)) got.push_back(m);
    }
    Check(got.size() == 1 && got[0] == "/atender 12", "chat: /atender 12; o segundo pedido cai na espera de 5 s");
    cw.chat = {{"[SOS] Beltrano (40) esta precisando de atendimento"}};
    cw.now = 9000;
    c.Frame(adm, s, cw, nullptr);
    cw.chat = {{"[SOS] alguem esta precisando de atendimento"}};
    cw.now = 15000;
    c.Frame(adm, s, cw, nullptr);
    std::string m;
    got.clear();
    while (c.Next(cw.now += 2000, 1200, &m)) got.push_back(m);
    Check(got.size() == 1 && got[0] == "/atender 40", "chat: depois da espera vai o 40; sem jogador nao vai");
    std::vector<Notice> notes = c.TakeNotices();
    Check(notes.size() == 1, "aviso quando o jogador do pedido nao e achado");
}

static void TestEngineTargets() {
    Engine e;
    World w = Base(0);
    w.players = {{3, "Perto", 2.0f}, {4, "Medio", 4.5f}, {5, "Longe", 9.0f}, {6, "Fora", 0}};
    Bind r;
    r.cls = CLASS_AROUND;
    r.text = "/revistar (id)";
    r.radius = 5;
    Check(e.Run(r, w) && e.Queued() == 1, "ao redor: o mais perto");
    r.target = TARGET_ALL;
    r.ignore = {"PERTO"};
    e.Run(r, w);
    std::string m;
    std::vector<std::string> sent;
    unsigned t = 0;
    while (sent.size() < 5 && t < 20000) {
        if (e.Next(t, 1000, &m)) sent.push_back(m);
        t += 100;
    }
    Check(sent.size() == 2 && sent[0] == "/revistar 3" && sent[1] == "/revistar 4",
          "ao redor: todos no raio, sem o ignorado (sem diferenciar maiusculas)");
    r.radius = 1;
    Check(!e.Run(r, w), "ninguem no raio: nao roda");
    Bind v;
    v.cls = CLASS_VIEW;
    v.text = "/pm (id) (nick), encosta o carro";
    Check(!e.Run(v, w), "na visao sem alvo: nao roda");
    w.hasTarget = true;
    w.target = {9, "Alvo_Teste", 8};
    Check(e.Run(v, w) && e.Next(t + 5000, 1000, &m) && m == "/pm 9 Alvo_Teste, encosta o carro", "na visao: o alvo");
    w.sendReady = false;
    Check(!e.Run(v, w), "sem o chat pronto: nao roda");
    Check(e.TakeNotices().size() == 3, "um aviso por bind que nao rodou");
}

static void TestV2Model() {
    // Campos novos no arquivo, e o intervalo do [geral] da v0.1 vale para os presets sem o deles.
    Data old;
    Parse("[geral]\nintervalo=2000\n[preset]\nnome=A\n[preset]\nnome=B\nintervalo=900\n", &old);
    Check(old.presets.size() == 2 && old.presets[0].flood == 2000 && old.presets[1].flood == 900,
          "anti-flood: o do [geral] antigo vale para o preset sem o dele");
    Preset p;
    p.name = "Teste";
    Bind b;
    b.text = "/x";
    b.group = "Pol\xC3\xAD" "cia";
    b.where = WHERE_VEHICLE;
    b.speechGuard = false;
    p.binds.push_back(b);
    Data back;
    Parse(SerializePreset(p), &back);
    Check(back.presets.size() == 1 && back.presets[0].binds[0].group == b.group && back.presets[0].binds[0].where == WHERE_VEHICLE &&
              !back.presets[0].binds[0].speechGuard,
          "grupo, onde vale e ignorar fala no arquivo do preset");

    // Fala repassada pelo servidor nao dispara.
    Check(LooksLikeSpeech("est\xC3\xA1 precisando de atendimento", "Fulano_Silva diz: esta precisando de atendimento") &&
              !LooksLikeSpeech("est\xC3\xA1 precisando de atendimento", "[SOS] Fulano_Silva (12) esta precisando de atendimento"),
          "\":\" antes do trecho = fala; o aviso do sistema passa");
    Engine e;
    Settings s;
    Preset adm;
    Bind atender;
    atender.cls = CLASS_AUTO;
    atender.cond = C_CHAT;
    atender.trigger = "precisando de atendimento";
    atender.text = "/atender (id)";
    adm.binds = {atender};
    World w = Base(1000);
    w.players = {{12, "Fulano_Silva", 0}};
    w.chat = {{"Fulano_Silva diz: estou precisando de atendimento"}};
    e.Frame(adm, s, w, nullptr);
    std::string m;
    Check(!e.Next(5000, 1200, &m) && e.Scheduled() == 0, "fala de jogador (com \":\") nao dispara");
    adm.binds[0].speechGuard = false;
    w.now = 9000;
    e.Frame(adm, s, w, nullptr);
    w.chat.clear();
    w.now = 9100;
    e.Frame(adm, s, w, nullptr);
    Check(e.Next(9100, 1200, &m) && m == "/atender 12", "com \"ignorar fala\" desligado, dispara");

    // So no veiculo / so a pe.
    Engine k;
    Preset car;
    Bind cinto;
    cinto.text = "/cinto";
    cinto.key = {600, 0};
    cinto.where = WHERE_VEHICLE;
    car.binds = {cinto};
    FakeKeys keys;
    World cw = Base(0);
    k.Frame(car, s, cw, &keys);
    keys.down = {600};
    cw.now = 20;
    k.Frame(car, s, cw, &keys);
    keys.down.clear();
    cw.now = 40;
    k.Frame(car, s, cw, &keys);
    Check(k.Queued() == 0, "\"so no veiculo\": a pe a tecla nao dispara");
    cw.me.inVehicle = true;
    keys.down = {600};
    cw.now = 60;
    k.Frame(car, s, cw, &keys);
    Check(k.Queued() == 1 && WhereOk(cinto, true) && !WhereOk(cinto, false), "\"so no veiculo\": no carro dispara");

    // Ordem no menu: a posicao entre as binds do mesmo grupo (as da visao a parte).
    std::vector<Bind> list(5);
    for (int i = 0; i < 5; ++i) {
        list[i].name = std::string(1, static_cast<char>('A' + i));
        list[i].inMenu = true;
    }
    list[2].cls = CLASS_VIEW;   // C fica na lista da visao
    list[3].group = "G";        // D no grupo G
    Check(MenuPosition(list, 4) == 3 && MenuPosition(list, 2) == 1 && MenuPosition(list, 3) == 1,
          "posicao no menu conta so o mesmo grupo e o mesmo menu");
    int at = MoveInMenu(list, 4, 1);
    Check(at == 0 && list[0].name == "E" && list[1].name == "A" && list[2].name == "B", "E vai para a primeira posicao");
    at = MoveInMenu(list, 0, 9);
    Check(list[at].name == "E" && MenuPosition(list, at) == 3, "posicao alem do fim vai para o fim");
    Check(Groups(list).size() == 1 && Groups(list)[0] == "G", "grupos do menu");
}

static void TestFolder() {
    std::string dir = "test/out/pasta";
    std::string presets = JoinPath(dir, "presets");
    if (system(("rm -rf '" + dir + "'").c_str())) {
    }
    Data d;
    LinkServer(d, "10.0.0.1:7777", "Horizonte RP | Servidor 1");
    d.presets[0].binds.push_back(Bind());
    int pol = NewPreset(d, "hzrp pol\xC3\xAD" "cia: \"nova\"?", false, -1);
    d.presets[pol].binds.push_back(Bind());
    Check(SaveFolder(dir, d), "pasta gravada");
    Check(FileExists(JoinPath(dir, "geral.txt")) && FileExists(JoinPath(presets, "Horizonte RP _ Servidor 1.txt")) &&
              FileExists(JoinPath(presets, "hzrp pol\xC3\xAD" "cia_ _nova__.txt")),
          "geral.txt e um arquivo por preset (sem os caracteres que o Windows recusa)");
    Data back;
    Check(LoadFolder(dir, &back) && back.presets.size() == 2 && back.servers.size() == 1 && back.knownFiles.size() == 2,
          "pasta lida de volta");
    RenamePreset(back, 1, "hzrp policia");
    SaveFolder(dir, back);
    Check(FileExists(JoinPath(presets, "hzrp policia.txt")) && !FileExists(JoinPath(presets, "hzrp pol\xC3\xAD" "cia_ _nova__.txt")),
          "renomear o preset renomeia o arquivo");
    DeletePreset(back, 1);
    SaveFolder(dir, back);
    Check(!FileExists(JoinPath(presets, "hzrp policia.txt")) && FileExists(JoinPath(JoinPath(presets, "apagados"), "hzrp policia.txt")),
          "apagar o preset move o arquivo para presets/apagados");
    // Um amigo manda o arquivo dele, com o mesmo nome de preset.
    std::string text;
    ReadText(JoinPath(presets, "Horizonte RP _ Servidor 1.txt"), &text);
    WriteText(JoinPath(presets, "do amigo.txt"), text);
    Check(MergeNewPresets(dir, back) == 1 && back.presets.size() == 2 && back.presets[1].name == "Horizonte RP | Servidor 1 (2)" &&
              MergeNewPresets(dir, back) == 0,
          "arquivo novo em presets entra sozinho, com nome unico (e uma vez so)");
    SaveFolder(dir, back);
    Check(FileExists(JoinPath(presets, "do amigo.txt")) == false && FileExists(JoinPath(presets, "Horizonte RP _ Servidor 1 (2).txt")),
          "o preset que veio de fora ganha o arquivo com o nome dele");
    // A v0.1 tinha um arquivo so.
    std::string old = "test/out/Trok KeyBinder.txt", dir2 = "test/out/pasta2";
    if (system(("rm -rf '" + dir2 + "' '" + old + ".antigo'").c_str())) {
    }
    WriteText(old, "[geral]\nintervalo=1500\n[preset]\nnome=velho\n[bind]\ntexto=/oi\n[servidor]\nendereco=1.2.3.4:7777\npreset=velho\n");
    Data migrated;
    Check(MigrateOldFile(old, dir2, &migrated) && FileExists(JoinPath(JoinPath(dir2, "presets"), "velho.txt")) &&
              FileExists(old + ".antigo") && !FileExists(old) && migrated.presets[0].flood == 1500,
          "o arquivo da v0.1 vira a pasta (e o antigo fica como .antigo)");
    Data again;
    Check(!MigrateOldFile(old + ".antigo", dir2, &again), "com a pasta ja feita, nao migra de novo");
}

int main() {
    TestFile();
    TestPresets();
    TestConflicts();
    TestText();
    TestEngineKeys();
    TestEngineAuto();
    TestEngineTargets();
    TestV2Model();
    TestFolder();
    printf("%s\n", failures ? "FALHOU" : "PASSOU");
    return failures ? 1 : 0;
}
