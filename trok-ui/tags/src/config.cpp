// Trok Tags -- Trok Tags.ini: [padrao] e uma secao por servidor ("ip:porta"). Ao entrar num servidor, vale o
// [padrao] com a secao dele por cima; cada mudanca e gravada nas duas, entao um servidor novo comeca com as
// ultimas escolhas.

#include "config.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>

namespace tags {

Config cfg;

namespace {

struct Key {
    const char* name;
    bool* flag;
    int* number;
};

const Key KEYS[] = {
    {"ligado", &cfg.enabled, nullptr},       {"estilo", nullptr, &cfg.style},
    {"cor", nullptr, &cfg.colorMode},        {"id", &cfg.showId, nullptr},
    {"barras", &cfg.bars, nullptr},          {"etiqueta_nome", &cfg.nameTags, nullptr},
    {"tamanho", nullptr, &cfg.size},         {"distancia", nullptr, &cfg.distance},
    {"desbotar", &cfg.fade, nullptr},        {"baloes", &cfg.bubbles, nullptr},
    {"largura_balao", nullptr, &cfg.bubbleWidth}, {"etiqueta_balao", &cfg.bubbleTags, nullptr},
    {"ooc", &cfg.ooc, nullptr},              {"opacidade_balao", nullptr, &cfg.bubbleOpacity},
    {"fixos", &cfg.fixed, nullptr},          {"etiqueta_fixo", &cfg.fixedTags, nullptr},
    {"cor_etiqueta_fixo", nullptr, &cfg.fixedChip}, {"cor_texto_fixo", nullptr, &cfg.fixedText},
    {"afk", &cfg.afk, nullptr},              {"afk_tempo", &cfg.afkTime, nullptr},
    {"digitando", &cfg.typing, nullptr},     {"ping", &cfg.ping, nullptr},
    {"ping_limite", nullptr, &cfg.pingLimit}, {"vida_baixa", &cfg.lowHealth, nullptr},
};

int Clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

void ReadSection(const char* path, const char* section) {
    char buf[32];
    for (const Key& k : KEYS) {
        DWORD n = GetPrivateProfileStringA(section, k.name, "", buf, sizeof(buf), path);
        if (!n) {
            continue;
        }
        int v = atoi(buf);
        if (k.flag) {
            *k.flag = v != 0;
        } else {
            *k.number = v;
        }
    }
}

void WriteSection(const char* path, const char* section) {
    char buf[32];
    for (const Key& k : KEYS) {
        snprintf(buf, sizeof(buf), "%d", k.flag ? (*k.flag ? 1 : 0) : *k.number);
        WritePrivateProfileStringA(section, k.name, buf, path);
    }
}

} // namespace

void LoadConfig(const char* path, const char* server) {
    cfg = Config();
    ReadSection(path, "padrao");
    if (server && server[0]) {
        ReadSection(path, server);
    }
    // Valores fora do que o menu oferece (arquivo editado a mao) voltam para dentro.
    cfg.style = Clamp(cfg.style, 0, 1);
    cfg.colorMode = Clamp(cfg.colorMode, 0, 1);
    cfg.size = Clamp(cfg.size, 70, 150);
    cfg.distance = Clamp(cfg.distance, 0, 1000);
    cfg.bubbleWidth = Clamp(cfg.bubbleWidth, 60, 160);
    cfg.bubbleOpacity = Clamp(cfg.bubbleOpacity, 40, 100);
    cfg.fixedChip = Clamp(cfg.fixedChip, 0, 2);
    cfg.fixedText = Clamp(cfg.fixedText, 0, 1);
    cfg.pingLimit = Clamp(cfg.pingLimit, 80, 1000);
}

void SaveConfig(const char* path, const char* server) {
    WriteSection(path, "padrao");
    if (server && server[0]) {
        WriteSection(path, server);
    }
}

} // namespace tags
