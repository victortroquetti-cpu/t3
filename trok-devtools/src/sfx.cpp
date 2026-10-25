// Trok DevTools (.asi) -- Victor_Trok
// Formato (gta-reversed, CAEBankLoader): PakFiles.dat = 0x34 por pacote (nome em 12 bytes); BankLkup.dat = 12 bytes por
// banco {pacote (1 byte), 3 de folga, inicio no pacote, tamanho}; no pacote, cada banco comeca com o cabecalho de 0x12C4
// bytes {int16 numero de sons, 2 de folga, 400 x {inicio, inicio do loop (-1 = sem), frequencia (uint16), headroom}} e
// depois o PCM 16 bits mono de todos os sons em sequencia.

#include "sfx.h"

#include "audio_data.h"
#include "catalog.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <unordered_map>

void DevToolsLog(const char* fmt, ...);

namespace sfx {
namespace {

constexpr DWORD HEADER = 0x12C4;
constexpr int MAX_SOUNDS = 400;

std::atomic<int> g_stage{0}; // 0 parado, 1 lendo, 2 lido, 3 pronto (com os veiculos)
std::string g_status = "parado";
std::string g_gameDir;
std::vector<std::string> g_pakNames;
std::vector<Pack> g_packs;
std::vector<Bank> g_banks;
int g_failed = 0, g_soundTotal = 0;

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Capitalize(std::string s) {
    s = Lower(s);
    if (!s.empty()) {
        s[0] = static_cast<char>(toupper(static_cast<unsigned char>(s[0])));
    }
    return s;
}

bool ReadFile(const std::string& path, std::vector<BYTE>* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    out->resize(size > 0 ? size : 0);
    bool ok = size > 0 && fread(out->data(), 1, out->size(), f) == out->size();
    fclose(f);
    return ok;
}

bool ReadAt(FILE* f, DWORD offset, void* buf, size_t size) {
    return fseek(f, static_cast<long>(offset), SEEK_SET) == 0 && fread(buf, 1, size, f) == size;
}

// O cabecalho faz sentido? (para decidir se o inicio do BankLkup.dat esta em bytes ou em setores de 2048)
bool Plausible(const BYTE* h, DWORD bytes) {
    short count = *reinterpret_cast<const short*>(h);
    if (count < 1 || count > MAX_SOUNDS) {
        return false;
    }
    DWORD prev = 0;
    for (int i = 0; i < count; ++i) {
        const BYTE* item = h + 4 + i * 12;
        DWORD off = *reinterpret_cast<const DWORD*>(item);
        WORD rate = *reinterpret_cast<const WORD*>(item + 8);
        if ((i == 0 && off != 0) || off < prev || off + HEADER > bytes || rate < 4000 || rate > 48000) {
            return false;
        }
        prev = off;
    }
    return true;
}

const char* FeetSurface(const std::string& n) {
    static const char* const map[][2] = {{"GENERIC", "gen\xC3\xA9rico"}, {"GRASS", "grama"},  {"GRAVEL", "cascalho"},
                                         {"METAL", "metal"},             {"SAND", "areia"},   {"TILE", "piso"},
                                         {"WOOD", "madeira"}};
    for (const auto& m : map) {
        if (n == m[0]) {
            return m[1];
        }
    }
    return "";
}

// O tipo e a frase de cada banco (os de motor ganham os veiculos depois).
void Classify(Bank& b, const std::string& pack, const std::vector<bool>& playerBank, const std::vector<bool>& dummyBank) {
    const std::string& n = b.name;
    auto ends = [&](const char* s) { return n.size() >= strlen(s) && n.compare(n.size() - strlen(s), strlen(s), s) == 0; };
    if (pack == "FEET") {
        b.kind = T_FEET;
        b.text = std::string("passos \xC2\xB7 ") + FeetSurface(n);
    } else if (pack == "PAIN_A") {
        b.kind = T_PAIN;
        b.text = n == "CARL" ? "gritos de dor do CJ" : n == "FEMALE" ? "gritos de dor de mulheres" : "gritos de dor de homens";
    } else if (pack == "SCRIPT") {
        b.kind = T_SCRIPT;
        b.text = n.rfind("SCANNER", 0) == 0 ? "r\xC3\xA1" "dio da pol\xC3\xAD" "cia" : "som de miss\xC3\xA3o";
    } else if (pack.rfind("SPC_", 0) == 0) {
        b.kind = T_SPEECH;
        b.text = "falas";
    } else {
        static const char* const other[][2] = {
            {"BULLET_HITS", "impactos e ricochetes de bala"},
            {"BULLET_PASS", "balas passando perto"},
            {"COLLISIONS", "colis\xC3\xB5" "es e batidas"},
            {"DOORS", "portas e garagens"},
            {"EXPLOSIONS", "explos\xC3\xB5" "es"},
            {"FRONTEND_GAME", "avisos do jogo: pickups, dinheiro, miss\xC3\xA3o"},
            {"FRONTEND_MENU", "menus"},
            {"HAIRCUT", "barbearia"},
            {"HORN", "buzinas"},
            {"LOADING", "tela de carregamento"},
            {"RAIN", "chuva"},
            {"SWIMMING", "nata\xC3\xA7\xC3\xA3o e \xC3\xA1gua"},
            {"VEHICLE_GEN", "ve\xC3\xAD" "culos: portas, freio, pneus, sirene"},
            {"WEAPONS", "armas: tiros, recarga e golpes"},
            {"CHAINSAW", "motosserra"},
        };
        for (const auto& o : other) {
            if (n.rfind(o[0], 0) == 0) {
                b.text = o[1];
            }
        }
        bool weapon = n.rfind("BULLET", 0) == 0 || n == "WEAPONS" || n == "EXPLOSIONS" || n.rfind("CHAINSAW", 0) == 0;
        if (weapon) {
            b.kind = T_WEAPONS;
        } else if (ends("_D") || (dummyBank[b.id] && !playerBank[b.id])) {
            b.kind = T_ENGINE_TRAFFIC;
            b.text = "motor, no tr\xC3\xA1" "fego";
        } else if (ends("_P") || playerBank[b.id]) {
            b.kind = T_ENGINE_PLAYER;
            b.text = "motor, com voc\xC3\xAA dirigindo";
        } else if (b.text.empty()) {
            b.kind = T_OTHER;
        }
    }
}

DWORD WINAPI Worker(void*) {
    std::vector<BYTE> paks, lkup;
    std::string cfg = g_gameDir + "audio\\CONFIG\\";
    if (!ReadFile(cfg + "PakFiles.dat", &paks) || !ReadFile(cfg + "BankLkup.dat", &lkup)) {
        g_status = "audio\\CONFIG n\xC3\xA3o encontrado";
        DevToolsLog("sfx: audio\\CONFIG\\PakFiles.dat ou BankLkup.dat nao abriu");
        g_stage = 2;
        return 0;
    }
    for (size_t i = 0; i + 0x34 <= paks.size(); i += 0x34) {
        g_pakNames.push_back(std::string(reinterpret_cast<const char*>(&paks[i]), strnlen(reinterpret_cast<const char*>(&paks[i]), 12)));
    }
    int count = static_cast<int>(lkup.size() / 12);
    std::vector<bool> playerBank(count + 1, false), dummyBank(count + 1, false);
    for (const audio_data::VehicleBanks& v : audio_data::VEHICLE_BANKS) {
        if (v.player >= 0 && v.player < count) {
            playerBank[v.player] = true;
        }
        if (v.dummy >= 0 && v.dummy < count) {
            dummyBank[v.dummy] = true;
        }
    }
    std::vector<FILE*> files(g_pakNames.size(), nullptr);
    for (size_t i = 0; i < g_pakNames.size(); ++i) {
        files[i] = fopen((g_gameDir + "audio\\SFX\\" + g_pakNames[i]).c_str(), "rb");
        g_packs.push_back(Pack());
        g_packs.back().name = g_pakNames[i];
    }
    static const char* const packText[][2] = {
        {"GENRL", "efeitos gerais: motores, armas, portas, menus"},
        {"FEET", "passos, por tipo de ch\xC3\xA3o"},
        {"PAIN_A", "gritos de dor"},
        {"SCRIPT", "sons de miss\xC3\xB5" "es e de script"},
        {"SPC_EA", "falas: pol\xC3\xAD" "cia, ex\xC3\xA9rcito e socorro"},
        {"SPC_FA", "falas: personagens e lojas"},
        {"SPC_GA", "falas: pedestres e gangues"},
        {"SPC_NA", "falas: personagens da hist\xC3\xB3ria"},
        {"SPC_PA", "falas do CJ"},
    };
    for (Pack& p : g_packs) {
        for (const auto& t : packText) {
            if (p.name == t[0]) {
                p.description = t[1];
            }
        }
    }
    std::unordered_map<long long, const char*> soundNames;
    for (const audio_data::SoundName& s : audio_data::SOUND_NAMES) {
        soundNames[static_cast<long long>(s.bank) << 16 | s.sound] = s.name;
    }
    std::vector<BYTE> header(HEADER);
    for (int id = 0; id < count; ++id) {
        const BYTE* e = &lkup[id * 12];
        Bank b;
        b.id = id;
        b.pakFile = e[0];
        b.fileOffset = *reinterpret_cast<const DWORD*>(e + 4);
        b.bytes = *reinterpret_cast<const DWORD*>(e + 8);
        b.pack = b.pakFile < static_cast<int>(g_packs.size()) ? b.pakFile : 0;
        Pack& pack = g_packs[b.pack];
        b.number = static_cast<int>(pack.banks.size()) + 1;
        b.name = id < audio_data::BANK_COUNT ? audio_data::BANK_NAMES[id] : "BANK";
        FILE* f = b.pakFile < static_cast<int>(files.size()) ? files[b.pakFile] : nullptr;
        bool read = f && ReadAt(f, b.fileOffset, header.data(), HEADER) && Plausible(header.data(), b.bytes);
        if (!read && f && ReadAt(f, b.fileOffset * 2048, header.data(), HEADER) && Plausible(header.data(), b.bytes)) {
            b.fileOffset *= 2048; // em setores
            read = true;
        }
        if (read) {
            b.ok = true;
            int n = *reinterpret_cast<const short*>(header.data());
            for (int i = 0; i < n; ++i) {
                const BYTE* item = &header[4 + i * 12];
                Sound s;
                s.index = i;
                s.offset = *reinterpret_cast<const DWORD*>(item);
                s.loopStart = *reinterpret_cast<const DWORD*>(item + 4);
                s.rate = *reinterpret_cast<const WORD*>(item + 8);
                DWORD next = i + 1 < n ? *reinterpret_cast<const DWORD*>(&header[4 + (i + 1) * 12]) : b.bytes - HEADER;
                s.size = next > s.offset ? next - s.offset : 0;
                s.seconds = s.rate > 0 ? s.size / 2.0f / s.rate : 0.0f;
                s.loop = s.loopStart != 0xFFFFFFFF;
                auto it = soundNames.find(static_cast<long long>(id) << 16 | i);
                if (it != soundNames.end()) {
                    s.name = it->second;
                }
                b.sounds.push_back(s);
            }
        } else {
            ++g_failed;
        }
        Classify(b, pack.name, playerBank, dummyBank);
        pack.banks.push_back(id);
        pack.sounds += static_cast<int>(b.sounds.size());
        g_soundTotal += static_cast<int>(b.sounds.size());
        g_banks.push_back(std::move(b));
    }
    for (FILE* f : files) {
        if (f) {
            fclose(f);
        }
    }
    // Armas: a lista de cada som do banco WEAPONS (e a motosserra inteira)
    for (const audio_data::Weapon& w : audio_data::WEAPONS) {
        if (w.sounds) {
            for (const short* s = w.sounds; *s >= 0; ++s) {
                if (audio_data::WEAPONS_BANK < count && *s < static_cast<int>(g_banks[audio_data::WEAPONS_BANK].sounds.size())) {
                    Sound& snd = g_banks[audio_data::WEAPONS_BANK].sounds[*s];
                    snd.weapons += (snd.weapons.empty() ? "" : ", ") + std::string(w.name);
                    snd.search += std::string(" ") + w.keys + " " + Lower(w.name);
                }
            }
        } else if (audio_data::CHAINSAW_BANK < count) {
            for (Sound& snd : g_banks[audio_data::CHAINSAW_BANK].sounds) {
                snd.weapons = w.name;
                snd.search += std::string(" ") + w.keys;
            }
        }
    }
    DevToolsLog("sfx: %d pacotes, %d bancos, %d sons (%d bancos sem cabecalho)", static_cast<int>(g_packs.size()), count,
                g_soundTotal, g_failed);
    g_status = "pronto";
    g_stage = 2;
    return 0;
}

// Com o catalogo pronto: os nomes dos veiculos de cada banco de motor e as palavras da busca.
void Finish() {
    for (int i = 0; i < 212; ++i) {
        const audio_data::VehicleBanks& v = audio_data::VEHICLE_BANKS[i];
        for (short id : {v.player, v.dummy}) {
            if (id >= 0 && id < static_cast<int>(g_banks.size())) {
                std::vector<int>& list = g_banks[id].vehicles;
                if (std::find(list.begin(), list.end(), 400 + i) == list.end()) {
                    list.push_back(400 + i);
                }
            }
        }
    }
    for (Bank& b : g_banks) {
        std::string names;
        for (size_t k = 0; k < b.vehicles.size(); ++k) {
            const catalog::Entry* e = catalog::Find(b.vehicles[k]);
            std::string name = e && !e->dff.empty() ? Capitalize(e->dff) : std::to_string(b.vehicles[k]);
            names += " " + Lower(name);
            if (k < 3) {
                b.vehicleText += (k ? ", " : "") + name;
            }
        }
        if (b.vehicles.size() > 3) {
            b.vehicleText += " e mais " + std::to_string(b.vehicles.size() - 3);
        }
        char number[32];
        snprintf(number, sizeof(number), "bank_%03d %d ", b.number, b.number);
        b.search = Lower(g_packs[b.pack].name + " " + number + b.name + " " + b.text + " " + KindName(b.kind)) + names;
        for (Sound& s : b.sounds) {
            char label[32];
            snprintf(label, sizeof(label), "sound_%03d", s.index + 1);
            s.search = Lower(std::string(label) + " " + s.name + " " + s.weapons) + s.search + (s.loop ? " loop" : "");
        }
    }
}

} // namespace

void Start(const char* gameDir) {
    if (g_stage != 0) {
        return;
    }
    g_gameDir = gameDir ? gameDir : "";
    g_stage = 1;
    g_status = "lendo audio\\SFX";
    HANDLE thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        Worker(nullptr);
    }
}

