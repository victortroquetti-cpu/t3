// Trok Hitmarker (.asi) -- Victor_Trok
// Trok Hitmarker.ini: [padrao] e uma secao por servidor ("ip:porta"). Ao entrar num servidor, vale o [padrao] com a
// secao dele por cima; cada mudanca e gravada nas duas, entao um servidor novo comeca com as ultimas escolhas.
// Cores em hexadecimal (RRGGBB, ou RRGGBBAA com transparencia); posicoes em fracao da tela.

#include "config.h"

#include "options.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cfg {
namespace {

enum Type { FLAG, NUMBER, COLOR, FRACTION };

struct Key {
    const char* name;
    Type type;
    void* value;
    int lo, hi; // NUMBER
};

opt::Options& O() {
    return opt::Get();
}

#define FLAG_KEY(n, f) {n, FLAG, &O().f, 0, 0}
#define NUM_KEY(n, f, lo, hi) {n, NUMBER, &O().f, lo, hi}
#define COLOR_KEY(n, f) {n, COLOR, &O().f, 0, 0}
#define PLACE_KEY(n, f) {n, FRACTION, &O().f, 0, 0}

const Key* Keys(int* count) {
    static const Key keys[] = {
        FLAG_KEY("ligado", enabled),
        NUM_KEY("estilo_pronto", preset, 0, 1),
        NUM_KEY("mira", crosshair, 0, 1),
        FLAG_KEY("mira_estatica", crosshairStatic),
        FLAG_KEY("mira_sempre_visivel", crosshairAlways),
        NUM_KEY("mira_tamanho", crosshairSize, 50, 200),
        FLAG_KEY("sinal_na_mira", onAim),
        FLAG_KEY("sinal_acerto", fxHit),
        FLAG_KEY("sinal_colete", fxArmour),
        FLAG_KEY("sinal_cabeca", fxHead),
        FLAG_KEY("sinal_abate", fxKill),
        NUM_KEY("numeros", numbers, 0, 2),
        FLAG_KEY("numeros_somar", numbersSum),
        NUM_KEY("numeros_tamanho", numbersSize, 50, 200),
        NUM_KEY("numeros_fonte", numbersFont, 0, 1),
        NUM_KEY("dano_mostrado", source, 0, 1),
        FLAG_KEY("local_do_acerto", hitLocal),
        NUM_KEY("local_tempo", hitLocalTime, 1, 5),
        NUM_KEY("local_tamanho", hitLocalSize, 50, 150),
        NUM_KEY("informer", informer, 0, 2),
        NUM_KEY("nick_dado", nickGiven, 0, 1),
        NUM_KEY("nick_levado", nickTaken, 0, 1),
        FLAG_KEY("afk", afk),
        NUM_KEY("informer_tamanho", informerSize, 50, 200),
        FLAG_KEY("combo", combo),
        NUM_KEY("combo_tempo", comboWindow, 1, 10),
        FLAG_KEY("seta", direction),
        FLAG_KEY("borda", edgeFlash),
        COLOR_KEY("cor_vida", cHealth),
        COLOR_KEY("cor_colete", cArmour),
        COLOR_KEY("cor_cabeca", cHead),
        COLOR_KEY("cor_abate", cKill),
        COLOR_KEY("cor_dado", cGiven),
        COLOR_KEY("cor_levado", cTaken),
        COLOR_KEY("cor_combo", cCombo),
        COLOR_KEY("borda_vida", cEdgeHealth),
        COLOR_KEY("borda_colete", cEdgeArmour),
        COLOR_KEY("borda_cabeca", cEdgeHead),
        FLAG_KEY("som_acerto", sHit),
        FLAG_KEY("som_cabeca", sHeadDiff),
        FLAG_KEY("som_levado", sTaken),
        FLAG_KEY("som_abate", sKill),
        FLAG_KEY("som_abate_cabeca", sKillHead),
        FLAG_KEY("som_morte", sDeath),
        NUM_KEY("volume", volume, 0, 100),
        PLACE_KEY("dado_x", given.x),
        PLACE_KEY("dado_y", given.y),
        PLACE_KEY("levado_x", taken.x),
        PLACE_KEY("levado_y", taken.y),
    };
    *count = static_cast<int>(sizeof(keys) / sizeof(keys[0]));
    return keys;
}

int Clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

// "RRGGBB" ou "RRGGBBAA" (com ou sem #) -> cor do ImGui.
bool ParseColor(const char* s, ImU32* out) {
    if (*s == '#') {
        ++s;
    }
    size_t n = strlen(s);
    if (n != 6 && n != 8) {
        return false;
    }
    char* end = nullptr;
    unsigned long v = strtoul(s, &end, 16);
    if (*end) {
        return false;
    }
    if (n == 6) {
        v = (v << 8) | 0xFF;
    }
    *out = IM_COL32((v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
    return true;
}

// Aceita virgula ou ponto (Windows em portugues).
bool ParseFraction(const char* s, float* out) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", s);
    for (char* p = buf; *p; ++p) {
        if (*p == ',') {
            *p = '.';
        }
    }
    int whole = 0, frac = 0, digits = 0;
    const char* p = buf;
    while (*p >= '0' && *p <= '9') {
        whole = whole * 10 + (*p++ - '0');
    }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9' && digits < 6) {
            frac = frac * 10 + (*p++ - '0');
            ++digits;
        }
    }
    if (p == buf) {
        return false;
    }
    float v = static_cast<float>(whole);
    float scale = 1.0f;
    for (int i = 0; i < digits; ++i) {
        scale *= 10.0f;
    }
    v += frac / scale;
    if (v < 0.0f || v > 1.0f) {
        return false;
    }
    *out = v;
    return true;
}

