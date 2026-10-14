// Trok Hitmarker (.asi) -- Victor_Trok
// PNG (stb_image, dominio publico) -> textura gerenciada do D3D9 (sobrevive ao Reset). Imagem grande e reduzida ate
// 256 px (media de area, com o alfa pre-multiplicado: sem borda escura), e os pixels transparentes herdam a cor dos
// vizinhos para a filtragem nao escurecer o contorno. A textura e potencia de 2; o resto fica fora das coordenadas.

#include "images.h"

#include "hud.h"
#include "res.h"

#include <cstdio>
#include <cstring>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_image.h"
#pragma GCC diagnostic pop

void HitLog(const char* fmt, ...); // main.cpp

namespace images {
namespace {

const char* const NAMES[hud::IMG_COUNT] = {"mira", "acerto", "colete", "cabeca", "abate", "corpo"};
constexpr int MAX_SIDE = 256;

struct Slot {
    IDirect3DTexture9* texture = nullptr;
    unsigned long long stamp = 1; // 1 = ainda nao carregada
};

Slot g_slots[hud::IMG_COUNT];
IDirect3DDevice9* g_device = nullptr;
char g_folder[MAX_PATH] = "";
float g_check = 0;

struct Image {
    int w = 0, h = 0;
    std::vector<float> px; // RGBA pre-multiplicado, 0..1
};

bool Decode(const unsigned char* data, size_t size, Image* out) {
    int w = 0, h = 0, n = 0;
    unsigned char* rgba = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &n, 4);
    if (!rgba || w < 1 || h < 1 || w > 8192 || h > 8192) {
        stbi_image_free(rgba);
        return false;
    }
    out->w = w;
    out->h = h;
    out->px.resize(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
        float a = rgba[i * 4 + 3] / 255.0f;
        out->px[i * 4 + 0] = rgba[i * 4 + 0] / 255.0f * a;
        out->px[i * 4 + 1] = rgba[i * 4 + 1] / 255.0f * a;
        out->px[i * 4 + 2] = rgba[i * 4 + 2] / 255.0f * a;
        out->px[i * 4 + 3] = a;
    }
    stbi_image_free(rgba);
    return true;
}

// Media de area: cada pixel novo e a media ponderada dos pixels de origem que ele cobre.
Image Shrink(const Image& in, int w, int h) {
    Image out;
    out.w = w;
    out.h = h;
    out.px.assign(static_cast<size_t>(w) * h * 4, 0.0f);
    float sx = static_cast<float>(in.w) / w, sy = static_cast<float>(in.h) / h;
    for (int y = 0; y < h; ++y) {
        float y0 = y * sy, y1 = y0 + sy;
        for (int x = 0; x < w; ++x) {
            float x0 = x * sx, x1 = x0 + sx;
            float acc[4] = {0, 0, 0, 0}, total = 0;
            for (int iy = static_cast<int>(y0); iy < in.h && iy < y1; ++iy) {
                float wy = (iy + 1 < y1 ? iy + 1 : y1) - (iy > y0 ? iy : y0);
                for (int ix = static_cast<int>(x0); ix < in.w && ix < x1; ++ix) {
                    float wx = (ix + 1 < x1 ? ix + 1 : x1) - (ix > x0 ? ix : x0);
                    const float* p = &in.px[(static_cast<size_t>(iy) * in.w + ix) * 4];
                    for (int c = 0; c < 4; ++c) {
                        acc[c] += p[c] * wx * wy;
                    }
                    total += wx * wy;
                }
            }
            float* q = &out.px[(static_cast<size_t>(y) * w + x) * 4];
            for (int c = 0; c < 4; ++c) {
                q[c] = total > 0 ? acc[c] / total : 0;
            }
        }
    }
    return out;
}

int Pow2(int v) {
    int p = 1;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

// Volta ao alfa normal (o ImGui mistura com SRCALPHA) e espalha a cor para dentro dos pixels transparentes.
std::vector<unsigned> ToBgra(const Image& im, int pw, int ph) {
    std::vector<float> rgb(static_cast<size_t>(im.w) * im.h * 3, 0.0f);
    std::vector<unsigned char> known(static_cast<size_t>(im.w) * im.h, 0);
    for (size_t i = 0; i < known.size(); ++i) {
        float a = im.px[i * 4 + 3];
        if (a > 0.002f) {
            for (int c = 0; c < 3; ++c) {
                rgb[i * 3 + c] = im.px[i * 4 + c] / a;
            }
            known[i] = 1;
        }
    }
    for (int pass = 0; pass < 4; ++pass) {
        std::vector<unsigned char> next = known;
        for (int y = 0; y < im.h; ++y) {
            for (int x = 0; x < im.w; ++x) {
                size_t i = static_cast<size_t>(y) * im.w + x;
                if (known[i]) {
                    continue;
                }
                float acc[3] = {0, 0, 0};
                int n = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= im.w || ny >= im.h) {
                            continue;
                        }
                        size_t j = static_cast<size_t>(ny) * im.w + nx;
                        if (known[j]) {
                            for (int c = 0; c < 3; ++c) {
                                acc[c] += rgb[j * 3 + c];
                            }
                            ++n;
                        }
                    }
                }
                if (n) {
                    for (int c = 0; c < 3; ++c) {
                        rgb[i * 3 + c] = acc[c] / n;
                    }
                    next[i] = 1;
                }
            }
        }
        known.swap(next);
    }
    std::vector<unsigned> out(static_cast<size_t>(pw) * ph, 0);
    for (int y = 0; y < im.h; ++y) {
        for (int x = 0; x < im.w; ++x) {
            size_t i = static_cast<size_t>(y) * im.w + x;
            auto byte = [](float v) { return static_cast<unsigned>(v <= 0 ? 0 : v >= 1 ? 255 : v * 255.0f + 0.5f); };
            out[static_cast<size_t>(y) * pw + x] = byte(im.px[i * 4 + 3]) << 24 | byte(rgb[i * 3 + 0]) << 16 |
                                                   byte(rgb[i * 3 + 1]) << 8 | byte(rgb[i * 3 + 2]);
        }
    }
    return out;
}

