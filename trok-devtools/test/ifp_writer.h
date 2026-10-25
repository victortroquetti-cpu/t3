// Trok DevTools -- testes: monta .ifp (ANP3 e ANPK) e o anim.img (VER2) de mentira, no formato que o jogo le
// (CAnimManager::LoadAnimFile do gta-reversed). Usado pelo teste do Linux (test/anim) e pelo do Wine (test/smoke).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace ifpw {

struct Key {
    float time;
    float q[4]; // x, y, z, w (como o jogo usa)
    float t[3];
};
struct Seq {
    std::string name;
    int bone; // a etiqueta do osso (-1 = so o nome)
    bool trans;
    std::vector<Key> keys;
};
struct Anim {
    std::string name;
    std::vector<Seq> seqs;
    bool compressed = true;
};

inline void Put(std::string& s, const void* p, size_t n) {
    s.append(static_cast<const char*>(p), n);
}
template <class T>
void Put(std::string& s, T v) {
    Put(s, &v, sizeof(v));
}
inline void Name(std::string& s, const std::string& n, size_t len) {
    std::string b(len, '\0');
    memcpy(&b[0], n.data(), std::min(n.size(), len - 1));
    s += b;
}

// Rotacao em torno de um eixo (x, y, z unitario), em graus
inline void Axis(float* q, float x, float y, float z, float degrees) {
    float h = degrees * 3.14159265f / 360.0f, s = std::sin(h);
    q[0] = x * s, q[1] = y * s, q[2] = z * s, q[3] = std::cos(h);
}

inline std::string Anp3(const std::string& block, const std::vector<Anim>& anims) {
    std::string body;
    Name(body, block, 24);
    Put<uint32_t>(body, static_cast<uint32_t>(anims.size()));
    for (const Anim& a : anims) {
        std::string seqs;
        uint32_t frameBytes = 0;
        for (const Seq& s : a.seqs) {
            Name(seqs, s.name, 24);
            uint32_t type = a.compressed ? (s.trans ? 4 : 3) : (s.trans ? 2 : 1);
            Put<uint32_t>(seqs, type);
            Put<uint32_t>(seqs, static_cast<uint32_t>(s.keys.size()));
            Put<int32_t>(seqs, s.bone);
            size_t before = seqs.size();
            for (const Key& k : s.keys) {
                if (a.compressed) {
                    for (float v : k.q) {
                        Put<int16_t>(seqs, static_cast<int16_t>(std::lround(v * 4096.0f)));
                    }
                    Put<int16_t>(seqs, static_cast<int16_t>(std::lround(k.time * 60.0f)));
                    if (s.trans) {
                        for (float v : k.t) {
                            Put<int16_t>(seqs, static_cast<int16_t>(std::lround(v * 1024.0f)));
                        }
                    }
                } else {
                    for (float v : k.q) {
                        Put<float>(seqs, v);
                    }
                    Put<float>(seqs, k.time);
                    if (s.trans) {
                        for (float v : k.t) {
                            Put<float>(seqs, v);
                        }
                    }
                }
            }
            frameBytes += static_cast<uint32_t>(seqs.size() - before);
        }
        Name(body, a.name, 24);
        Put<uint32_t>(body, static_cast<uint32_t>(a.seqs.size()));
        Put<uint32_t>(body, frameBytes);
        Put<uint32_t>(body, a.compressed ? 1u : 0u);
        body += seqs;
    }
    std::string out = "ANP3";
    Put<uint32_t>(out, static_cast<uint32_t>(body.size()));
    return out + body;
}

inline std::string Chunk(const char* id, const std::string& data) {
    std::string out(id, 4);
    Put<uint32_t>(out, static_cast<uint32_t>(data.size()));
    out += data;
    out.append((4 - data.size() % 4) % 4, '\0');
    return out;
}

// O formato do GTA3/VC: a rotacao vai conjugada no arquivo (o jogo inverte na leitura).
inline std::string Anpk(const std::string& block, const std::vector<Anim>& anims) {
    std::string info;
    Put<uint32_t>(info, static_cast<uint32_t>(anims.size()));
    info += block + '\0';
    std::string body = Chunk("INFO", info);
    for (const Anim& a : anims) {
        body += Chunk("NAME", a.name + '\0');
        std::string seqInfo;
        Put<uint32_t>(seqInfo, static_cast<uint32_t>(a.seqs.size()));
        std::string dgan = Chunk("INFO", seqInfo);
        for (const Seq& s : a.seqs) {
            std::string anim;
            Name(anim, s.name, 28);
            Put<uint32_t>(anim, static_cast<uint32_t>(s.keys.size()));
            Put<uint32_t>(anim, 0);
            Put<int32_t>(anim, -1);
            Put<int32_t>(anim, -1);
            std::string frames;
            for (const Key& k : s.keys) {
                Put<float>(frames, -k.q[0]);
                Put<float>(frames, -k.q[1]);
                Put<float>(frames, -k.q[2]);
                Put<float>(frames, k.q[3]);
                if (s.trans) {
                    for (float v : k.t) {
                        Put<float>(frames, v);
                    }
                }
                Put<float>(frames, k.time);
            }
            std::string cpan = Chunk("ANIM", anim) + (s.keys.empty() ? std::string() : Chunk(s.trans ? "KRT0" : "KR00", frames));
            dgan += Chunk("CPAN", cpan);
        }
        body += Chunk("DGAN", dgan);
    }
    std::string out = "ANPK";
    Put<uint32_t>(out, static_cast<uint32_t>(body.size()));
    return out + body;
}

// anim.img (VER2): o diretorio e os arquivos, cada um em setores de 2048 bytes.
inline std::string Img(const std::vector<std::pair<std::string, std::string>>& files) {
    uint32_t dirSectors = static_cast<uint32_t>((8 + files.size() * 32 + 2047) / 2048);
    std::string dir = "VER2", data;
    Put<uint32_t>(dir, static_cast<uint32_t>(files.size()));
    uint32_t sector = dirSectors;
    for (const auto& f : files) {
        uint32_t sectors = static_cast<uint32_t>((f.second.size() + 2047) / 2048);
        Put<uint32_t>(dir, sector);
        Put<uint16_t>(dir, static_cast<uint16_t>(sectors));
        Put<uint16_t>(dir, 0);
        Name(dir, f.first, 24);
        std::string padded = f.second;
        padded.resize(sectors * 2048, '\0');
        data += padded;
        sector += sectors;
    }
    dir.resize(dirSectors * 2048, '\0');
    return dir + data;
}

} // namespace ifpw
