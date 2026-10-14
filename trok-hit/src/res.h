// Trok Hitmarker (.asi) -- Victor_Trok
// As imagens e os sons padrao, dentro do .asi (o build gera os dados a partir de dist/Trok Hitmarker com res/embed.py).
#pragma once

#include <vector>

namespace res {

struct File {
    const char* name; // com a extensao: "mira.png", "acerto.wav"
    const unsigned char* data;
    unsigned size;
};

extern const File IMAGES[6]; // mira, acerto, colete, cabeca, abate, corpo (a ordem do hud::ImageSlot)
extern const File SOUNDS[6]; // acerto, cabeca, dano_levado, abate, abate_cabeca, morte (a ordem do sound::Id)

// Primeira vez (a pasta ainda nao existe): cria a pasta e grava os padroes, para o usuario ver o que pode trocar.
// Pasta que ja existe fica como esta (arquivo apagado de proposito volta ao padrao so na memoria).
void Unpack(const char* folder, const File* files, int count);

// Arquivo inteiro na memoria (aberto sem travar: o editor de imagem pode estar salvando). Vazio = nao existe/erro.
bool ReadAll(const char* path, std::vector<unsigned char>* out);
// Data de gravacao e tamanho (0 = nao existe): muda quando o usuario troca o arquivo.
unsigned long long Stamp(const char* path);

} // namespace res
