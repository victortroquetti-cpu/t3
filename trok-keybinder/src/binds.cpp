// Trok KeyBinder -- binds, presets e o arquivo. Ver binds.h.

#include "binds.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace kb {
namespace {

const char* const CLASS_KEYS[] = {"padrao", "auto", "redor", "visao"};

std::string Escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') {
            out += "\\\\";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c != '\r') {
            out += c;
        }
    }
    return out;
}

std::string Unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            out += s[i] == 'n' ? '\n' : s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        return std::string();
    }
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

char Lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool NickChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool IsHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Tira as cores do texto, em qualquer lugar (ate no meio de uma palavra: "aten{FF0000}dimento" -> "atendimento"):
// {RRGGBB} do SA-MP, {RRGGBBAA} e #RRGGBB.
std::string StripColors(const std::string& s) {
    auto hexRun = [&](size_t from, size_t n) {
        if (from + n > s.size()) {
            return false;
        }
        for (size_t k = 0; k < n; ++k) {
            if (!IsHex(s[from + k])) {
                return false;
            }
        }
        return true;
    };
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '{') {
            if (hexRun(i + 1, 6) && i + 7 < s.size() && s[i + 7] == '}') {
                i += 7;
                continue;
            }
            if (hexRun(i + 1, 8) && i + 9 < s.size() && s[i + 9] == '}') {
                i += 9;
                continue;
            }
        } else if (s[i] == '#' && hexRun(i + 1, 6)) {
            i += 6;
            continue;
        }
        out += s[i];
    }
    return out;
}

// Letra sem acento para o segundo byte de um "C3 xx" do UTF-8 (U+00C0..U+00FF). 0 = deixa como esta.
char Fold(unsigned char b) {
    static const char table[64 + 1] = "aaaaaaaceeeeiiiidnooooo\0ouuuuy\0saaaaaaaceeeeiiiidnooooo\0ouuuuy\0y";
    if (b < 0x80 || b > 0xBF) {
        return 0;
    }
    return table[b - 0x80];
}

bool LowerEquals(const std::string& a, size_t at, const std::string& b) {
    if (at + b.size() > a.size()) {
        return false;
    }
    for (size_t i = 0; i < b.size(); ++i) {
        if (Lower(a[at + i]) != Lower(b[i])) {
            return false;
        }
    }
    return true;
}

int Clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

// Pode disparar junto (a mesma condicao, no mesmo momento)?
bool SameMoment(const Bind& a, const Bind& b) {
    if (a.cond != b.cond) {
        return false;
    }
    switch (a.cond) {
    case C_ENTER_VEHICLE:
        return a.seat == SEAT_ANY || b.seat == SEAT_ANY || a.seat == b.seat;
    case C_WEAPON:
        return a.value == WEAPON_ANY || b.value == WEAPON_ANY || a.value == b.value;
    case C_CHAT: {
        std::string x = Normalize(a.trigger), y = Normalize(b.trigger);
        if (x.empty() || y.empty()) {
            return false;
        }
        return x.find(y) != std::string::npos || y.find(x) != std::string::npos;
    }
    default:
        return true; // vida, colete e lataria: um tiro forte cruza dois limites de uma vez
    }
}

} // namespace

// ---------------------------------------------------------------- arquivo