void Update() {
    if (g_stage == 2 && catalog::Ready()) {
        Finish();
        g_stage = 3;
    }
}

bool Ready() {
    return g_stage == 3;
}

const char* Status() {
    return g_stage == 2 ? "esperando o cat\xC3\xA1logo de modelos" : g_status.c_str();
}

int Failed() {
    return g_failed;
}

const std::vector<Pack>& Packs() {
    return g_packs;
}

const std::vector<Bank>& Banks() {
    return g_banks;
}

const Bank* FindBank(int id) {
    return id >= 0 && id < static_cast<int>(g_banks.size()) ? &g_banks[id] : nullptr;
}

const Sound* FindSound(int bank, int sound) {
    const Bank* b = FindBank(bank);
    return b && sound >= 0 && sound < static_cast<int>(b->sounds.size()) ? &b->sounds[sound] : nullptr;
}

const char* KindName(int kind) {
    static const char* const names[] = {"Motor no tr\xC3\xA1" "fego", "Motor com voc\xC3\xAA dirigindo", "Armas e tiros",
                                        "Passos",                   "Dor",                            "Miss\xC3\xB5" "es e script",
                                        "Falas",                    "Outros efeitos"};
    return kind >= 0 && kind < KIND_COUNT ? names[kind] : "Todos os tipos";
}

