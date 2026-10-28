#pragma once

// Trok KeyBinder -- as binds, os presets e o arquivo (Trok KeyBinder.txt, UTF-8). Sem nada do jogo aqui: o mesmo
// codigo roda no teste do Linux.
//
// Cada servidor ("ip:porta") aponta para um preset. O preset novo nasce com o nome do servidor; varios servidores
// podem apontar para o mesmo preset (Horizonte 1, 2, 3 e 4 com o mesmo) e o preset pode ser renomeado.

#include <string>
#include <vector>

namespace kb {

enum Class { CLASS_KEY = 0, CLASS_AUTO, CLASS_AROUND, CLASS_VIEW, CLASS_COUNT };
enum Mode { MODE_PRESS = 0, MODE_HOLD, MODE_DOUBLE, MODE_COUNT };
enum Mods { HK_CTRL = 1, HK_SHIFT = 2, HK_ALT = 4 }; // os mesmos do tui::Hotkey*

enum Cond {
    C_ENTER_VEHICLE = 0,
    C_EXIT_VEHICLE,
    C_HEALTH,
    C_ARMOUR,
    C_VEHICLE_HEALTH,
    C_SPAWN,
    C_DEATH,
    C_WEAPON,
    C_ENTER_INTERIOR,
    C_EXIT_INTERIOR,
    C_CONNECT,
    C_CHAT,
    C_COUNT
};
enum Seat { SEAT_ANY = 0, SEAT_DRIVER, SEAT_PASSENGER };
enum Target { TARGET_NEAREST = 0, TARGET_ALL };

constexpr int WEAPON_ANY = -1;

struct Hotkey {
    int key = 0; // ImGuiKey (o mod e compilado sempre com o mesmo ImGui 1.89.9)
    int mods = 0;
    bool Empty() const { return key == 0; }
    bool operator==(const Hotkey& o) const { return key == o.key && mods == o.mods; }
};

struct Bind {
    int cls = CLASS_KEY;
    bool enabled = true;
    std::string name;
    std::string text; // uma mensagem por linha (comando ou frase)
    // Tecla (Padrao, Ao redor e Na visao)
    Hotkey key;
    int mode = MODE_PRESS;
    bool inMenu = false; // aparece no menu circular
    // Automatico
    int cond = C_ENTER_VEHICLE;
    int value = 50;        // vida, colete, vida do veiculo ou arma (WEAPON_ANY = qualquer)
    int seat = SEAT_ANY;   // entrar no veiculo
    int delay = 0;         // ms depois da condicao
    std::string trigger;   // mensagem no chat: o trecho que dispara
    int cooldown = 5000;   // mensagem no chat: espera minima entre disparos (ms)
    // Ao redor
    int radius = 5;        // m
    int target = TARGET_NEAREST;
    std::vector<std::string> ignore; // nicks
};

struct Preset {
    std::string name;
    std::vector<Bind> binds;
};

struct ServerLink {
    std::string address;  // "ip:porta"
    std::string hostname; // o nome que o servidor mostra (UTF-8)
    std::string preset;   // nome do preset
};

struct Settings {
    bool enabled = true;
    Hotkey radial;           // abre o menu circular
    int flood = 1200;        // ms entre duas mensagens mandadas pelo mod
    int hold = 2000;         // ms segurando para "Segurar"
    int doubleTap = 350;     // ms entre os dois toques de "Duas vezes"
    int viewDistance = 25;   // m (Na visao)
    bool viewDot = true;     // a bolinha no alvo
    bool viewList = true;    // com alguem na mira, o atalho abre a lista presa nele (false = o circulo)
    int radialSize = 100;    // %
};

struct Data {
    Settings settings;
    std::vector<Preset> presets;
    std::vector<ServerLink> servers;
    std::string lastPreset; // o preset editado fora de um servidor
};

// ---------------------------------------------------------------- arquivo

std::string Serialize(const Data& d);
bool Parse(const std::string& text, Data* d);
bool LoadFile(const char* path, Data* d);
bool SaveFile(const char* path, const Data& d); // grava num .tmp e troca (o antigo vira .bak)

// ---------------------------------------------------------------- presets

int FindPreset(const Data& d, const std::string& name);
int FindServer(const Data& d, const std::string& address);
// O preset do servidor; sem ligacao ainda, cria um preset com o nome do servidor (ou usa o que ja tem esse nome).
int LinkServer(Data& d, const std::string& address, const std::string& hostname);
// Liga o servidor a outro preset. Um preset vazio que ficar sem servidor some.
void UsePreset(Data& d, const std::string& address, int preset);
// false se o nome estiver vazio ou ja for de outro preset. Os servidores ligados seguem o novo nome.
bool RenamePreset(Data& d, int preset, const std::string& name);
int NewPreset(Data& d, const std::string& base, bool copyFrom, int source);
void DeletePreset(Data& d, int preset); // os servidores ligados voltam a ter um preset proprio na proxima entrada
std::string UniqueName(const Data& d, const std::string& base, int except = -1);
// "Horizonte Roleplay | Servidor 1 | discord.gg/..." -> sem cores {RRGGBB}, espacos aparados, ate 40 bytes.
std::string AutoName(const std::string& hostname);
std::vector<std::string> ServersOf(const Data& d, const std::string& preset); // hostnames (ou enderecos)

// ---------------------------------------------------------------- binds

// Automaticas ligadas na mesma condicao (e, na do chat, no mesmo trecho) com o mesmo tempo: nao podem ser salvas.
// self = o indice da propria bind no preset (-1 = nova).
std::vector<int> Conflicts(const std::vector<Bind>& binds, const Bind& b, int self);
// Binds ligadas (de qualquer aba) que disparam no mesmo gesto: o mesmo atalho com o mesmo modo, ou "Pressionar"
// numa delas (o primeiro toque de um "Segurar" ou de um "2x" ja e um "Pressionar"). Segurar e 2x na mesma tecla
// convivem. Tambem nao podem ser salvas.
bool KeysOverlap(const Bind& a, const Bind& b);
std::vector<int> KeyConflicts(const std::vector<Bind>& binds, const Bind& b, int self);
// A tecla da bind e o atalho do menu circular?
bool RadialConflict(const Settings& s, const Bind& b);
// Binds ligadas com a tecla do atalho do menu circular.
std::vector<int> RadialConflicts(const std::vector<Bind>& binds, const Hotkey& radial);

std::vector<std::string> Lines(const std::string& text); // linhas nao vazias
// Troca (id)/{id} e (nick)/{nick} pelo jogador.
std::string Fill(const std::string& line, int id, const std::string& nick);
bool UsesTarget(const std::string& text); // tem (id) ou (nick)
int Codepoints(const std::string& utf8);  // ~ bytes no SA-MP (128 no maximo por mensagem)

const char* ClassName(int cls);
const char* CondName(int cond);
bool CondHasValue(int cond);
const char* WeaponName(int id); // nullptr = sem arma com esse id
// Texto curto da condicao: "Entrar num veiculo (motorista) - 1,0 s".
std::string Describe(const Bind& b);

// ---------------------------------------------------------------- mensagem no chat

// Texto para comparar: sem cores {RRGGBB}, minusculo e sem acento ("Esta" com acento -> "esta").
std::string Normalize(const std::string& utf8);
bool ChatMatches(const std::string& trigger, const std::string& message);

struct Player {
    int id = -1;
    std::string nick;
    float distance = 0; // m (0 = nao se sabe)
};

// Quem pediu: o autor (fala de jogador), um nick conectado que aparece na mensagem ou um ID "(12)", "[12]", "ID 12".
// players = os conectados (sem o jogador local). false = ninguem achado.
bool FindPlayer(const std::string& message, const std::string& author, const std::vector<Player>& players, Player* out);

bool SameNick(const std::string& a, const std::string& b);
bool Ignored(const Bind& b, const std::string& nick);

} // namespace kb
