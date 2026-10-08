// Trok Shadows (.asi) -- Victor_Trok
// Declaracoes compartilhadas: log, remendos na memoria do jogo, configuracao, shaders e ganchos.
#pragma once

#include <windows.h>
#include <cstdint>

#define TROK_SHADOWS_VERSION "1.1"

// ---------------------------------------------------------------- log (log.cpp)
void LogOpen(const char* path);
void Log(const char* fmt, ...);

// ---------------------------------------------------------------- remendos (patch.cpp)
namespace patch {

bool Readable(uintptr_t addr, size_t size);
void Write(uintptr_t addr, const void* data, size_t size);
void Fill(uintptr_t addr, uint8_t value, size_t size);
uintptr_t CallTarget(uintptr_t site); // destino do E8/E9 no endereco, ou 0
bool InImage(uintptr_t addr);         // dentro do gta_sa.exe?
void SetCall(uintptr_t site, const void* fn);
void SetJump(uintptr_t site, const void* fn);

// Chamada do jogo que o mod troca pela sua. Aceita o destino original do 1.0 US ou o gancho de outro mod
// (destino fora do exe: o mod chama o gancho dele no lugar da original, e a corrente continua).
struct Call {
    const char* name;
    uintptr_t site;
    uintptr_t expected; // destino no 1.0 US
    uintptr_t original; // destino lido antes de trocar (o que o mod chama)
    bool installed;

    bool Check() const;             // o endereco tem uma chamada aceitavel?
    bool Install(const void* hook); // grava a chamada para o gancho
};

// O que tem que estar no endereco antes do remendo (confere que e o codigo do 1.0 US).
enum Expect : uint8_t {
    EXPECT_SHORT_JCC, // jcc curto (70..7F)
    EXPECT_NEAR_JCC,  // jcc longo (0F 80..8F)
    EXPECT_CALL,      // E8
};

// Bytes que o mod liga e desliga (NOP, jmp curto). Guarda os bytes originais para poder desligar.
struct Toggle {
    const char* name;
    uintptr_t addr;
    uint8_t size;
    Expect expect;
    uint8_t value[12]; // bytes com o recurso ligado
    uint8_t original[12];
    bool saved;
    bool broken; // o endereco nao tinha o codigo esperado: fica sempre desligado
    bool on;

    void Set(bool enable);
};

} // namespace patch

// ---------------------------------------------------------------- configuracao (config.cpp)
struct Config {
    // [STENCIL_SHADOWS]
    int maxShadows;
    float stencilMaxDistance;
    bool flagIgnoreSomeShadows;
    bool disableBuildingShadows;
    bool stencilLowSettings;
    // [STENCIL_SHADOWS_COLOR]
    int stencilColor[4];
    // [REALTIME_SHADOWS_COLOR]
    int realtimeColor[4];
    // [REALTIME_SHADOWS]
    bool realtimeLowSettings;
    bool combineWithStencil;
    float realtimeMaxDistance;
    bool createBlur1;
    int blurLevel;
    bool createBlur2;
    int rasterSize;
    int blurRasterSize;
    int rasterSize2;
    int blurRasterSize2;
    int gradientMax;
    int gradientMin;
    float boundSphere;
    float boundSphereInAir;
    float sunZLimit;
    float zLimit;
    float zLimitInAir;
    bool drawVehicleDefaultWithRealTime;
    bool disableVehicleDefaultShadow;
    bool enableShader;
    float nightFactor;
    float cloudsFactor;
    bool moreThanOnePlayer;
    // Novos do Trok Shadows
    bool realtimeEnabled;  // sombra em tempo real ligada (desligada: a sombra simples do jogo)
    bool vehicleRealtime;  // veiculos com sombra em tempo real
    bool weaponsInShadow;  // arma, paraquedas e mochila a jato na sombra
    int maxRealtime;       // quantas sombras em tempo real ao mesmo tempo (as mais perto da camera)
    // [GERAL]
    bool autoReload;
};

extern Config g_cfg;

// Le o INI. Se ele nao existir, cria (com os valores do shadows.ini antigo, se houver). Devolve os avisos de
// valores corrigidos pelo log.
void ConfigLoad(Config& cfg, const char* iniPath, const char* oldIniPath, bool sampLoaded);
// INI de uma versao anterior (sem as chaves novas): reescreve com os mesmos valores e as chaves novas comentadas.
void ConfigUpgrade(const char* iniPath, const Config& cfg);
void ConfigLog(const Config& cfg);
bool ConfigFileTime(const char* path, FILETIME* out);

// ---------------------------------------------------------------- shaders (shaders.cpp)
bool ShadersCreate();  // cria os dois pixel shaders no device do jogo
void ShadersRelease();
bool ShadersReady();
void* ShaderRealtime(); // IDirect3DPixelShader9* (sombra em tempo real com cor propria)
void* ShaderStencil();  // IDirect3DPixelShader9* (marca o stencil no modo combinado)

// ---------------------------------------------------------------- ganchos (hooks.cpp)
bool HooksVersionOk();        // confere pontos fixos do gta_sa.exe 1.0 US
void HooksInstallEvents();    // eventos (DllMain)
void HooksApply();            // todos os recursos (depois do RenderWare iniciar)
void HooksApplyLive(const Config& before); // o que muda com o jogo aberto
void HooksRestoreRealtimeUpdate(); // religa a atualizacao das sombras em tempo real (o SA-MP desliga)
void HooksFrame();            // uma vez por quadro: limite de sombras em tempo real e diagnostico
void HooksShutdown();

// Chamado pelos stubs (stubs.cpp) a cada evento.
enum TrokEvent { EVENT_INIT_RW, EVENT_SHUTDOWN_RW, EVENT_INIT_GAME, EVENT_GAME_PROCESS, EVENT_COUNT };
extern "C" void TrokOnEvent(int id);