int BankCount() {
    return static_cast<int>(g_banks.size());
}

int SoundCount() {
    return g_soundTotal;
}

Result Apply(const Filter& f) {
    Result r;
    r.sounds.resize(g_banks.size());
    r.bankCount.assign(g_packs.size(), 0);
    r.soundCount.assign(g_packs.size(), 0);
    std::vector<std::string> terms;
    std::string q = Lower(f.query);
    for (size_t i = 0; i < q.size();) {
        size_t j = q.find(' ', i);
        if (j == std::string::npos) {
            j = q.size();
        }
        if (j > i) {
            terms.push_back(q.substr(i, j - i));
        }
        i = j + 1;
    }
    std::vector<bool> weaponSound;
    bool weaponChainsaw = false;
    if (f.weapon >= 0 && f.weapon < static_cast<int>(sizeof(audio_data::WEAPONS) / sizeof(audio_data::WEAPONS[0]))) {
        const audio_data::Weapon& w = audio_data::WEAPONS[f.weapon];
        if (w.sounds) {
            weaponSound.assign(MAX_SOUNDS, false);
            for (const short* s = w.sounds; *s >= 0; ++s) {
                weaponSound[*s] = true;
            }
        } else {
            weaponChainsaw = true;
        }
    }
    for (const Bank& b : g_banks) {
        if (f.kind >= 0 && b.kind != f.kind) {
            continue;
        }
        if (f.vehicle >= 0 && std::find(b.vehicles.begin(), b.vehicles.end(), f.vehicle) == b.vehicles.end()) {
            continue;
        }
        if (f.weapon >= 0 && !(weaponChainsaw ? b.id == audio_data::CHAINSAW_BANK : b.id == audio_data::WEAPONS_BANK)) {
            continue;
        }
        std::vector<int>& list = r.sounds[b.id];
        for (const Sound& s : b.sounds) {
            if ((f.loop == 1 && !s.loop) || (f.loop == 2 && s.loop)) {
                continue;
            }
            if (!weaponSound.empty() && !weaponSound[s.index]) {
                continue;
            }
            bool ok = true;
            for (const std::string& t : terms) {
                ok = ok && (b.search.find(t) != std::string::npos || s.search.find(t) != std::string::npos);
            }
            if (ok) {
                list.push_back(s.index);
            }
        }
        if (!list.empty()) {
            ++r.bankCount[b.pack];
            r.soundCount[b.pack] += static_cast<int>(list.size());
            ++r.banks;
            r.total += static_cast<int>(list.size());
        }
    }
    return r;
}