std::string Serialize(const Data& d) {
    std::string out = "# Trok KeyBinder -- binds e presets (UTF-8). Edite com o jogo fechado.\n\n[geral]\n";
    char line[512];
    const Settings& s = d.settings;
    snprintf(line, sizeof(line),
             "ligado=%d\nmenu_tecla=%d\nmenu_mods=%d\nintervalo=%d\nsegurar=%d\nduas_vezes=%d\nvisao_distancia=%d\n"
             "visao_bolinha=%d\nvisao_lista=%d\nmenu_tamanho=%d\n",
             s.enabled ? 1 : 0, s.radial.key, s.radial.mods, s.flood, s.hold, s.doubleTap, s.viewDistance,
             s.viewDot ? 1 : 0, s.viewList ? 1 : 0, s.radialSize);
    out += line;
    out += "ultimo_preset=" + Escape(d.lastPreset) + "\n";
    for (const Preset& p : d.presets) {
        out += "\n[preset]\nnome=" + Escape(p.name) + "\n";
        for (const Bind& b : p.binds) {
            out += "\n[bind]\nclasse=";
            out += CLASS_KEYS[Clamp(b.cls, 0, CLASS_COUNT - 1)];
            out += "\nnome=" + Escape(b.name) + "\ntexto=" + Escape(b.text) + "\nicone=" + Escape(b.icon) + "\n";
            snprintf(line, sizeof(line),
                     "ligada=%d\ntecla=%d\nmods=%d\nmodo=%d\nmenu=%d\ncondicao=%d\nvalor=%d\nassento=%d\natraso=%d\n"
                     "espera=%d\nraio=%d\nalvo=%d\n",
                     b.enabled ? 1 : 0, b.key.key, b.key.mods, b.mode, b.inMenu ? 1 : 0, b.cond, b.value, b.seat,
                     b.delay, b.cooldown, b.radius, b.target);
            out += line;
            out += "gatilho=" + Escape(b.trigger) + "\nignorar=";
            for (size_t i = 0; i < b.ignore.size(); ++i) {
                out += (i ? "," : "") + b.ignore[i];
            }
            out += "\n";
        }
    }
    for (const ServerLink& l : d.servers) {
        out += "\n[servidor]\nendereco=" + Escape(l.address) + "\nnome=" + Escape(l.hostname) + "\npreset=" +
               Escape(l.preset) + "\n";
    }
    return out;
}

