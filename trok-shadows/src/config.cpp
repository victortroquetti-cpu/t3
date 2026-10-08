// Trok Shadows (.asi) -- Victor_Trok
// "Trok Shadows.ini": as mesmas secoes e chaves do shadows.ini do Shadows Extender, mais [GERAL]. Valores fora da
// faixa sao corrigidos (e avisados no log). Se o INI nao existir, ele e criado com os valores do shadows.ini antigo
// (quando ele esta na mesma pasta) ou com os padroes do Shadows Extender.

#include "trok.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

Config g_cfg;

namespace {

const char* const S_STENCIL = "STENCIL_SHADOWS";
const char* const S_STENCIL_COLOR = "STENCIL_SHADOWS_COLOR";
const char* const S_REALTIME_COLOR = "REALTIME_SHADOWS_COLOR";
const char* const S_REALTIME = "REALTIME_SHADOWS";
const char* const S_GERAL = "GERAL";

bool ReadRaw(const char* ini, const char* section, const char* key, char* out, DWORD size) {
    out[0] = 0;
    GetPrivateProfileStringA(section, key, "", out, size, ini);
    // Comentario no fim da linha (o shadows.ini original usa "valor ; comentario").
    char* semi = strchr(out, ';');
    if (semi) {
        *semi = 0;
    }
    char* end = out + strlen(out);
    while (end > out && (end[-1] == ' ' || end[-1] == '\t')) {
        *--end = 0;
    }
    char* start = out;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (start != out) {
        memmove(out, start, strlen(start) + 1);
    }
    return out[0] != 0;
}

int ReadInt(const char* ini, const char* section, const char* key, int def) {
    char buf[64];
    if (!ReadRaw(ini, section, key, buf, sizeof(buf))) {
        return def;
    }
    char* end;
    const long v = strtol(buf, &end, 10);
    return end == buf ? def : static_cast<int>(v);
}

float ReadFloat(const char* ini, const char* section, const char* key, float def) {
    char buf[64];
    if (!ReadRaw(ini, section, key, buf, sizeof(buf))) {
        return def;
    }
    for (char* p = buf; *p; p++) {
        if (*p == ',') {
            *p = '.'; // "0,6" vira 0.6
        }
    }
    char* end;
    const double v = strtod(buf, &end);
    return end == buf ? def : static_cast<float>(v);
}

bool ReadBool(const char* ini, const char* section, const char* key, bool def) {
    return ReadInt(ini, section, key, def ? 1 : 0) != 0;
}

// -1 = automatico (texto "auto" ou chave ausente).
int ReadAuto(const char* ini, const char* section, const char* key) {
    char buf[64];
    if (!ReadRaw(ini, section, key, buf, sizeof(buf)) || !lstrcmpiA(buf, "auto")) {
        return -1;
    }
    char* end;
    const long v = strtol(buf, &end, 10);
    return end == buf ? -1 : (v != 0);
}

template <class T>
void Clamp(T& v, T lo, T hi, const char* name) {
    if (v < lo || v > hi) {
        const T fixed = v < lo ? lo : hi;
        Log("  aviso: %s fora da faixa, usando %g", name, static_cast<double>(fixed));
        v = fixed;
    }
}

void ReadAll(Config& c, const char* ini, bool sampLoaded) {
    c.maxShadows = ReadInt(ini, S_STENCIL, "MaxShadows", 64);
    c.stencilMaxDistance = ReadFloat(ini, S_STENCIL, "MaxDistance", 50.0f);
    c.flagIgnoreSomeShadows = ReadBool(ini, S_STENCIL, "FlagIgnoreSomeShadows", false);
    c.disableBuildingShadows = ReadBool(ini, S_STENCIL, "DisableBuildingShadows", false);
    c.stencilLowSettings = ReadBool(ini, S_STENCIL, "DisplayShadowsAtLowSettings", false);

    const char* rgba[4] = {"R", "G", "B", "A"};
    for (int i = 0; i < 4; i++) {
        c.stencilColor[i] = ReadInt(ini, S_STENCIL_COLOR, rgba[i], i == 3 ? 50 : 0);
        c.realtimeColor[i] = ReadInt(ini, S_REALTIME_COLOR, rgba[i], i == 3 ? 50 : 0);
    }

    c.realtimeLowSettings = ReadBool(ini, S_REALTIME, "DisplayShadowsAtLowSettings", false);
    c.combineWithStencil = ReadBool(ini, S_REALTIME, "CombineRealTimeShadowsWithStencil", true);
    c.realtimeMaxDistance = ReadFloat(ini, S_REALTIME, "MaxDistance", 15.0f);
    c.createBlur1 = ReadBool(ini, S_REALTIME, "CreateBlur1", true);
    c.blurLevel = ReadInt(ini, S_REALTIME, "BlurLevel", 4);
    c.createBlur2 = ReadBool(ini, S_REALTIME, "CreateBlur2", true);
    c.rasterSize = ReadInt(ini, S_REALTIME, "RasterSize", 7);
    c.blurRasterSize = ReadInt(ini, S_REALTIME, "BlurRasterSize", 6);
    c.rasterSize2 = ReadInt(ini, S_REALTIME, "RasterSize2", 6);
    c.blurRasterSize2 = ReadInt(ini, S_REALTIME, "BlurRasterSize2", 6);
    c.gradientMax = ReadInt(ini, S_REALTIME, "GradientMax", 128);
    c.gradientMin = ReadInt(ini, S_REALTIME, "GradientMin", 64);
    c.boundSphere = ReadFloat(ini, S_REALTIME, "ShadowBoundSphere", 2.0f);
    c.boundSphereInAir = ReadFloat(ini, S_REALTIME, "ShadowBoundSphereInAir", 2.0f);
    c.sunZLimit = ReadFloat(ini, S_REALTIME, "ShadowSunZLimit", 0.6f);
    c.zLimit = ReadFloat(ini, S_REALTIME, "ShadowZDistanceLimit", 4.0f);
    c.zLimitInAir = ReadFloat(ini, S_REALTIME, "ShadowZDistanceLimitInAir", 4.0f);
    c.drawVehicleDefaultWithRealTime = ReadBool(ini, S_REALTIME, "DrawVehicleDefaultShadowWithRealTime", true);
    c.disableVehicleDefaultShadow = ReadBool(ini, S_REALTIME, "DisableVehicleDefaultShadow", false);
    c.enableShader = ReadBool(ini, S_REALTIME, "EnableShadowsShader", true);
    c.nightFactor = ReadFloat(ini, S_REALTIME, "ShadowIntensityNightFactor", 0.2f);
    c.cloudsFactor = ReadFloat(ini, S_REALTIME, "ShadowIntensityCloudsFactor", 0.4f);
    const int multi = ReadAuto(ini, S_REALTIME, "MoreThanOnePlayer");
    c.moreThanOnePlayer = multi < 0 ? sampLoaded : multi != 0;

    c.autoReload = ReadBool(ini, S_GERAL, "recarregar", true);

    Clamp(c.maxShadows, 16, 4096, "MaxShadows");
    Clamp(c.stencilMaxDistance, 1.0f, 1000.0f, "STENCIL_SHADOWS MaxDistance");
    Clamp(c.realtimeMaxDistance, 1.0f, 500.0f, "REALTIME_SHADOWS MaxDistance");
    for (int i = 0; i < 4; i++) {
        Clamp(c.stencilColor[i], 0, 255, "STENCIL_SHADOWS_COLOR");
        Clamp(c.realtimeColor[i], 0, 255, "REALTIME_SHADOWS_COLOR");
    }
    Clamp(c.blurLevel, 0, 16, "BlurLevel");
    Clamp(c.rasterSize, 4, 11, "RasterSize");
    Clamp(c.blurRasterSize, 4, 11, "BlurRasterSize");
    Clamp(c.rasterSize2, 4, 11, "RasterSize2");
    Clamp(c.blurRasterSize2, 4, 11, "BlurRasterSize2");
    Clamp(c.gradientMax, 0, 255, "GradientMax");
    Clamp(c.gradientMin, 0, 255, "GradientMin");
    Clamp(c.boundSphere, 0.1f, 100.0f, "ShadowBoundSphere");
    Clamp(c.boundSphereInAir, 0.1f, 100.0f, "ShadowBoundSphereInAir");
    Clamp(c.sunZLimit, -1.0f, 2.0f, "ShadowSunZLimit");
    Clamp(c.zLimit, 0.1f, 100.0f, "ShadowZDistanceLimit");
    Clamp(c.zLimitInAir, 0.1f, 100.0f, "ShadowZDistanceLimitInAir");
    Clamp(c.nightFactor, 0.0f, 1.0f, "ShadowIntensityNightFactor");
    Clamp(c.cloudsFactor, 0.0f, 1.0f, "ShadowIntensityCloudsFactor");
    if (!c.createBlur1) {
        // No Shadows Extender o jogo travava com CreateBlur1=0 (a sombra e o blur ficam com tamanhos diferentes).
        Log("  aviso: CreateBlur1=0 trava o jogo, usando 1");
        c.createBlur1 = true;
    }
}

const char* Float(char* buf, float v) {
    snprintf(buf, 32, "%g", static_cast<double>(v));
    if (!strpbrk(buf, ".e")) {
        strcat(buf, ".0");
    }
    return buf;
}

bool WriteIni(const char* path, const Config& c, bool multiAuto) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    char a[32], b[32];
    fprintf(f,
            "; Trok Shadows -- configuração\r\n"
            "; Salve este arquivo com o jogo aberto e as mudanças valem na hora (cor, força, distância, liga/desliga).\r\n"
            "; Os itens marcados com (reinicie) só mudam quando o jogo abre de novo.\r\n"
            "; As chaves são as mesmas do shadows.ini do Shadows Extender.\r\n"
            "\r\n[STENCIL_SHADOWS]\r\n"
            "; Quantas sombras stencil podem existir ao mesmo tempo. Jogo: 64. (reinicie)\r\n"
            "MaxShadows=%d\r\n"
            "; Até quantos metros a sombra stencil aparece. Jogo: 50.\r\n"
            "MaxDistance=%s\r\n"
            "; 1 = desenha a sombra de todos os objetos em todo quadro (o jogo reveza um em cada quatro).\r\n"
            "FlagIgnoreSomeShadows=%d\r\n"
            "; 1 = tira a sombra stencil dos objetos e prédios.\r\n"
            "DisableBuildingShadows=%d\r\n"
            "; 1 = sombra stencil mesmo com Sombras no baixo nas opções do jogo.\r\n"
            "DisplayShadowsAtLowSettings=%d\r\n",
            c.maxShadows, Float(a, c.stencilMaxDistance), c.flagIgnoreSomeShadows, c.disableBuildingShadows,
            c.stencilLowSettings);
    fprintf(f,
            "\r\n[STENCIL_SHADOWS_COLOR]\r\n"
            "; Cor (0 a 255) e força A (0 a 255) da sombra stencil. Jogo: preto com força 50.\r\n"
            "R=%d\r\nG=%d\r\nB=%d\r\nA=%d\r\n"
            "\r\n[REALTIME_SHADOWS_COLOR]\r\n"
            "; Cor e força da sombra em tempo real. No modo combinado vale a cor de STENCIL_SHADOWS_COLOR.\r\n"
            "R=%d\r\nG=%d\r\nB=%d\r\nA=%d\r\n",
            c.stencilColor[0], c.stencilColor[1], c.stencilColor[2], c.stencilColor[3], c.realtimeColor[0],
            c.realtimeColor[1], c.realtimeColor[2], c.realtimeColor[3]);
    fprintf(f,
            "\r\n[REALTIME_SHADOWS]\r\n"
            "; 1 = sombra em tempo real mesmo com Sombras no baixo nas opções do jogo.\r\n"
            "DisplayShadowsAtLowSettings=%d\r\n"
            "; 1 = a sombra em tempo real entra no stencil: fica com a cor da stencil e não escurece dobrado onde\r\n"
            ";     as duas se cruzam. Precisa de EnableShadowsShader=1.\r\n"
            "CombineRealTimeShadowsWithStencil=%d\r\n"
            "; Até quantos metros a sombra em tempo real aparece. Jogo: 15.\r\n"
            "MaxDistance=%s\r\n"
            "; Desfoque da sombra. CreateBlur1 fica sempre em 1 (com 0 o jogo trava). (reinicie)\r\n"
            "CreateBlur1=%d\r\n"
            "BlurLevel=%d\r\n"
            "; Degradê na sombra, de GradientMax a GradientMin (0 a 255). (reinicie)\r\n"
            "CreateBlur2=%d\r\n"
            "GradientMax=%d\r\n"
            "GradientMin=%d\r\n"
            "; Resolução em potência de 2: 7 = 128x128, 8 = 256, 9 = 512, 10 = 1024. (reinicie)\r\n"
            "RasterSize=%d\r\n"
            "BlurRasterSize=%d\r\n"
            "RasterSize2=%d\r\n"
            "BlurRasterSize2=%d\r\n",
            c.realtimeLowSettings, c.combineWithStencil, Float(a, c.realtimeMaxDistance), c.createBlur1,
            c.blurLevel, c.createBlur2, c.gradientMax, c.gradientMin, c.rasterSize, c.blurRasterSize,
            c.rasterSize2, c.blurRasterSize2);
    fprintf(f,
            "; Raio em metros, em volta da sombra, onde ela é projetada. InAir: helicóptero e avião.\r\n"
            "ShadowBoundSphere=%s\r\n",
            Float(a, c.boundSphere));
    fprintf(f, "ShadowBoundSphereInAir=%s\r\n", Float(a, c.boundSphereInAir));
    fprintf(f,
            "; Altura mínima do sol para a sombra (evita sombra comprida demais). À noite a sombra fica embaixo.\r\n"
            "ShadowSunZLimit=%s\r\n"
            "; Distância vertical até onde a sombra alcança o chão. InAir: helicóptero e avião.\r\n"
            "ShadowZDistanceLimit=%s\r\n",
            Float(a, c.sunZLimit), Float(b, c.zLimit));
    fprintf(f,
            "ShadowZDistanceLimitInAir=%s\r\n"
            "; 1 = veículo com sombra em tempo real ganha também a sombra simples do jogo.\r\n"
            "DrawVehicleDefaultShadowWithRealTime=%d\r\n"
            "; 1 = tira a sombra simples dos veículos.\r\n"
            "DisableVehicleDefaultShadow=%d\r\n"
            "; 1 = pixel shader do mod (cor própria e modo combinado). 0 = sombra em tempo real do jogo.\r\n"
            "EnableShadowsShader=%d\r\n",
            Float(a, c.zLimitInAir), c.drawVehicleDefaultWithRealTime, c.disableVehicleDefaultShadow,
            c.enableShader);
    fprintf(f,
            "; Quanto da força sobra à noite e com o céu nublado (0 a 1).\r\n"
            "ShadowIntensityNightFactor=%s\r\n"
            "ShadowIntensityCloudsFactor=%s\r\n",
            Float(a, c.nightFactor), Float(b, c.cloudsFactor));
    if (multiAuto) {
        fprintf(f, "; Sombra em tempo real para todos os jogadores (SA-MP). auto = liga quando o SA-MP está aberto.\r\n"
                   "MoreThanOnePlayer=auto\r\n");
    } else {
        fprintf(f,
                "; Sombra em tempo real para todos os jogadores (SA-MP). auto = liga quando o SA-MP está aberto.\r\n"
                "MoreThanOnePlayer=%d\r\n",
                c.moreThanOnePlayer);
    }
    fprintf(f,
            "\r\n[GERAL]\r\n"
            "; 1 = aplica este arquivo assim que você salva, com o jogo aberto.\r\n"
            "recarregar=%d\r\n",
            c.autoReload);
    fclose(f);
    return true;
}

} // namespace

