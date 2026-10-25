// Trok DevTools (.asi) -- Victor_Trok
// Formato (gta-reversed, CAnimManager::LoadAnimFile, e os KeyFrame* do AnimSequenceFrames.h):
//   ANP3/ANP2: "ANP3", tamanho, bloco[24], quantas animacoes; cada animacao: nome[24], quantos ossos, (so no ANP3: bytes
//   dos quadros, flags: 1 = comprimida); cada osso: nome[24], tipo, quantos quadros, etiqueta do osso (-1 = pelo nome) e
//   os quadros: 1 = rotacao (float x4) + tempo, 2 = rotacao + tempo + translacao (float x3), 3 = rotacao (int16 x4 / 4096)
//   + tempo (int16 / 60), 4 = o 3 + translacao (int16 x3 / 1024). O tempo e desde o comeco da animacao.
//   ANPK (o formato do GTA3/VC): blocos "INFO", "NAME", "DGAN", "CPAN", "ANIM" e "KR00"/"KRT0"/"KRTS" (floats: rotacao,
//   translacao, escala, tempo); a rotacao vem conjugada (o jogo inverte na leitura).
//   anim.img: VER2 (32 bytes por entrada: inicio e tamanho em setores de 2048, nome[24]).

#include "ifp.h"

#include "anim_names.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

void DevToolsLog(const char* fmt, ...);

