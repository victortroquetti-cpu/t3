// Trok Chat (.asi) -- Victor_Trok
// O "Trok Chat.ini", ao lado do .asi:
//   [TROK_CHAT]
//   Etiquetas=1         [conteudo] vira etiqueta
//   Ligado=1            0 = o chat do SA-MP de volta
//   PosicaoX=-1         canto de cima a esquerda do chat em fracao da tela (0.5 = no meio); -1 = o canto de sempre
//   PosicaoY=-1
//   Linhas=10           copia do /pagesize, do /timestamp e do F7 (0 escondido, 1 sem contorno, 2 com contorno):
//   Hora=0              o SA-MP nao guarda o F7, e o menu muda os tres; valem de novo quando o jogo abre
//   ModoF7=2

#include "tc.h"
#include "chat.h"
#include "options.h"

#include <cstdio>

namespace ini {
namespace {

char g_path[MAX_PATH] = "";
const char* const SECTION = "TROK_CHAT";

bool Has(const char* key) {
    char value[64];
    return GetPrivateProfileStringA(SECTION, key, "\x01", value, sizeof(value), g_path) > 0 && value[0] != '\x01';
}

int Int(const char* key, int def) {
    return static_cast<int>(GetPrivateProfileIntA(SECTION, key, def, g_path));
}

// Fracao da tela (0 a 1), com ponto ou virgula. Sem atof/printf de float: o separador decimal que algum mod ponha no
// processo nao entra na conta. Fora da tela (ou -1): def.
float Fraction(const char* key, float def) {
    char value[32];
    if (GetPrivateProfileStringA(SECTION, key, "", value, sizeof(value), g_path) == 0) {
        return def;
    }
    const char* p = value;
    while (*p == ' ') {
        p++;
    }
    double v = 0;
    bool digits = false;
    for (; *p >= '0' && *p <= '9'; p++) {
        v = v * 10 + (*p - '0');
        digits = true;
    }
    if (*p == '.' || *p == ',') {
        double scale = 0.1;
        for (p++; *p >= '0' && *p <= '9'; p++) {
            v += (*p - '0') * scale;
            scale *= 0.1;
            digits = true;
        }
    }
    return digits && v <= 1.0 ? static_cast<float>(v) : def;
}

bool WriteFraction(const char* key, float value) {
    char text[32];
    if (value < 0) {
        snprintf(text, sizeof(text), "-1");
    } else {
        const int tenThousandths = static_cast<int>(value * 10000.0f + 0.5f);
        snprintf(text, sizeof(text), "%d.%04d", tenThousandths / 10000, tenThousandths % 10000);
    }
    return WritePrivateProfileStringA(SECTION, key, text, g_path) != FALSE;
}

bool WriteInt(const char* key, int value) {
    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    return WritePrivateProfileStringA(SECTION, key, text, g_path) != FALSE;
}

} // namespace

void SetPath(const char* path) {
    lstrcpynA(g_path, path, MAX_PATH);
}

// As opcoes vao para opt::Get(); devolve true se havia a copia das do SA-MP (em chat).
bool Load(host::ChatSettings* chat) {
    opt::Options& o = opt::Get();
    const opt::Options& d = opt::Defaults();
    o.tags = Int("Etiquetas", d.tags) != 0;
    o.enabled = Int("Ligado", d.enabled) != 0;
    o.posX = Fraction("PosicaoX", d.posX);
    o.posY = Fraction("PosicaoY", d.posY);
    if (o.posX < 0 || o.posY < 0) {
        o.posX = o.posY = -1.0f;
    }
    const bool saved = Has("Linhas") || Has("Hora") || Has("ModoF7");
    if (saved) {
        chat->pageSize = Int("Linhas", chat->pageSize);
        chat->timestamps = Int("Hora", chat->timestamps) != 0;
        chat->mode = Int("ModoF7", chat->mode);
    }
    char where[48] = "no canto de sempre";
    if (o.posX >= 0) {
        snprintf(where, sizeof(where), "em %d%% x %d%% da tela", static_cast<int>(o.posX * 100 + 0.5f),
                 static_cast<int>(o.posY * 100 + 0.5f));
    }
    Log("opcoes: etiquetas %s, %s, chat %s%s", o.tags ? "sim" : "nao", o.enabled ? "ligado" : "DESLIGADO (chat do SA-MP)",
        where, GetFileAttributesA(g_path) == INVALID_FILE_ATTRIBUTES ? " (sem Trok Chat.ini: padroes)" : "");
    return saved;
}

bool Save(const host::ChatSettings& chat) {
    const opt::Options& o = opt::Get();
    bool ok = WriteInt("Etiquetas", o.tags);
    ok = WriteInt("Ligado", o.enabled) && ok;
    ok = WriteFraction("PosicaoX", o.posX) && ok;
    ok = WriteFraction("PosicaoY", o.posY) && ok;
    ok = WriteInt("Linhas", chat.pageSize) && ok;
    ok = WriteInt("Hora", chat.timestamps) && ok;
    ok = WriteInt("ModoF7", chat.mode) && ok;
    Log(ok ? "Trok Chat.ini gravado" : "Trok Chat.ini: nao deu para gravar");
    return ok;
}

} // namespace ini