bool ConfigFileTime(const char* path, FILETIME* out) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        return false;
    }
    *out = data.ftLastWriteTime;
    return true;
}

void ConfigLoad(Config& cfg, const char* iniPath, const char* oldIniPath, bool sampLoaded) {
    if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) {
        const bool hasOld = GetFileAttributesA(oldIniPath) != INVALID_FILE_ATTRIBUTES;
        Config fresh;
        ReadAll(fresh, hasOld ? oldIniPath : iniPath, sampLoaded);
        const bool multiAuto = !hasOld || ReadAuto(oldIniPath, S_REALTIME, "MoreThanOnePlayer") < 0;
        if (WriteIni(iniPath, fresh, multiAuto)) {
            Log(hasOld ? "INI criado com os valores do shadows.ini antigo" : "INI criado com os valores padrao");
        } else {
            Log("aviso: nao deu para criar o INI (%s)", iniPath);
        }
    }
    ReadAll(cfg, iniPath, sampLoaded);
}

void ConfigLog(const Config& c) {
    Log("  stencil: MaxShadows=%d MaxDistance=%g FlagIgnoreSomeShadows=%d DisableBuildingShadows=%d "
        "DisplayShadowsAtLowSettings=%d cor=%d,%d,%d,%d",
        c.maxShadows, c.stencilMaxDistance, c.flagIgnoreSomeShadows, c.disableBuildingShadows, c.stencilLowSettings,
        c.stencilColor[0], c.stencilColor[1], c.stencilColor[2], c.stencilColor[3]);
    Log("  tempo real: MaxDistance=%g Raster=%d/%d/%d/%d Blur=%d/%d/%d Gradient=%d/%d cor=%d,%d,%d,%d",
        c.realtimeMaxDistance, c.rasterSize, c.blurRasterSize, c.rasterSize2, c.blurRasterSize2, c.createBlur1,
        c.blurLevel, c.createBlur2, c.gradientMax, c.gradientMin, c.realtimeColor[0], c.realtimeColor[1],
        c.realtimeColor[2], c.realtimeColor[3]);
    Log("  tempo real: BoundSphere=%g/%g ZLimit=%g/%g SunZLimit=%g Noite=%g Nuvens=%g", c.boundSphere,
        c.boundSphereInAir, c.zLimit, c.zLimitInAir, c.sunZLimit, c.nightFactor, c.cloudsFactor);
    Log("  tempo real: DisplayShadowsAtLowSettings=%d Shader=%d Combinado=%d SombraSimplesJunto=%d "
        "SemSombraSimples=%d MaisDeUmJogador=%d recarregar=%d",
        c.realtimeLowSettings, c.enableShader, c.combineWithStencil, c.drawVehicleDefaultWithRealTime,
        c.disableVehicleDefaultShadow, c.moreThanOnePlayer, c.autoReload);
}
