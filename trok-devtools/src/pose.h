// Trok DevTools (.asi) -- Victor_Trok
// A previa das animacoes: o personagem (a malha com pele do mesh.h) na pose de um instante de uma animacao, como o
// jogo faz (RpAnimBlend do gta-reversed): cada osso com quadro-chave usa a rotacao dele e, se tiver, a translacao; os
// outros ficam na pose do modelo (a raiz sem translacao = no lugar dela). Depois a pele: cada vertice segue ate 4 ossos.
#pragma once

#include "ifp.h"
#include "mesh.h"

namespace pose {

// Ossos da animacao que o esqueleto tem (para dizer quando a animacao e de outro esqueleto).
int Matched(const mesh::Mesh& base, const ifp::Clip& clip);

// out: a copia da malha (feita aqui na primeira vez) com os vertices na pose do instante 'time'. inPlace: a raiz nao
// anda no chao (fica onde comecou). A caixa (para a camera) e a do modelo, com folga: a camera nao pula com a animacao.
bool Apply(const mesh::Mesh& base, const ifp::Clip& clip, float time, bool inPlace, mesh::Mesh* out);

} // namespace pose
