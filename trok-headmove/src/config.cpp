// Trok Headmove (.asi) -- Victor_Trok
// Trok Headmove.ini: uma secao [headmove]. Sem o arquivo, o mod cria um com os padroes e uma linha de explicacao em
// cada chave. Valores fora da faixa sao trazidos para a borda; numero com virgula tambem vale (Windows em portugues).

#include "config.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cfg {
namespace {

constexpr const char* SECTION = "headmove";

Options g_options;
char g_path[MAX_PATH] = "";
FILETIME g_stamp = {};
bool g_haveStamp = false;

const char DEFAULT_FILE[] =
    "; Trok Headmove -- a cabeca acompanhando para onde voce olha (o /headmove do SA-MP), sem o atraso do SA-MP.\r\n"
    "; Mudou aqui? Vale na hora, com o jogo aberto: o mod confere este arquivo a cada segundo.\r\n"
    "[headmove]\r\n"
    "; 1 = ligado. 0 = a cabeca volta ao jeito do SA-MP.\r\n"
    "ligado=1\r\n"
    "; Seu personagem: quanto tempo (em ms) a cabeca leva para chegar onde a camera olha. 0 = na hora.\r\n"
    "; (O SA-MP sozinho so muda a direcao da cabeca uma vez por segundo.)\r\n"
    "atraso_ms=0\r\n"
    "; Os outros jogadores, como VOCE os ve: 1 = o mod cuida deles tambem, 0 = como o SA-MP faz.\r\n"
    "outros_jogadores=1\r\n"
    "; Atraso deles (ms). A direcao deles chega pela rede aos pedacos: um pouco de atraso deixa o giro liso.\r\n"
    "atraso_outros_ms=100\r\n"
    "; Giro do GTA por quadro, de 0.1 (o do SA-MP, lento) a 1.0 (na hora). Com atraso 0, deixe 1.0.\r\n"
    "giro=1.0\r\n";

int Clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

bool ReadText(const char* key, char* out, int size) {
    return GetPrivateProfileStringA(SECTION, key, "", out, size, g_path) > 0;
}

void ReadFlag(const char* key, bool* value) {
    char buf[32];
    if (ReadText(key, buf, sizeof(buf))) {
        *value = atoi(buf) != 0;
    }
}

void ReadInt(const char* key, int* value, int lo, int hi) {
    char buf[32];
    if (ReadText(key, buf, sizeof(buf))) {
        *value = Clamp(atoi(buf), lo, hi);
    }
}

// "1.0", "1,0", "0.25", "1": sem depender da localidade do Windows.
void ReadFloat(const char* key, float* value, float lo, float hi) {
    char buf[32];
    if (!ReadText(key, buf, sizeof(buf))) {
        return;
    }
    const char* p = buf;
    while (*p == ' ') {
        ++p;
    }
    if ((*p < '0' || *p > '9') && *p != '.' && *p != ',') {
        return;
    }
    float v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p++ - '0');
    }
    if (*p == '.' || *p == ',') {
        ++p;
        float scale = 0.1f;
        while (*p >= '0' && *p <= '9' && scale > 1e-6f) {
            v += (*p++ - '0') * scale;
            scale *= 0.1f;
        }
    }
    *value = v < lo ? lo : v > hi ? hi : v;
}

bool Stamp(FILETIME* out) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(g_path, GetFileExInfoStandard, &data)) {
        return false;
    }
    *out = data.ftLastWriteTime;
    return true;
}

void Read() {
    Options o; // um arquivo sem a chave volta ao padrao dela
    ReadFlag("ligado", &o.enabled);
    ReadInt("atraso_ms", &o.delayMs, 0, 3000);
    ReadFlag("outros_jogadores", &o.others);
    ReadInt("atraso_outros_ms", &o.othersDelayMs, 0, 3000);
    ReadFloat("giro", &o.turn, 0.05f, 1.0f);
    g_options = o;
}

void WriteDefaults() {
    HANDLE file = CreateFileA(g_path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, DEFAULT_FILE, sizeof(DEFAULT_FILE) - 1, &written, nullptr);
    CloseHandle(file);
}

} // namespace

const Options& Get() {
    return g_options;
}

void Init(const char* path) {
    snprintf(g_path, sizeof(g_path), "%s", path ? path : "");
    g_options = Options();
    if (!g_path[0]) {
        return;
    }
    if (GetFileAttributesA(g_path) == INVALID_FILE_ATTRIBUTES) {
        WriteDefaults();
    }
    g_haveStamp = Stamp(&g_stamp);
    Read();
}

bool Reload() {
    if (!g_path[0]) {
        return false;
    }
    FILETIME now;
    if (!Stamp(&now)) {
        return false; // apagado: fica com o que ja tinha
    }
    if (g_haveStamp && CompareFileTime(&now, &g_stamp) == 0) {
        return false;
    }
    g_stamp = now;
    g_haveStamp = true;
    Read();
    return true;
}

void Describe(char* out, int size) {
    const Options& o = g_options;
    if (!o.enabled) {
        snprintf(out, size, "desligado (a cabeca fica como o SA-MP faz)");
        return;
    }
    char others[48];
    if (o.others) {
        snprintf(others, sizeof(others), "outros %d ms", o.othersDelayMs);
    } else {
        snprintf(others, sizeof(others), "outros como o SA-MP");
    }
    snprintf(out, size, "atraso %d ms, %s, giro %.2f", o.delayMs, others, o.turn);
}

} // namespace cfg
