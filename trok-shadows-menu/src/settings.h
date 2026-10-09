// Trok Shadows Menu (.asi) -- Victor_Trok
// As configuracoes do Shadows Extender 2.0 (as mesmas chaves do shadows.ini, com os padroes dele) e o que o menu
// pede para o resto do mod. Sem windows.h: o teste de renderizacao do menu compila no Linux.
#pragma once

struct Settings {
    // [STENCIL_SHADOWS]
    int maxShadows = 64;
    float stencilDistance = 50.0f;
    bool flagIgnoreSome = false;
    bool disableBuildings = false;
    bool stencilLow = false;
    // [STENCIL_SHADOWS_COLOR] e [REALTIME_SHADOWS_COLOR]: R, G, B e A (forca), de 0 a 255
    int stencilColor[4] = {0, 0, 0, 50};
    int realtimeColor[4] = {0, 0, 0, 50};
    // [REALTIME_SHADOWS]
    bool realtimeLow = false;
    bool combine = true;
    float realtimeDistance = 15.0f;
    bool blur1 = true;
    int blurLevel = 4;
    bool blur2 = true;
    int raster = 7, blurRaster = 6, raster2 = 6, blurRaster2 = 6;
    int gradientMax = 128, gradientMin = 64;
    float bound = 2.0f, boundAir = 2.0f;
    float sunZ = 0.6f;
    float zLimit = 4.0f, zLimitAir = 4.0f;
    bool vehicleDefaultWithRealtime = true;
    bool disableVehicleDefault = false;
    bool shader = true;
    float night = 0.2f, clouds = 0.4f;
    bool morePlayers = false;
    // [TROK_MENU]
    bool fixOccupants = true; // quem esta no veiculo entra na sombra dele (nada de escurecer dobrado)
};

// Campo a campo (o memcmp pegaria os bytes de enchimento entre os bool e os int).
inline bool operator==(const Settings& a, const Settings& b) {
    for (int i = 0; i < 4; i++) {
        if (a.stencilColor[i] != b.stencilColor[i] || a.realtimeColor[i] != b.realtimeColor[i]) {
            return false;
        }
    }
    return a.maxShadows == b.maxShadows && a.stencilDistance == b.stencilDistance &&
           a.flagIgnoreSome == b.flagIgnoreSome && a.disableBuildings == b.disableBuildings &&
           a.stencilLow == b.stencilLow && a.realtimeLow == b.realtimeLow && a.combine == b.combine &&
           a.realtimeDistance == b.realtimeDistance && a.blur1 == b.blur1 && a.blurLevel == b.blurLevel &&
           a.blur2 == b.blur2 && a.raster == b.raster && a.blurRaster == b.blurRaster && a.raster2 == b.raster2 &&
           a.blurRaster2 == b.blurRaster2 && a.gradientMax == b.gradientMax && a.gradientMin == b.gradientMin &&
           a.bound == b.bound && a.boundAir == b.boundAir && a.sunZ == b.sunZ && a.zLimit == b.zLimit &&
           a.zLimitAir == b.zLimitAir && a.vehicleDefaultWithRealtime == b.vehicleDefaultWithRealtime &&
           a.disableVehicleDefault == b.disableVehicleDefault && a.shader == b.shader && a.night == b.night &&
           a.clouds == b.clouds && a.morePlayers == b.morePlayers && a.fixOccupants == b.fixOccupants;
}

inline bool operator!=(const Settings& a, const Settings& b) {
    return !(a == b);
}

namespace backend {

bool Attached();                    // achou o Shadows Extender e ja pode mexer nele
const char* Problem();              // por que ainda nao (texto para o menu)
Settings& Current();                // os valores em uso
const Settings& Defaults();         // os padroes do Shadows Extender
void Apply(const Settings& before); // aplica o que mudou de before para Current()
bool RestartPending();              // alguma mudanca so vale quando o jogo abrir de novo
bool Save();                        // grava no shadows.ini
int ActiveShadows();                // sombras em tempo real no jogo agora (0 a 16)
bool FixInstalled();                // a correcao do veiculo conseguiu entrar nos ganchos do Shadows Extender

} // namespace backend