IDirect3DTexture9* Upload(const Image& src, float* u1, float* v1) {
    Image im = src;
    int big = im.w > im.h ? im.w : im.h;
    if (big > MAX_SIDE) {
        int w = im.w * MAX_SIDE / big, h = im.h * MAX_SIDE / big;
        im = Shrink(im, w < 1 ? 1 : w, h < 1 ? 1 : h);
    }
    int pw = Pow2(im.w), ph = Pow2(im.h);
    std::vector<unsigned> bgra = ToBgra(im, pw, ph);
    IDirect3DTexture9* tex = nullptr;
    HRESULT hr = g_device->CreateTexture(pw, ph, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr);
    if (FAILED(hr) || !tex) {
        HitLog("imagem: CreateTexture %dx%d falhou (0x%08X)", pw, ph, static_cast<unsigned>(hr));
        return nullptr;
    }
    D3DLOCKED_RECT lr;
    if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) {
        tex->Release();
        return nullptr;
    }
    for (int y = 0; y < ph; ++y) {
        memcpy(static_cast<unsigned char*>(lr.pBits) + y * lr.Pitch, &bgra[static_cast<size_t>(y) * pw], pw * 4);
    }
    tex->UnlockRect(0);
    *u1 = static_cast<float>(im.w) / pw;
    *v1 = static_cast<float>(im.h) / ph;
    return tex;
}

void Load(int slot) {
    Slot& s = g_slots[slot];
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s.png", g_folder, NAMES[slot]);
    unsigned long long stamp = res::Stamp(path) ^ 2; // nunca 1
    if (stamp == s.stamp && s.texture) {
        return;
    }
    s.stamp = stamp;
    Image im;
    std::vector<unsigned char> file;
    bool custom = (stamp ^ 2) && res::ReadAll(path, &file) && Decode(file.data(), file.size(), &im);
    if (!custom) {
        if (stamp ^ 2) {
            HitLog("imagem %s.png: nao abriu como PNG -- usando a padrao", NAMES[slot]);
        }
        const res::File& def = res::IMAGES[slot];
        if (!Decode(def.data, def.size, &im)) {
            return;
        }
    }
    float u1 = 1, v1 = 1;
    IDirect3DTexture9* tex = Upload(im, &u1, &v1);
    if (!tex) {
        return;
    }
    if (s.texture) {
        s.texture->Release();
    }
    s.texture = tex;
    hud::SetImage(slot, reinterpret_cast<ImTextureID>(tex), u1, v1, static_cast<float>(im.w) / im.h);
    if (custom) {
        HitLog("imagem %s.png: %dx%d da pasta", NAMES[slot], im.w, im.h);
    }
}

} // namespace

void Init(const char* folder) {
    snprintf(g_folder, sizeof(g_folder), "%s", folder);
    res::Unpack(folder, res::IMAGES, hud::IMG_COUNT);
}

void Update(IDirect3DDevice9* device, float dt) {
    if (device != g_device) {
        Release();
        g_device = device;
        g_check = 1.0f;
    }
    g_check += dt;
    if (!g_device || g_check < 1.0f) {
        return;
    }
    g_check = 0;
    for (int i = 0; i < hud::IMG_COUNT; ++i) {
        Load(i);
    }
}

void Release() {
    for (int i = 0; i < hud::IMG_COUNT; ++i) {
        if (g_slots[i].texture) {
            g_slots[i].texture->Release();
            g_slots[i].texture = nullptr;
        }
        g_slots[i].stamp = 1;
        hud::SetImage(i, nullptr);
    }
    g_device = nullptr;
}

} // namespace images
