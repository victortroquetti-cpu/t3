// Trok DevTools (.asi) -- Victor_Trok
// Os arquivos do jogo sao lidos numa thread (so leitura de disco): o DATA\DEFAULT.DAT e o DATA\GTA.DAT dizem quais
// .ide carregar; cada .ide da o id, o nome do DFF e do TXD de cada modelo (secoes objs, tobj, anim, peds, cars, weap e
// hier) e os pais dos TXDs (txdp). Os .ide do SA-MP (SAMP\SAMP.IDE e SAMP\CUSTOM.IDE) entram se existirem. Os .img
// (VER2) dao o nome de cada entrada pela posicao. Depois, na thread do jogo, cada id da tabela de modelos ganha o tipo,
// o .img e a posicao de onde o streaming le, e a comparacao: a entrada do .img naquela posicao tem o nome do .ide?
// Se nao tem, o modelo foi substituido por outro arquivo (outro .img ou um loader de mods).

#include "catalog.h"

#include "gta.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_map>

void DevToolsLog(const char* fmt, ...);

namespace catalog {
namespace {

constexpr DWORD SECTOR = 2048;

struct IdeDef {
    std::string section, dff, txd, ide;
};

struct Files {
    std::string gameDir;
    std::string imgPaths[8];
    // Saida da thread:
    std::unordered_map<int, IdeDef> ide;
    std::unordered_map<std::string, std::string> txdParent; // filho -> pai (minusculas)
    std::map<DWORD, std::string> imgDir[8];                 // posicao -> nome da entrada (minusculas, com extensao)
    int ideFiles = 0, ideModels = 0;
};

Files g_files;
std::atomic<int> g_stage{0}; // 0 parado, 1 lendo, 2 arquivos prontos, 3 catalogo pronto
std::vector<Entry> g_entries;
std::unordered_map<int, int> g_byId;
std::vector<std::string> g_archives;
std::string g_status = "esperando";

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace(static_cast<unsigned char>(s[a]))) {
        ++a;
    }
    while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) {
        --b;
    }
    return s.substr(a, b - a);
}

// Campos separados por virgula e/ou espaco.
std::vector<std::string> Fields(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ',' || c == ' ' || c == '\t') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