bool Parse(const std::string& text, Data* d) {
    *d = Data();
    enum { NONE, GERAL, PRESET, BIND, SERVER } section = NONE;
    size_t pos = 0;
    bool any = false;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (pos == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) {
            line = line.substr(3); // BOM
        }
        std::string t = Trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') {
            if (end == text.size()) {
                break;
            }
            continue;
        }
        if (t[0] == '[') {
            any = true;
            if (t == "[geral]") {
                section = GERAL;
            } else if (t == "[preset]") {
                section = PRESET;
                d->presets.push_back(Preset());
            } else if (t == "[bind]") {
                section = d->presets.empty() ? NONE : BIND;
                if (section == BIND) {
                    d->presets.back().binds.push_back(Bind());
                }
            } else if (t == "[servidor]") {
                section = SERVER;
                d->servers.push_back(ServerLink());
            } else {
                section = NONE;
            }
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = Trim(line.substr(0, eq));
        std::string raw = line.substr(eq + 1);
        std::string val = Unescape(raw);
        int n = atoi(raw.c_str());
        if (section == GERAL) {
            Settings& s = d->settings;
            if (key == "ligado") s.enabled = n != 0;
            else if (key == "menu_tecla") s.radial.key = n;
            else if (key == "menu_mods") s.radial.mods = n & 7;
            else if (key == "intervalo") s.flood = Clamp(n, 500, 10000);
            else if (key == "segurar") s.hold = Clamp(n, 500, 5000);
            else if (key == "duas_vezes") s.doubleTap = Clamp(n, 150, 1000);
            else if (key == "visao_distancia") s.viewDistance = Clamp(n, 3, 100);
            else if (key == "visao_bolinha") s.viewDot = n != 0;
            else if (key == "visao_lista") s.viewList = n != 0;
            else if (key == "menu_tamanho") s.radialSize = Clamp(n, 70, 140);
            else if (key == "ultimo_preset") d->lastPreset = val;
        } else if (section == PRESET) {
            if (key == "nome") d->presets.back().name = val;
        } else if (section == BIND) {
            Bind& b = d->presets.back().binds.back();
            if (key == "classe") {
                for (int c = 0; c < CLASS_COUNT; ++c) {
                    if (Trim(raw) == CLASS_KEYS[c]) b.cls = c;
                }
            } else if (key == "ligada") b.enabled = n != 0;
            else if (key == "nome") b.name = val;
            else if (key == "texto") b.text = val;
            else if (key == "icone") b.icon = Trim(val);
            else if (key == "tecla") b.key.key = n;
            else if (key == "mods") b.key.mods = n & 7;
            else if (key == "modo") b.mode = Clamp(n, 0, MODE_COUNT - 1);
            else if (key == "menu") b.inMenu = n != 0;
            else if (key == "condicao") b.cond = Clamp(n, 0, C_COUNT - 1);
            else if (key == "valor") b.value = n;
            else if (key == "assento") b.seat = Clamp(n, 0, 2);
            else if (key == "atraso") b.delay = Clamp(n, 0, 600000);
            else if (key == "gatilho") b.trigger = val;
            else if (key == "espera") b.cooldown = Clamp(n, 0, 600000);
            else if (key == "raio") b.radius = Clamp(n, 1, 100);
            else if (key == "alvo") b.target = Clamp(n, 0, 1);
            else if (key == "ignorar") {
                b.ignore.clear();
                size_t a = 0;
                while (a <= raw.size()) {
                    size_t c = raw.find(',', a);
                    if (c == std::string::npos) c = raw.size();
                    std::string nick = Trim(raw.substr(a, c - a));
                    if (!nick.empty()) b.ignore.push_back(nick);
                    a = c + 1;
                }
            }
        } else if (section == SERVER) {
            ServerLink& l = d->servers.back();
            if (key == "endereco") l.address = Trim(val);
            else if (key == "nome") l.hostname = val;
            else if (key == "preset") l.preset = val;
        }
        if (end == text.size()) {
            break;
        }
    }
    // Presets sem nome ou repetidos ganham um nome unico; servidores sem endereco saem.
    for (size_t i = 0; i < d->presets.size(); ++i) {
        std::string base = d->presets[i].name.empty() ? std::string("Preset") : d->presets[i].name;
        // So contra os de cima: o primeiro com o nome fica com ele.
        Data before;
        before.presets.assign(d->presets.begin(), d->presets.begin() + i);
        d->presets[i].name = UniqueName(before, base);
    }
    d->servers.erase(std::remove_if(d->servers.begin(), d->servers.end(),
                                    [](const ServerLink& l) { return l.address.empty(); }),
                     d->servers.end());
    return any;
}

bool LoadFile(const char* path, Data* d) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        *d = Data();
        return false;
    }
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        text.append(buf, n);
    }
    fclose(f);
    return Parse(text, d);
}

