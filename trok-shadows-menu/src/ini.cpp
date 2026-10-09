// Trok Shadows Menu (.asi) -- Victor_Trok
// Grava o shadows.ini do Shadows Extender trocando so os valores que mudaram. O que vem depois do ';' (os
// comentarios dele), as linhas em branco, a ordem e as chaves que o mod nao conhece ficam como estavam.

#include "tsm.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) {
        a++;
    }
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) {
        b--;
    }
    return s.substr(a, b - a);
}

bool SameText(const std::string& a, const char* b) {
    return !lstrcmpiA(a.c_str(), b);
}

class Ini {
public:
    bool Load(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) {
            return true; // sem arquivo: comeca vazio
        }
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            text.append(buf, n);
        }
        fclose(f);
        crlf_ = text.find("\r\n") != std::string::npos || text.empty();
        size_t start = 0;
        while (start < text.size()) {
            size_t end = text.find('\n', start);
            if (end == std::string::npos) {
                end = text.size();
            }
            std::string line = text.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines_.push_back(line);
            start = end + 1;
        }
        return true;
    }

    bool Save(const char* path) const {
        FILE* f = fopen(path, "wb");
        if (!f) {
            return false;
        }
        for (const std::string& line : lines_) {
            fputs(line.c_str(), f);
            fputs(crlf_ ? "\r\n" : "\n", f);
        }
        return fclose(f) == 0;
    }

    void Set(const char* section, const char* key, const std::string& value) {
        int at = FindSection(section);
        if (at < 0) {
            if (!lines_.empty() && !Trim(lines_.back()).empty()) {
                lines_.push_back("");
            }
            lines_.push_back(std::string("[") + section + "]");
            lines_.push_back(std::string(key) + "=" + value);
            return;
        }
        int last = at; // ultima linha com conteudo da secao
        for (int i = at + 1; i < static_cast<int>(lines_.size()); i++) {
            const std::string t = Trim(lines_[i]);
            if (!t.empty() && t[0] == '[') {
                break;
            }
            if (!t.empty()) {
                last = i;
            }
            const size_t eq = lines_[i].find('=');
            if (t.empty() || t[0] == ';' || eq == std::string::npos ||
                !SameText(Trim(lines_[i].substr(0, eq)), key)) {
                continue;
            }
            // "chave = valor ; comentario": troca so o valor, com os espacos e o comentario de antes.
            std::string& line = lines_[i];
            size_t v = eq + 1;
            while (v < line.size() && (line[v] == ' ' || line[v] == '\t')) {
                v++;
            }
            size_t end = line.find(';', v);
            if (end == std::string::npos) {
                end = line.size();
            }
            while (end > v && (line[end - 1] == ' ' || line[end - 1] == '\t')) {
                end--;
            }
            line = line.substr(0, v) + value + line.substr(end);
            return;
        }
        lines_.insert(lines_.begin() + last + 1, std::string(key) + "=" + value);
    }

private:
    int FindSection(const char* section) const {
        const std::string want = std::string("[") + section + "]";
        for (int i = 0; i < static_cast<int>(lines_.size()); i++) {
            if (SameText(Trim(lines_[i]), want.c_str())) {
                return i;
            }
        }
        return -1;
    }

    std::vector<std::string> lines_;
    bool crlf_ = true;
};

std::string Int(int v) {
    return std::to_string(v);
}

// Como o shadows.ini escreve: "180.0", "0.6".
std::string Real(float v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    if (!strpbrk(buf, ".e")) {
        strcat(buf, ".0");
    }
    return buf;
}

} // namespace

bool IniWrite(const char* path, const Settings& s, const Settings& b) {
    Ini ini;
    if (!ini.Load(path)) {
        return false;
    }
    const char* ST = "STENCIL_SHADOWS";
    const char* RT = "REALTIME_SHADOWS";
    const char* rgba[4] = {"R", "G", "B", "A"};
    int changed = 0;
    auto i = [&](const char* sec, const char* key, int now, int was) {
        if (now != was) {
            ini.Set(sec, key, Int(now));
            changed++;
        }
    };
    auto f = [&](const char* sec, const char* key, float now, float was) {
        if (now != was) {
            ini.Set(sec, key, Real(now));
            changed++;
        }
    };
    i(ST, "FlagIgnoreSomeShadows", s.flagIgnoreSome, b.flagIgnoreSome);
    i(ST, "MaxShadows", s.maxShadows, b.maxShadows);
    f(ST, "MaxDistance", s.stencilDistance, b.stencilDistance);
    i(ST, "DisableBuildingShadows", s.disableBuildings, b.disableBuildings);
    i(ST, "DisplayShadowsAtLowSettings", s.stencilLow, b.stencilLow);
    for (int c = 0; c < 4; c++) {
        i("STENCIL_SHADOWS_COLOR", rgba[c], s.stencilColor[c], b.stencilColor[c]);
        i("REALTIME_SHADOWS_COLOR", rgba[c], s.realtimeColor[c], b.realtimeColor[c]);
    }
    f(RT, "MaxDistance", s.realtimeDistance, b.realtimeDistance);
    i(RT, "CreateBlur1", s.blur1, b.blur1);
    i(RT, "CreateBlur2", s.blur2, b.blur2);
    i(RT, "BlurLevel", s.blurLevel, b.blurLevel);
    i(RT, "RasterSize", s.raster, b.raster);
    i(RT, "BlurRasterSize", s.blurRaster, b.blurRaster);
    i(RT, "RasterSize2", s.raster2, b.raster2);
    i(RT, "BlurRasterSize2", s.blurRaster2, b.blurRaster2);
    i(RT, "GradientMax", s.gradientMax, b.gradientMax);
    i(RT, "GradientMin", s.gradientMin, b.gradientMin);
    f(RT, "ShadowBoundSphere", s.bound, b.bound);
    f(RT, "ShadowBoundSphereInAir", s.boundAir, b.boundAir);
    f(RT, "ShadowSunZLimit", s.sunZ, b.sunZ);
    f(RT, "ShadowZDistanceLimit", s.zLimit, b.zLimit);
    f(RT, "ShadowZDistanceLimitInAir", s.zLimitAir, b.zLimitAir);
    f(RT, "ShadowIntensityNightFactor", s.night, b.night);
    f(RT, "ShadowIntensityCloudsFactor", s.clouds, b.clouds);
    i(RT, "DrawVehicleDefaultShadowWithRealTime", s.vehicleDefaultWithRealtime, b.vehicleDefaultWithRealtime);
    i(RT, "DisableVehicleDefaultShadow", s.disableVehicleDefault, b.disableVehicleDefault);
    i(RT, "DisplayShadowsAtLowSettings", s.realtimeLow, b.realtimeLow);
    i(RT, "EnableShadowsShader", s.shader, b.shader);
    i(RT, "CombineRealTimeShadowsWithStencil", s.combine, b.combine);
    i(RT, "MoreThanOnePlayer", s.morePlayers, b.morePlayers);
    i("TROK_MENU", "CorrigirVeiculo", s.fixOccupants, b.fixOccupants);
    if (!changed) {
        return true;
    }
    const bool ok = ini.Save(path);
    if (ok) {
        Log("shadows.ini gravado (%d valores)", changed);
    }
    return ok;
}