bool ReadFile(const std::string& path, std::string* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    char buf[16384];
    size_t n;
    out->clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out->append(buf, n);
    }
    fclose(f);
    return true;
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n' || text[i] == '\r') {
            if (i > start) {
                out.push_back(text.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    return out;
}

std::string StripComment(const std::string& line) {
    size_t hash = line.find('#');
    return Trim(hash == std::string::npos ? line : line.substr(0, hash));
}

std::string Normalize(std::string path) {
    for (char& c : path) {
        if (c == '/') {
            c = '\\';
        }
    }
    return path;
}

void ParseIde(Files& f, const std::string& relPath) {
    std::string text;
    if (!ReadFile(f.gameDir + relPath, &text)) {
        return;
    }
    ++f.ideFiles;
    std::string section;
    for (const std::string& raw : Lines(text)) {
        std::string line = StripComment(raw);
        if (line.empty()) {
            continue;
        }
        std::string lower = Lower(line);
        if (lower == "end") {
            section.clear();
            continue;
        }
        if (section.empty()) {
            if (lower == "objs" || lower == "tobj" || lower == "anim" || lower == "peds" || lower == "cars" ||
                lower == "weap" || lower == "hier" || lower == "txdp" || lower == "path" || lower == "2dfx" ||
                lower == "tanm") {
                section = lower;
            }
            continue;
        }
        std::vector<std::string> v = Fields(line);
        if (section == "txdp") {
            if (v.size() >= 2) {
                f.txdParent[Lower(v[0])] = Lower(v[1]);
            }
            continue;
        }
        if (section == "path" || section == "2dfx" || section == "tanm" || v.size() < 3) {
            continue;
        }
        char* end = nullptr;
        long id = strtol(v[0].c_str(), &end, 10);
        if (*end || id < 0 || id >= gta::MODEL_COUNT) {
            continue;
        }
        IdeDef& d = f.ide[static_cast<int>(id)];
        d.section = section;
        d.dff = v[1];
        d.txd = v[2];
        d.ide = Normalize(relPath);
        ++f.ideModels;
    }
}

// Os .ide que o jogo carrega: as linhas "IDE caminho" do DEFAULT.DAT e do GTA.DAT, na mesma ordem.
void ParseDat(Files& f, const char* relPath, std::vector<std::string>* ides) {
    std::string text;
    if (!ReadFile(f.gameDir + relPath, &text)) {
        DevToolsLog("catalogo: %s nao abriu", relPath);
        return;
    }
    for (const std::string& raw : Lines(text)) {
        std::string line = StripComment(raw);
        if (line.size() > 4 && Lower(line.substr(0, 4)) == "ide ") {
            ides->push_back(Trim(line.substr(4)));
        }
    }
}

void ReadImg(Files& f, int slot) {
    const std::string& rel = f.imgPaths[slot];
    if (rel.empty()) {
        return;
    }
    bool absolute = rel.size() > 1 && rel[1] == ':';
    FILE* file = fopen((absolute ? rel : f.gameDir + rel).c_str(), "rb");
    if (!file) {
        DevToolsLog("catalogo: %s nao abriu", rel.c_str());
        return;
    }
    char magic[4];
    unsigned count = 0;
    if (fread(magic, 1, 4, file) == 4 && memcmp(magic, "VER2", 4) == 0 && fread(&count, 4, 1, file) == 1 &&
        count < 200000) {
        std::vector<BYTE> dir(count * 32);
        if (fread(dir.data(), 32, count, file) == count) {
            for (unsigned i = 0; i < count; ++i) {
                const BYTE* e = &dir[i * 32];
                DWORD offset;
                memcpy(&offset, e, 4);
                char name[25] = {};
                memcpy(name, e + 8, 24);
                f.imgDir[slot][offset] = Lower(name);
            }
        }
    } else {
        DevToolsLog("catalogo: %s nao e um .img VER2", rel.c_str());
    }
    fclose(file);
}

DWORD WINAPI Worker(LPVOID) {
    Files& f = g_files;
    std::vector<std::string> ides;
    ParseDat(f, "data\\default.dat", &ides);
    ParseDat(f, "data\\gta.dat", &ides);
    for (const char* samp : {"SAMP\\SAMP.IDE", "SAMP\\CUSTOM.IDE"}) {
        if (GetFileAttributesA((f.gameDir + samp).c_str()) != INVALID_FILE_ATTRIBUTES) {
            ides.push_back(samp);
        }
    }
    for (const std::string& ide : ides) {
        ParseIde(f, ide);
    }
    for (int i = 0; i < 8; ++i) {
        ReadImg(f, i);
    }
    DevToolsLog("catalogo: %d .ide (%d modelos), %d txdp", f.ideFiles, f.ideModels, static_cast<int>(f.txdParent.size()));
    g_stage = 2;
    return 0;
}

std::string StripExt(const std::string& name) {
    size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string FileName(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    return Lower(slash == std::string::npos ? path : path.substr(slash + 1));
}

int KindOf(const std::string& section, int type) {
    if (section == "cars" || type == gta::MT_VEHICLE) {
        return K_VEHICLE;
    }
    if (section == "peds" || type == gta::MT_PED) {
        return K_PED;
    }
    if (section == "weap" || type == gta::MT_WEAPON) {
        return K_WEAPON;
    }
    return K_OBJECT;
}

void Build() {
    Files& f = g_files;
    g_entries.clear();
    g_byId.clear();
    std::vector<int> archiveOrder;
    for (int id = 0; id < gta::MODEL_COUNT; ++id) {
        DWORD info = gta::ModelInfo(id);
        if (!info) {
            continue;
        }
        Entry e;
        e.id = id;
        e.type = gta::TypeOf(info);
        e.txdIndex = gta::TxdIndex(info);
        e.drawDistance = gta::DrawDistance(info);
        e.flags = gta::Flags(info);
        auto def = f.ide.find(id);
        if (def != f.ide.end()) {
            e.section = def->second.section;
            e.dff = def->second.dff;
            e.txd = def->second.txd;
            e.ide = def->second.ide;
        }
        gta::StreamInfo si;
        if (gta::Streaming(id, &si)) {
            e.noFile = si.size == 0;
            if (!e.noFile && si.img >= 0 && si.img < 8) {
                e.img = si.img;
                e.posn = si.posn;
                e.size = si.size;
                e.imgName = FileName(f.imgPaths[si.img]);
                auto at = f.imgDir[si.img].find(si.posn);
                if (at != f.imgDir[si.img].end()) {
                    e.imgEntry = at->second;
                }
            }
        }
        if (e.dff.empty() && !e.imgEntry.empty()) {
            e.dff = StripExt(e.imgEntry);
        }
        if (!e.noFile && !e.dff.empty()) {
            e.replaced = e.imgEntry.empty() || StripExt(e.imgEntry) != Lower(e.dff);
        }
        if (e.txd.empty() && e.txdIndex >= 0) {
            gta::StreamInfo ti;
            if (gta::Streaming(gta::TXD_FIRST + e.txdIndex, &ti) && ti.size && ti.img >= 0 && ti.img < 8) {
                auto at = f.imgDir[ti.img].find(ti.posn);
                if (at != f.imgDir[ti.img].end()) {
                    e.txd = StripExt(at->second);
                }
            }
        }
        if (!e.txd.empty()) {
            auto parent = f.txdParent.find(Lower(e.txd));
            if (parent != f.txdParent.end()) {
                e.txdParent = parent->second;
            }
        }
        e.kind = KindOf(e.section, e.type);
        char idText[16];
        snprintf(idText, sizeof(idText), "%d", id);
        e.search = Lower(std::string(idText) + " " + e.dff + " " + e.txd + " " + e.imgName);
        if (e.img >= 0 && std::find(archiveOrder.begin(), archiveOrder.end(), e.img) == archiveOrder.end()) {
            archiveOrder.push_back(e.img);
        }
        g_byId[id] = static_cast<int>(g_entries.size());
        g_entries.push_back(std::move(e));
    }
    std::sort(archiveOrder.begin(), archiveOrder.end());
    g_archives.clear();
    for (int slot : archiveOrder) {
        g_archives.push_back(FileName(f.imgPaths[slot]));
    }
    int replaced = 0;
    for (const Entry& e : g_entries) {
        replaced += e.replaced;
    }
    DevToolsLog("catalogo pronto: %d modelos, %d .img, %d substituidos", static_cast<int>(g_entries.size()),
              static_cast<int>(g_archives.size()), replaced);
}

} // namespace

void Start(const char* gameDir) {
    if (g_stage != 0) {
        return;
    }
    g_files.gameDir = gameDir;
    for (int i = 0; i < 8; ++i) {
        g_files.imgPaths[i] = Normalize(gta::ImgPath(i));
    }
    g_stage = 1;
    g_status = "lendo os .ide e os .img";
    HANDLE thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        Worker(nullptr);
    }
}

