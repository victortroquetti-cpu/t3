// Trok Chat (.asi) -- Victor_Trok
// O "Trok Chat.ini", ao lado do .asi:
//   [TROK_CHAT]
//   Etiquetas=1         [conteudo] vira etiqueta
//   Ligado=1            0 = o chat do SA-MP de volta
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
    const bool saved = Has("Linhas") || Has("Hora") || Has("ModoF7");
    if (saved) {
        chat->pageSize = Int("Linhas", chat->pageSize);
        chat->timestamps = Int("Hora", chat->timestamps) != 0;
        chat->mode = Int("ModoF7", chat->mode);
    }
    Log("opcoes: etiquetas %s, %s%s", o.tags ? "sim" : "nao", o.enabled ? "ligado" : "DESLIGADO (chat do SA-MP)",
        GetFileAttributesA(g_path) == INVALID_FILE_ATTRIBUTES ? " (sem Trok Chat.ini: padroes)" : "");
    return saved;
}

bool Save(const host::ChatSettings& chat) {
    const opt::Options& o = opt::Get();
    bool ok = WriteInt("Etiquetas", o.tags);
    ok = WriteInt("Ligado", o.enabled) && ok;
    ok = WriteInt("Linhas", chat.pageSize) && ok;
    ok = WriteInt("Hora", chat.timestamps) && ok;
    ok = WriteInt("ModoF7", chat.mode) && ok;
    Log(ok ? "Trok Chat.ini gravado" : "Trok Chat.ini: nao deu para gravar");
    return ok;
}

} // namespace ini