namespace ifp {
namespace {

constexpr DWORD SECTOR = 2048;
constexpr DWORD MAX_FILE = 64u << 20;
constexpr DWORD MAX_ANIMS = 4096, MAX_TRACKS = 128, MAX_FRAMES = 50000;
enum Format { F_ANPK = 0, F_ANP2 = 2, F_ANP3 = 3 };

std::atomic<int> g_stage{0}; // 0 parado, 1 lendo, 2 pronto
std::string g_status = "parado";
std::string g_gameDir, g_reference;
std::vector<File> g_files;
std::vector<Anim> g_anims;
int g_mods = 0, g_news = 0;

std::string Upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// "bar.ifp" -> "BAR"
std::string LibOf(const std::string& entry) {
    size_t dot = entry.rfind('.');
    return Upper(dot == std::string::npos ? entry : entry.substr(0, dot));
}

DWORD Fnv(const BYTE* p, size_t n) {
    DWORD h = 2166136261u;
    for (size_t i = 0; i < n; ++i) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

bool ReadFile(const std::string& path, DWORD offset, DWORD size, std::vector<BYTE>* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    if (size == 0) {
        fseek(f, 0, SEEK_END);
        long end = ftell(f);
        size = end > 0 ? static_cast<DWORD>(std::min<long>(end, MAX_FILE)) : 0;
    }
    out->resize(size);
    bool ok = size > 0 && fseek(f, static_cast<long>(offset), SEEK_SET) == 0 && fread(out->data(), 1, size, f) == size;
    fclose(f);
    return ok;
}

struct Reader {
    const BYTE* p;
    size_t size, pos = 0;
    bool ok = true;
    bool Need(size_t n) {
        if (pos + n > size) {
            ok = false;
        }
        return ok;
    }
    template <class T>
    T Get() {
        T v{};
        if (Need(sizeof(T))) {
            memcpy(&v, p + pos, sizeof(T));
            pos += sizeof(T);
        }
        return v;
    }
    std::string Name(size_t n) {
        if (!Need(n)) {
            return std::string();
        }
        std::string s(reinterpret_cast<const char*>(p + pos), strnlen(reinterpret_cast<const char*>(p + pos), n));
        pos += n;
        return s;
    }
    void Skip(size_t n) {
        if (Need(n)) {
            pos += n;
        }
    }
    bool Chunk(const char* id, DWORD* size) { // um bloco do ANPK: 4 letras e o tamanho
        if (!Need(8) || memcmp(p + pos, id, 4) != 0) {
            ok = false;
            return false;
        }
        pos += 4;
        *size = Get<DWORD>();
        return ok;
    }
};

DWORD Round4(DWORD n) {
    return (n + 3) & ~3u;
}

struct Parsed {
    std::string name;
    size_t begin = 0, end = 0;
    int tracks = 0, frames = 0;
    float seconds = 0;
    bool moves = false, compressed = true;
};

// O fim de um osso: duracao, quadros e se a raiz anda.
void Finish(const Track& t, Parsed* out) {
    if (t.keys.empty()) {
        return;
    }
    out->seconds = std::max(out->seconds, t.keys.back().time);
    out->frames = std::max(out->frames, static_cast<int>(t.keys.size()));
    std::string n = Lower(t.name);
    if (t.translation && (t.bone == 0 || n == "root" || n == "normal")) {
        float dx = t.keys.back().t[0] - t.keys.front().t[0], dy = t.keys.back().t[1] - t.keys.front().t[1];
        out->moves = out->moves || dx * dx + dy * dy > 0.05f * 0.05f;
    }
}

// Uma animacao, a partir de onde o leitor esta. clip = nullptr: so as contas.
bool ParseAnim(Reader& r, int format, Parsed* out, Clip* clip) {
    out->begin = r.pos;
    Track track;
    if (format == F_ANPK) {
        DWORD size = 0;
        if (!r.Chunk("NAME", &size)) {
            return false;
        }
        out->name = r.Name(Round4(size));
        DWORD dgan = 0, info = 0;
        if (!r.Chunk("DGAN", &dgan) || !r.Chunk("INFO", &info)) {
            return false;
        }
        DWORD count = r.Get<DWORD>();
        r.Skip(Round4(info) >= 4 ? Round4(info) - 4 : 0);
        if (!r.ok || count > MAX_TRACKS) {
            return false;
        }
        out->compressed = false;
        for (DWORD s = 0; s < count && r.ok; ++s) {
            DWORD cpan = 0, anim = 0;
            if (!r.Chunk("CPAN", &cpan) || !r.Chunk("ANIM", &anim) || anim < 32) {
                return false;
            }
            track = Track();
            track.name = r.Name(28);
            DWORD frames = r.Get<DWORD>();
            r.Skip(Round4(anim) - 32);
            if (frames > MAX_FRAMES) {
                return false;
            }
            if (frames > 0) {
                if (!r.Need(8)) {
                    return false;
                }
                char id[4];
                memcpy(id, r.p + r.pos, 4);
                r.pos += 4;
                r.Get<DWORD>();
                if (id[0] != 'K' || id[1] != 'R') {
                    return false;
                }
                bool hasT = id[2] == 'T', hasS = id[3] == 'S';
                track.translation = hasT;
                for (DWORD k = 0; k < frames && r.ok; ++k) {
                    Key key = {};
                    float q[4];
                    for (float& v : q) {
                        v = r.Get<float>();
                    }
                    key.q[0] = -q[0]; // conjugada, como o jogo faz
                    key.q[1] = -q[1];
                    key.q[2] = -q[2];
                    key.q[3] = q[3];
                    if (hasT) {
                        for (float& v : key.t) {
                            v = r.Get<float>();
                        }
                    }
                    if (hasS) {
                        r.Skip(12);
                    }
                    key.time = r.Get<float>();
                    track.keys.push_back(key);
                }
            }
            Finish(track, out);
            ++out->tracks;
            if (clip) {
                clip->tracks.push_back(track);
            }
        }
    } else {
        out->name = r.Name(24);
        DWORD count = r.Get<DWORD>();
        if (format == F_ANP3) {
            r.Get<DWORD>(); // bytes dos quadros
            out->compressed = (r.Get<DWORD>() & 1) != 0;
        }
        if (!r.ok || count > MAX_TRACKS) {
            return false;
        }
        for (DWORD s = 0; s < count && r.ok; ++s) {
            track = Track();
            track.name = r.Name(24);
            DWORD type = r.Get<DWORD>(), frames = r.Get<DWORD>();
            track.bone = r.Get<int>();
            if (type < 1 || type > 4 || frames > MAX_FRAMES) {
                return false;
            }
            track.translation = type == 2 || type == 4;
            for (DWORD k = 0; k < frames && r.ok; ++k) {
                Key key = {};
                if (type <= 2) {
                    for (float& v : key.q) {
                        v = r.Get<float>();
                    }
                    key.time = r.Get<float>();
                    if (type == 2) {
                        for (float& v : key.t) {
                            v = r.Get<float>();
                        }
                    }
                } else {
                    for (float& v : key.q) {
                        v = r.Get<short>() / 4096.0f;
                    }
                    key.time = r.Get<short>() / 60.0f;
                    if (type == 4) {
                        for (float& v : key.t) {
                            v = r.Get<short>() / 1024.0f;
                        }
                    }
                }
                track.keys.push_back(key);
            }
            Finish(track, out);
            ++out->tracks;
            if (clip) {
                clip->tracks.push_back(track);
            }
        }
    }
    out->end = r.pos;
    return r.ok;
}

// Um .ifp inteiro: o formato, o nome interno e as animacoes (ate onde der para ler).
bool ParseFile(const BYTE* data, size_t size, int* format, std::string* block, std::vector<Parsed>* anims) {
    Reader r{data, size};
    if (size < 8) {
        return false;
    }
    if (memcmp(data, "ANP3", 4) == 0 || memcmp(data, "ANP2", 4) == 0) {
        *format = data[3] == '3' ? F_ANP3 : F_ANP2;
        r.Skip(8);
        *block = r.Name(24);
        DWORD count = r.Get<DWORD>();
        if (!r.ok || count > MAX_ANIMS) {
            return false;
        }
        for (DWORD i = 0; i < count; ++i) {
            Parsed p;
            if (!ParseAnim(r, *format, &p, nullptr)) {
                return false;
            }
            anims->push_back(p);
        }
        return true;
    }
    if (memcmp(data, "ANPK", 4) == 0) {
        *format = F_ANPK;
        r.Skip(8);
        DWORD info = 0;
        if (!r.Chunk("INFO", &info) || info < 4) {
            return false;
        }
        DWORD count = r.Get<DWORD>();
        *block = r.Name(Round4(info) - 4);
        if (!r.ok || count > MAX_ANIMS) {
            return false;
        }
        for (DWORD i = 0; i < count; ++i) {
            Parsed p;
            if (!ParseAnim(r, F_ANPK, &p, nullptr)) {
                return false;
            }
            anims->push_back(p);
        }
        return true;
    }
    return false;
}

const char* FormatName(int f) {
    return f == F_ANP3 ? "ANP3" : f == F_ANP2 ? "ANP2" : "ANPK";
}

// Um arquivo do jogo para ler: o ped.ifp sozinho ou uma entrada do anim.img.
struct Source {
    std::string path, shortPath, entry;
    DWORD offset = 0, size = 0; // 0 = o arquivo inteiro
};

std::vector<Source> Sources(const std::string& dir, const std::string& shortDir) {
    std::vector<Source> out;
    if (GetFileAttributesA((dir + "ped.ifp").c_str()) != INVALID_FILE_ATTRIBUTES) {
        out.push_back({dir + "ped.ifp", shortDir + "ped.ifp", "ped.ifp", 0, 0});
    }
    std::string img = dir + "anim.img";
    FILE* f = fopen(img.c_str(), "rb");
    if (!f) {
        return out;
    }
    char head[8];
    if (fread(head, 1, 8, f) == 8 && memcmp(head, "VER2", 4) == 0) {
        DWORD count = *reinterpret_cast<DWORD*>(head + 4);
        for (DWORD i = 0; i < count && i < 4096; ++i) {
            BYTE e[32];
            if (fread(e, 1, 32, f) != 32) {
                break;
            }
            DWORD posn = *reinterpret_cast<DWORD*>(e), sectors = *reinterpret_cast<WORD*>(e + 4);
            if (sectors == 0) {
                sectors = *reinterpret_cast<WORD*>(e + 6);
            }
            std::string name(reinterpret_cast<char*>(e + 8), strnlen(reinterpret_cast<char*>(e + 8), 24));
            if (Lower(name).find(".ifp") != std::string::npos && sectors > 0) {
                out.push_back({img, shortDir + "anim.img", Lower(name), posn * SECTOR, sectors * SECTOR});
            }
        }
    }
    fclose(f);
    return out;
}

// ---------------------------------------------------------------- referencia (as originais)

std::unordered_map<std::string, DWORD> g_ref; // "PED:WALK_CIVI" -> impressao digital
std::unordered_set<std::string> g_refLibs;

bool ReferenceFromFolder() {
    std::string dir = g_gameDir + "Trok DevTools\\originais\\";
    std::vector<Source> sources = Sources(dir, "originais\\");
    if (sources.empty()) {
        return false;
    }
    std::vector<BYTE> data;
    for (const Source& s : sources) {
        int format = 0;
        std::string block;
        std::vector<Parsed> anims;
        if (!ReadFile(s.path, s.offset, s.size, &data)) {
            continue;
        }
        ParseFile(data.data(), data.size(), &format, &block, &anims);
        std::string lib = LibOf(s.entry);
        g_refLibs.insert(lib);
        for (const Parsed& p : anims) {
            g_ref[lib + ":" + Upper(p.name)] = Fnv(data.data() + p.begin, p.end - p.begin);
        }
    }
    // Guarda as impressoes num .txt: da para levar so ele para outro jogo (ou mandar para embutir no mod)
    std::string out = g_gameDir + "Trok DevTools\\animacoes_originais.txt";
    if (FILE* f = fopen(out.c_str(), "wb")) {
        fprintf(f, "# Trok DevTools: impressoes digitais das animacoes originais (%d), de Trok DevTools\\originais\r\n",
                static_cast<int>(g_ref.size()));
        std::vector<std::pair<std::string, DWORD>> rows(g_ref.begin(), g_ref.end());
        std::sort(rows.begin(), rows.end());
        for (const auto& r : rows) {
            fprintf(f, "%s %08lX\r\n", r.first.c_str(), static_cast<unsigned long>(r.second));
        }
        fclose(f);
    }
    g_reference = "a pasta Trok DevTools\\originais";
    return !g_ref.empty();
}

bool ReferenceFromText() {
    FILE* f = fopen((g_gameDir + "Trok DevTools\\animacoes_originais.txt").c_str(), "rb");
    if (!f) {
        return false;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[200];
        unsigned long hash = 0;
        if (line[0] == '#' || sscanf(line, "%199s %lx", key, &hash) != 2) {
            continue;
        }
        std::string k = Upper(key);
        size_t colon = k.find(':');
        if (colon != std::string::npos) {
            g_refLibs.insert(k.substr(0, colon));
            g_ref[k] = static_cast<DWORD>(hash);
        }
    }
    fclose(f);
    g_reference = "o Trok DevTools\\animacoes_originais.txt";
    return !g_ref.empty();
}

// ---------------------------------------------------------------- leitura

DWORD WINAPI Worker(void*) {
    std::unordered_map<std::string, int> names; // "PED:WALK_CIVI" -> indice do SA-MP
    std::unordered_map<std::string, std::vector<std::string>> libNames;
    for (const anim_names::Name& n : anim_names::NAMES) {
        names[std::string(n.lib) + ":" + Upper(n.name)] = n.sampId;
        libNames[n.lib].push_back(n.name);
    }
    std::unordered_set<std::string> libs(std::begin(anim_names::LIBS), std::end(anim_names::LIBS));

    g_status = "lendo as anima\xC3\xA7\xC3\xB5" "es originais";
    if (!ReferenceFromFolder()) {
        g_ref.clear();
        g_refLibs.clear();
        if (!ReferenceFromText()) {
            g_ref.clear();
            g_refLibs.clear();
            g_reference.clear();
        }
    }

    g_status = "lendo anim\\anim.img e anim\\ped.ifp";
    std::vector<BYTE> data;
    for (const Source& s : Sources(g_gameDir + "anim\\", "anim\\")) {
        if (!ReadFile(s.path, s.offset, s.size, &data)) {
            continue;
        }
        File file;
        file.entry = s.entry;
        file.lib = LibOf(s.entry);
        file.source = s.shortPath;
        file.offset = s.offset;
        file.bytes = static_cast<DWORD>(data.size());
        int format = F_ANP3;
        std::vector<Parsed> parsed;
        file.broken = !ParseFile(data.data(), data.size(), &format, &file.block, &parsed);
        file.format = FormatName(format);
        file.known = libs.count(file.lib) > 0;
        file.renamed = !file.block.empty() && Upper(file.block) != file.lib;
        int fileIndex = static_cast<int>(g_files.size());
        bool hasRef = !g_ref.empty();
        std::unordered_set<std::string> present;
        for (const Parsed& p : parsed) {
            Anim a;
            a.index = static_cast<int>(g_anims.size());
            a.file = fileIndex;
            a.name = p.name;
            a.seconds = p.seconds;
            a.tracks = p.tracks;
            a.frames = p.frames;
            a.moves = p.moves;
            a.compressed = p.compressed;
            a.offset = s.offset + static_cast<DWORD>(p.begin);
            a.size = static_cast<DWORD>(p.end - p.begin);
            a.hash = Fnv(data.data() + p.begin, p.end - p.begin);
            std::string key = file.lib + ":" + Upper(p.name);
            present.insert(Upper(p.name));
            auto n = names.find(key);
            a.sampId = n != names.end() ? n->second : -1;
            if (hasRef) {
                auto r = g_ref.find(key);
                a.tag = r == g_ref.end() ? TAG_NEW : r->second != a.hash ? TAG_MOD : TAG_NONE;
            } else if (!file.known || (libNames.count(file.lib) && n == names.end())) {
                a.tag = TAG_NEW;
            }
            file.mods += a.tag == TAG_MOD;
            file.news += a.tag == TAG_NEW;
            a.search = Lower(file.lib + " " + p.name + (a.tag == TAG_MOD ? " mod modificada" : "") +
                             (a.tag == TAG_NEW ? " nova" : ""));
            file.anims.push_back(a.index);
            g_anims.push_back(a);
        }
        // As originais que faltam no arquivo (pela referencia, ou pela lista do jogo)
        if (hasRef) {
            std::string prefix = file.lib + ":";
            for (const auto& r : g_ref) {
                if (r.first.compare(0, prefix.size(), prefix) == 0 && !present.count(r.first.substr(prefix.size()))) {
                    file.missing.push_back(r.first.substr(prefix.size()));
                }
            }
            std::sort(file.missing.begin(), file.missing.end());
        } else if (libNames.count(file.lib)) {
            for (const std::string& n : libNames[file.lib]) {
                if (!present.count(Upper(n))) {
                    file.missing.push_back(n);
                }
            }
        }
        file.search = Lower(file.lib + " " + file.entry + " " + file.block + (file.renamed ? " nome interno" : ""));
        g_mods += file.mods;
        g_news += file.news;
        g_files.push_back(file);
        if (file.broken) {
            DevToolsLog("ifp: %s nao foi lido ate o fim (%d animacoes lidas)", file.entry.c_str(),
                        static_cast<int>(file.anims.size()));
        }
        if (file.renamed) {
            DevToolsLog("ifp: %s tem o nome interno \"%s\"", file.entry.c_str(), file.block.c_str());
        }
    }
    DevToolsLog("ifp: %d arquivos, %d animacoes (%d modificadas, %d novas; referencia: %s)", static_cast<int>(g_files.size()),
                static_cast<int>(g_anims.size()), g_mods, g_news, g_reference.empty() ? "nenhuma" : g_reference.c_str());
    g_status = "pronto";
    g_stage = 2;
    return 0;
}

} // namespace

void Start(const char* gameDir) {
    if (g_stage != 0) {
        return;
    }
    g_gameDir = gameDir ? gameDir : "";
    g_stage = 1;
    g_status = "lendo as anima\xC3\xA7\xC3\xB5" "es";
    HANDLE thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        Worker(nullptr);
    }
}

bool Ready() {
    return g_stage == 2;
}

const char* Status() {
    return g_status.c_str();
}

const std::vector<File>& Files() {
    return g_files;
}

const std::vector<Anim>& Anims() {
    return g_anims;
}

const File* FindFile(int index) {
    return index >= 0 && index < static_cast<int>(g_files.size()) ? &g_files[index] : nullptr;
}

const Anim* FindAnim(int index) {
    return index >= 0 && index < static_cast<int>(g_anims.size()) ? &g_anims[index] : nullptr;
}

int ModCount() {
    return g_mods;
}

int NewCount() {
    return g_news;
}

const char* Reference() {
    return g_reference.c_str();
}

Result Apply(const Filter& f) {
    Result r;
    r.anims.assign(g_files.size(), {});
    std::vector<std::string> terms;
    std::string q = Lower(f.query);
    for (size_t i = 0; i <= q.size();) {
        size_t j = q.find(' ', i);
        if (j == std::string::npos) {
            j = q.size();
        }
        if (j > i) {
            terms.push_back(q.substr(i, j - i));
        }
        i = j + 1;
    }
    for (size_t fi = 0; fi < g_files.size(); ++fi) {
        const File& file = g_files[fi];
        if (f.file >= 0 && static_cast<int>(fi) != f.file) {
            continue;
        }
        std::vector<int>& list = r.anims[fi];
        for (int ai : file.anims) {
            const Anim& a = g_anims[ai];
            if (f.changed && a.tag == TAG_NONE) {
                continue;
            }
            bool ok = true;
            for (const std::string& t : terms) {
                ok = ok && (file.search.find(t) != std::string::npos || a.search.find(t) != std::string::npos);
            }
            if (ok) {
                list.push_back(ai);
            }
        }
        if (!list.empty()) {
            ++r.files;
            r.total += static_cast<int>(list.size());
        }
    }
    return r;
}

bool LoadClip(int anim, Clip* out) {
    *out = Clip();
    const Anim* a = FindAnim(anim);
    const File* file = a ? FindFile(a->file) : nullptr;
    if (!a || !file || a->size == 0) {
        return false;
    }
    std::vector<BYTE> data;
    if (!ReadFile(g_gameDir + file->source, a->offset, a->size, &data)) {
        return false;
    }
    int format = file->format == "ANPK" ? F_ANPK : file->format == "ANP2" ? F_ANP2 : F_ANP3;
    Reader r{data.data(), data.size()};
    Parsed p;
    if (!ParseAnim(r, format, &p, out)) {
        out->tracks.clear();
        return false;
    }
    out->anim = anim;
    out->seconds = p.seconds;
    return true;
}

} // namespace ifp