void ReadSection(const char* path, const char* section) {
    int count = 0;
    const Key* keys = Keys(&count);
    char buf[64];
    for (int i = 0; i < count; ++i) {
        const Key& k = keys[i];
        if (!GetPrivateProfileStringA(section, k.name, "", buf, sizeof(buf), path)) {
            continue;
        }
        switch (k.type) {
        case FLAG: *static_cast<bool*>(k.value) = atoi(buf) != 0; break;
        case NUMBER: *static_cast<int*>(k.value) = Clamp(atoi(buf), k.lo, k.hi); break;
        case COLOR: ParseColor(buf, static_cast<ImU32*>(k.value)); break;
        case FRACTION: ParseFraction(buf, static_cast<float*>(k.value)); break;
        }
    }
}

void WriteSection(const char* path, const char* section) {
    int count = 0;
    const Key* keys = Keys(&count);
    char buf[64];
    for (int i = 0; i < count; ++i) {
        const Key& k = keys[i];
        switch (k.type) {
        case FLAG: snprintf(buf, sizeof(buf), "%d", *static_cast<bool*>(k.value) ? 1 : 0); break;
        case NUMBER: snprintf(buf, sizeof(buf), "%d", *static_cast<int*>(k.value)); break;
        case COLOR: {
            ImU32 c = *static_cast<ImU32*>(k.value);
            unsigned r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF, a = (c >> 24) & 0xFF;
            if (a == 0xFF) {
                snprintf(buf, sizeof(buf), "%02X%02X%02X", r, g, b);
            } else {
                snprintf(buf, sizeof(buf), "%02X%02X%02X%02X", r, g, b, a);
            }
            break;
        }
        case FRACTION: {
            int v = static_cast<int>(*static_cast<float*>(k.value) * 10000.0f + 0.5f);
            snprintf(buf, sizeof(buf), "%d.%04d", v / 10000, v % 10000);
            break;
        }
        }
        WritePrivateProfileStringA(section, k.name, buf, path);
    }
}

} // namespace

void Load(const char* path, const char* server) {
    O() = opt::Defaults();
    if (!path || !path[0]) {
        return;
    }
    ReadSection(path, "padrao");
    if (server && server[0]) {
        ReadSection(path, server);
    }
}

void Save(const char* path, const char* server) {
    if (!path || !path[0]) {
        return;
    }
    WriteSection(path, "padrao");
    if (server && server[0]) {
        WriteSection(path, server);
    }
}

} // namespace cfg