void Update() {
    if (g_stage == 2) {
        g_status = "montando o catalogo";
        Build();
        g_stage = 3;
        g_status = "pronto";
    }
}

bool Ready() {
    return g_stage == 3;
}

const char* Status() {
    return g_status.c_str();
}

const std::vector<Entry>& All() {
    return g_entries;
}

const Entry* Find(int id) {
    auto it = g_byId.find(id);
    return it == g_byId.end() ? nullptr : &g_entries[it->second];
}

std::vector<int> Filter(const char* query, int kind, int archive, bool onlyReplaced) {
    std::vector<std::string> terms = Fields(Lower(query ? query : ""));
    const std::string* archiveName = archive >= 0 && archive < static_cast<int>(g_archives.size())
                                         ? &g_archives[archive]
                                         : nullptr;
    std::vector<int> out;
    out.reserve(g_entries.size());
    for (int i = 0; i < static_cast<int>(g_entries.size()); ++i) {
        const Entry& e = g_entries[i];
        if ((kind >= 0 && e.kind != kind) || (archiveName && e.imgName != *archiveName) ||
            (onlyReplaced && !e.replaced)) {
            continue;
        }
        bool ok = true;
        for (const std::string& t : terms) {
            if (e.search.find(t) == std::string::npos) {
                ok = false;
                break;
            }
        }
        if (ok) {
            out.push_back(i);
        }
    }
    return out;
}

const std::vector<std::string>& Archives() {
    return g_archives;
}

const char* KindName(int kind) {
    static const char* const names[] = {"Objetos", "Ve\xC3\xAD" "culos", "Skins", "Armas"};
    return kind >= 0 && kind < KIND_COUNT ? names[kind] : "Todos";
}

const char* TypeName(const Entry& e) {
    if (e.section == "tobj" || e.type == gta::MT_TIME) {
        return "Objeto de hor\xC3\xA1rio";
    }
    if (e.section == "anim") {
        return "Objeto animado";
    }
    if (e.section == "hier") {
        return "Objeto de cutscene";
    }
    switch (e.kind) {
    case K_VEHICLE: return "Ve\xC3\xAD" "culo";
    case K_PED: return "Skin";
    case K_WEAPON: return "Arma";
    default: break;
    }
    if (e.type == gta::MT_LOD || (e.flags & 0x80)) {
        return "Objeto (LOD)";
    }
    return "Objeto";
}

} // namespace catalog