bool ReadPcm(int bank, int sound, std::vector<short>* out, int* rate) {
    const Bank* b = FindBank(bank);
    const Sound* s = FindSound(bank, sound);
    if (!b || !s || !b->ok || b->pakFile >= static_cast<int>(g_pakNames.size()) || s->size < 2) {
        return false;
    }
    FILE* f = fopen((g_gameDir + "audio\\SFX\\" + g_pakNames[b->pakFile]).c_str(), "rb");
    if (!f) {
        return false;
    }
    out->resize(s->size / 2);
    bool ok = ReadAt(f, b->fileOffset + HEADER + s->offset, out->data(), out->size() * 2);
    fclose(f);
    *rate = s->rate;
    return ok;
}

std::string Label(int bank, int sound) {
    const Bank* b = FindBank(bank);
    if (!b) {
        return "?";
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "%s_Bank_%03d_%s_sound_%03d", g_packs[b->pack].name.c_str(), b->number, b->name.c_str(),
             sound + 1);
    return buf;
}

bool ExportWav(int bank, int sound, std::string* path) {
    std::vector<short> pcm;
    int rate = 0;
    if (!ReadPcm(bank, sound, &pcm, &rate)) {
        return false;
    }
    std::string dir = g_gameDir + "Trok DevTools";
    CreateDirectoryA(dir.c_str(), nullptr);
    dir += "\\sons";
    CreateDirectoryA(dir.c_str(), nullptr);
    *path = dir + "\\" + Label(bank, sound) + ".wav";
    FILE* f = fopen(path->c_str(), "wb");
    if (!f) {
        return false;
    }
    DWORD data = static_cast<DWORD>(pcm.size() * 2), riff = 36 + data, fmtSize = 16, byteRate = rate * 2;
    WORD format = 1, channels = 1, align = 2, bits = 16;
    DWORD sampleRate = rate;
    fwrite("RIFF", 1, 4, f);
    fwrite(&riff, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmtSize, 4, 1, f);
    fwrite(&format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sampleRate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f);
    fwrite(&align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data, 4, 1, f);
    bool ok = fwrite(pcm.data(), 2, pcm.size(), f) == pcm.size();
    fclose(f);
    return ok;
}

} // namespace sfx