bool SaveFile(const char* path, const Data& d) {
    std::string tmp = std::string(path) + ".tmp", bak = std::string(path) + ".bak";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) {
        return false;
    }
    std::string text = Serialize(d);
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(tmp.c_str());
        return false;
    }
    remove(bak.c_str());
    rename(path, bak.c_str());
    if (rename(tmp.c_str(), path) != 0) {
        rename(bak.c_str(), path);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- presets

int FindPreset(const Data& d, const std::string& name) {
    for (size_t i = 0; i < d.presets.size(); ++i) {
        if (d.presets[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int FindServer(const Data& d, const std::string& address) {
    for (size_t i = 0; i < d.servers.size(); ++i) {
        if (d.servers[i].address == address) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string UniqueName(const Data& d, const std::string& base, int except) {
    auto taken = [&](const std::string& name) {
        for (size_t i = 0; i < d.presets.size(); ++i) {
            if (static_cast<int>(i) != except && SameNick(d.presets[i].name, name)) {
                return true;
            }
        }
        return false;
    };
    if (!taken(base)) {
        return base;
    }
    for (int n = 2;; ++n) {
        std::string name = base + " (" + std::to_string(n) + ")";
        if (!taken(name)) {
            return name;
        }
    }
}

std::string AutoName(const std::string& hostname) {
    std::string s = StripColors(hostname), out;
    for (char c : s) {
        if (c == '\t' || c == '\n' || c == '\r') {
            c = ' ';
        }
        if (c == ' ' && (out.empty() || out.back() == ' ')) {
            continue;
        }
        out += c;
    }
    if (out.size() > 40) {
        size_t cut = 40;
        while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) {
            --cut;
        }
        out.resize(cut);
    }
    out = Trim(out);
    return out.empty() ? std::string("Servidor") : out;
}

int LinkServer(Data& d, const std::string& address, const std::string& hostname) {
    int s = FindServer(d, address);
    if (s < 0) {
        ServerLink l;
        l.address = address;
        d.servers.push_back(l);
        s = static_cast<int>(d.servers.size()) - 1;
    }
    ServerLink& link = d.servers[s];
    if (!hostname.empty()) {
        link.hostname = hostname;
    }
    int p = link.preset.empty() ? -1 : FindPreset(d, link.preset);
    if (p >= 0) {
        return p;
    }
    std::string name = AutoName(link.hostname.empty() ? address : link.hostname);
    p = FindPreset(d, name);
    if (p < 0) {
        Preset preset;
        preset.name = UniqueName(d, name);
        d.presets.push_back(preset);
        p = static_cast<int>(d.presets.size()) - 1;
    }
    link.preset = d.presets[p].name;
    return p;
}

std::vector<std::string> ServersOf(const Data& d, const std::string& preset) {
    std::vector<std::string> out;
    for (const ServerLink& l : d.servers) {
        if (l.preset == preset) {
            out.push_back(l.hostname.empty() ? l.address : AutoName(l.hostname));
        }
    }
    return out;
}

void UsePreset(Data& d, const std::string& address, int preset) {
    if (preset < 0 || preset >= static_cast<int>(d.presets.size())) {
        return;
    }
    int s = FindServer(d, address);
    if (s < 0) {
        ServerLink l;
        l.address = address;
        d.servers.push_back(l);
        s = static_cast<int>(d.servers.size()) - 1;
    }
    std::string old = d.servers[s].preset;
    std::string now = d.presets[preset].name;
    d.servers[s].preset = now;
    int o = old == now ? -1 : FindPreset(d, old);
    if (o >= 0 && d.presets[o].binds.empty() && ServersOf(d, old).empty()) {
        d.presets.erase(d.presets.begin() + o);
    }
}

bool RenamePreset(Data& d, int preset, const std::string& rawName) {
    std::string name = Trim(rawName);
    if (preset < 0 || preset >= static_cast<int>(d.presets.size()) || name.empty()) {
        return false;
    }
    if (UniqueName(d, name, preset) != name) {
        return false;
    }
    std::string old = d.presets[preset].name;
    d.presets[preset].name = name;
    for (ServerLink& l : d.servers) {
        if (l.preset == old) {
            l.preset = name;
        }
    }
    if (d.lastPreset == old) {
        d.lastPreset = name;
    }
    return true;
}

int NewPreset(Data& d, const std::string& base, bool copyFrom, int source) {
    Preset p;
    p.name = UniqueName(d, Trim(base).empty() ? std::string("Novo preset") : Trim(base));
    if (copyFrom && source >= 0 && source < static_cast<int>(d.presets.size())) {
        p.binds = d.presets[source].binds;
    }
    d.presets.push_back(p);
    return static_cast<int>(d.presets.size()) - 1;
}

void DeletePreset(Data& d, int preset) {
    if (preset < 0 || preset >= static_cast<int>(d.presets.size())) {
        return;
    }
    std::string name = d.presets[preset].name;
    d.presets.erase(d.presets.begin() + preset);
    d.servers.erase(std::remove_if(d.servers.begin(), d.servers.end(),
                                   [&](const ServerLink& l) { return l.preset == name; }),
                    d.servers.end());
    if (d.lastPreset == name) {
        d.lastPreset.clear();
    }
}

// ---------------------------------------------------------------- binds

std::vector<int> Conflicts(const std::vector<Bind>& binds, const Bind& b, int self) {
    std::vector<int> out;
    if (b.cls != CLASS_AUTO || !b.enabled) {
        return out;
    }
    for (size_t i = 0; i < binds.size(); ++i) {
        const Bind& o = binds[i];
        if (static_cast<int>(i) == self || o.cls != CLASS_AUTO || !o.enabled || o.delay != b.delay) {
            continue;
        }
        if (SameMoment(b, o)) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

bool KeysOverlap(const Bind& a, const Bind& b) {
    if (a.cls == CLASS_AUTO || b.cls == CLASS_AUTO || a.key.Empty() || !(a.key == b.key)) {
        return false;
    }
    return a.mode == b.mode || a.mode == MODE_PRESS || b.mode == MODE_PRESS;
}

std::vector<int> KeyConflicts(const std::vector<Bind>& binds, const Bind& b, int self) {
    std::vector<int> out;
    if (!b.enabled) {
        return out;
    }
    for (size_t i = 0; i < binds.size(); ++i) {
        if (static_cast<int>(i) != self && binds[i].enabled && KeysOverlap(b, binds[i])) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

bool RadialConflict(const Settings& s, const Bind& b) {
    return b.enabled && b.cls != CLASS_AUTO && !b.key.Empty() && b.key == s.radial;
}

std::vector<int> RadialConflicts(const std::vector<Bind>& binds, const Hotkey& radial) {
    std::vector<int> out;
    Settings s;
    s.radial = radial;
    for (size_t i = 0; i < binds.size(); ++i) {
        if (RadialConflict(s, binds[i])) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> out;
    size_t a = 0;
    while (a <= text.size()) {
        size_t e = text.find('\n', a);
        if (e == std::string::npos) {
            e = text.size();
        }
        std::string line = text.substr(a, e - a);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!Trim(line).empty()) {
            out.push_back(line);
        }
        a = e + 1;
    }
    return out;
}

std::string Fill(const std::string& line, int id, const std::string& nick) {
    std::string out;
    std::string idText = std::to_string(id);
    for (size_t i = 0; i < line.size();) {
        char open = line[i];
        if (open == '(' || open == '{') {
            char close = open == '(' ? ')' : '}';
            if (LowerEquals(line, i + 1, "id") && i + 3 < line.size() && line[i + 3] == close) {
                out += idText;
                i += 4;
                continue;
            }
            if (LowerEquals(line, i + 1, "nick") && i + 5 < line.size() && line[i + 5] == close) {
                out += nick;
                i += 6;
                continue;
            }
        }
        out += line[i++];
    }
    return out;
}

bool UsesTarget(const std::string& text) {
    return Fill(text, -7, "\x01") != text;
}

int Codepoints(const std::string& utf8) {
    int n = 0;
    for (unsigned char c : utf8) {
        n += (c & 0xC0) != 0x80;
    }
    return n;
}

const char* ClassName(int cls) {
    static const char* const names[] = {"Padr\xC3\xA3o", "Autom\xC3\xA1tico", "Ao redor", "Na vis\xC3\xA3o"};
    return names[Clamp(cls, 0, CLASS_COUNT - 1)];
}

const char* CondName(int cond) {
    static const char* const names[] = {
        "Entrar num ve\xC3\xAD" "culo",
        "Sair do ve\xC3\xAD" "culo",
        "Vida abaixo de",
        "Colete abaixo de",
        "Lataria do ve\xC3\xAD" "culo abaixo de",
        "Nascer (spawn)",
        "Morrer",
        "Sacar uma arma",
        "Entrar num interior",
        "Sair para a rua",
        "Conectar no servidor",
        "Mensagem no chat",
    };
    return names[Clamp(cond, 0, C_COUNT - 1)];
}

bool CondHasValue(int cond) {
    return cond == C_HEALTH || cond == C_ARMOUR || cond == C_VEHICLE_HEALTH;
}

const char* WeaponName(int id) {
    switch (id) {
    case WEAPON_ANY: return "Qualquer arma";
    case 1: return "Soco-ingl\xC3\xAAs";
    case 2: return "Taco de golfe";
    case 3: return "Cassetete";
    case 4: return "Faca";
    case 5: return "Taco de beisebol";
    case 6: return "P\xC3\xA1";
    case 7: return "Taco de sinuca";
    case 8: return "Katana";
    case 9: return "Motosserra";
    case 10: case 11: case 12: case 13: return "Vibrador";
    case 14: return "Flores";
    case 15: return "Bengala";
    case 16: return "Granada";
    case 17: return "G\xC3\xA1s lacrimog\xC3\xAAneo";
    case 18: return "Coquetel molotov";
    case 22: return "Pistola 9mm";
    case 23: return "Pistola com silenciador";
    case 24: return "Desert Eagle";
    case 25: return "Escopeta";
    case 26: return "Escopeta de cano serrado";
    case 27: return "Escopeta de combate";
    case 28: return "Micro Uzi";
    case 29: return "MP5";
    case 30: return "AK-47";
    case 31: return "M4";
    case 32: return "Tec-9";
    case 33: return "Rifle";
    case 34: return "Rifle de precis\xC3\xA3o";
    case 35: return "Lan\xC3\xA7" "a-foguetes";
    case 36: return "Lan\xC3\xA7" "a-m\xC3\xADsseis";
    case 37: return "Lan\xC3\xA7" "a-chamas";
    case 38: return "Minigun";
    case 39: return "Explosivo (C4)";
    case 40: return "Detonador";
    case 41: return "Spray";
    case 42: return "Extintor";
    case 43: return "C\xC3\xA2mera";
    case 44: return "Vis\xC3\xA3o noturna";
    case 45: return "Vis\xC3\xA3o t\xC3\xA9rmica";
    case 46: return "Paraquedas";
    default: return nullptr;
    }
}

std::string Describe(const Bind& b) {
    std::string s = CondName(b.cond);
    char buf[160];
    switch (b.cond) {
    case C_ENTER_VEHICLE:
        if (b.seat == SEAT_DRIVER) s += " (motorista)";
        if (b.seat == SEAT_PASSENGER) s += " (passageiro)";
        break;
    case C_HEALTH:
    case C_ARMOUR:
    case C_VEHICLE_HEALTH:
        snprintf(buf, sizeof(buf), " %d", b.value);
        s += buf;
        break;
    case C_WEAPON: {
        const char* w = WeaponName(b.value);
        if (b.value != WEAPON_ANY && w) {
            s = std::string("Sacar: ") + w;
        }
        break;
    }
    case C_CHAT:
        if (!b.trigger.empty()) {
            std::string t = b.trigger.size() > 34 ? b.trigger.substr(0, 31) + "..." : b.trigger;
            s = "Chat: \"" + t + "\"";
        }
        break;
    default:
        break;
    }
    if (b.delay == 0) {
        return s + " \xC2\xB7 na hora";
    }
    if (b.delay % 1000 == 0) {
        snprintf(buf, sizeof(buf), " \xC2\xB7 %d s", b.delay / 1000);
    } else {
        snprintf(buf, sizeof(buf), " \xC2\xB7 %d,%d s", b.delay / 1000, (b.delay % 1000) / 100);
    }
    return s + buf;
}

// ---------------------------------------------------------------- mensagem no chat

std::string Normalize(const std::string& utf8) {
    std::string s = StripColors(utf8), out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char ch = static_cast<char>(c);
        if (c == 0xC3 && i + 1 < s.size()) {
            char f = Fold(static_cast<unsigned char>(s[i + 1]));
            if (f) {
                ch = f;
                ++i;
            }
        } else if (c == '\t' || c == '\r' || c == '\n') {
            ch = ' ';
        }
        ch = Lower(ch);
        if (ch == ' ' && (out.empty() || out.back() == ' ')) {
            continue;
        }
        out += ch;
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

bool ChatMatches(const std::string& trigger, const std::string& message) {
    std::string t = Normalize(trigger);
    return !t.empty() && Normalize(message).find(t) != std::string::npos;
}

bool SameNick(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (Lower(a[i]) != Lower(b[i])) {
            return false;
        }
    }
    return true;
}

bool Ignored(const Bind& b, const std::string& nick) {
    for (const std::string& n : b.ignore) {
        if (SameNick(n, nick)) {
            return true;
        }
    }
    return false;
}

bool FindPlayer(const std::string& message, const std::string& author, const std::vector<Player>& players, Player* out) {
    if (!author.empty()) {
        for (const Player& p : players) {
            if (SameNick(p.nick, author)) {
                *out = p;
                return true;
            }
        }
    }
    std::string text = StripColors(message);
    // Nick que aparece na mensagem ("Fulano_Silva" ou "Fulano Silva"): o primeiro; no empate, o mais comprido.
    size_t bestAt = std::string::npos, bestLen = 0;
    const Player* best = nullptr;
    for (const Player& p : players) {
        if (p.nick.size() < 3) {
            continue;
        }
        std::string spaced = p.nick;
        std::replace(spaced.begin(), spaced.end(), '_', ' ');
        for (const std::string* form : {&p.nick, static_cast<const std::string*>(&spaced)}) {
            for (size_t at = 0; at + form->size() <= text.size(); ++at) {
                if (!LowerEquals(text, at, *form)) {
                    continue;
                }
                bool left = at == 0 || !NickChar(text[at - 1]);
                size_t e = at + form->size();
                bool right = e >= text.size() || !NickChar(text[e]);
                if (!left || !right) {
                    continue;
                }
                if (at < bestAt || (at == bestAt && form->size() > bestLen)) {
                    bestAt = at;
                    bestLen = form->size();
                    best = &p;
                }
                break;
            }
        }
    }
    if (best) {
        *out = *best;
        return true;
    }
    // ID: "(12)", "[12]", "id 12", "id: 12", "ID:12", "#12".
    std::string low = text;
    for (char& c : low) {
        c = Lower(c);
    }
    for (size_t i = 0; i < low.size(); ++i) {
        size_t digits = std::string::npos;
        char close = 0;
        if (low[i] == '(' || low[i] == '[') {
            digits = i + 1;
            close = low[i] == '(' ? ')' : ']';
        } else if (low[i] == '#') {
            digits = i + 1;
        } else if (low.compare(i, 2, "id") == 0 && (i == 0 || !NickChar(low[i - 1]))) {
            size_t k = i + 2;
            while (k < low.size() && (low[k] == ' ' || low[k] == ':' || low[k] == '.')) {
                ++k;
            }
            if (k > i + 2 || (k < low.size() && low[k] >= '0' && low[k] <= '9')) {
                digits = k;
            }
        }
        if (digits == std::string::npos || digits >= low.size() || low[digits] < '0' || low[digits] > '9') {
            continue;
        }
        size_t k = digits;
        int id = 0;
        while (k < low.size() && low[k] >= '0' && low[k] <= '9' && k - digits < 5) {
            id = id * 10 + (low[k++] - '0');
        }
        if (k < low.size() && low[k] >= '0' && low[k] <= '9') {
            continue;
        }
        if (close && (k >= low.size() || low[k] != close)) {
            continue;
        }
        for (const Player& p : players) {
            if (p.id == id) {
                *out = p;
                return true;
            }
        }
    }
    return false;
}

} // namespace kb
