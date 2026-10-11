// Trok Veiculos -- Trok Veiculos.ini: [padrao] e uma secao por servidor ("ip:porta"). Ao entrar num servidor, vale
// o [padrao] com a secao dele por cima; cada mudanca e gravada nas duas, entao um servidor novo comeca com as
// ultimas escolhas.

#include "config.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>

namespace veic {

Config cfg;

namespace {

struct Key {
    const char* name;
    bool* flag;
    int* number;
};

const Key KEYS[] = {
    {"ligado", &cfg.enabled, nullptr},          {"mostrar", nullptr, &cfg.show},
    {"tempo_dano", nullptr, &cfg.damageHold},   {"esconder_meu", &cfg.hideMine, nullptr},
    {"vazios", &cfg.empty, nullptr},            {"posicao", nullptr, &cfg.position},
    {"tamanho", nullptr, &cfg.size},            {"distancia", nullptr, &cfg.distance},
    {"parede", nullptr, &cfg.walls},            {"desbotar", &cfg.fade, nullptr},
    {"blindagem", &cfg.armour, nullptr},        {"teto_blindagem", nullptr, &cfg.armourCap},
    {"teto_fixo", nullptr, &cfg.armourFixed},   {"cores", &cfg.stateColors, nullptr},
    {"linha_fogo", &cfg.fireLine, nullptr},     {"dano_barra", &cfg.damageBar, nullptr},
    {"dano_numero", &cfg.damageNumber, nullptr}, {"nome", &cfg.name, nullptr},
    {"id", &cfg.id, nullptr},                   {"vida_numero", &cfg.healthNumber, nullptr},
    {"pneus", &cfg.tires, nullptr},             {"cadeado", &cfg.lock, nullptr},
    {"det_modelo", &cfg.detailModel, nullptr},  {"det_distancia", &cfg.detailDistance, nullptr},
    {"det_lugares", &cfg.detailSeats, nullptr},
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
    cfg.show = Clamp(cfg.show, 0, 1);
    cfg.damageHold = Clamp(cfg.damageHold, 3, 15);
    cfg.position = Clamp(cfg.position, 0, 1);
    cfg.size = Clamp(cfg.size, 70, 150);
    cfg.distance = Clamp(cfg.distance, 0, 20);
    cfg.walls = Clamp(cfg.walls, 0, 1);
    cfg.armourCap = Clamp(cfg.armourCap, 0, 1);
    cfg.armourFixed = Clamp(cfg.armourFixed, 500, 20000);
}

void SaveConfig(const char* path, const char* server) {
    WriteSection(path, "padrao");
    if (server && server[0]) {
        WriteSection(path, server);
    }
}

} // namespace veic
