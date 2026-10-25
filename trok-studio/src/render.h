// Trok Studio (.asi) -- Victor_Trok
// O desenho dos modelos com o proprio Direct3D 9 do jogo, fora da cena do GTA (no Present): as miniaturas dos cards
// (num atlas de 2048x2048, 128 px cada, desenhadas a 256 px e reduzidas) e o visualizador 3D do card aberto. Salva e
// devolve todo o estado do device; os alvos de render somem no Reset do device e voltam sozinhos.
#pragma once

#include <d3d9.h>

#include "imgui.h"
#include "mesh.h"

namespace render {

struct View {
    float yaw = 35.0f, pitch = 18.0f; // graus: de frente-direita, um pouco de cima
    float zoom = 1.0f;                // 1 = o modelo inteiro na tela
    bool textures = true, wire = false, collision = false, grid = true;
    DWORD background = 0xFF141414;
};

void SetDevice(IDirect3DDevice9* device);
void DeviceLost(); // antes do Reset

// Miniaturas
bool HasThumb(int id);
bool Thumb(int id, ImTextureID* tex, ImVec2* uv0, ImVec2* uv1); // tambem marca como usada agora
bool DrawThumb(const mesh::Mesh& m);                             // desenha agora (o modelo tem que estar carregado)
void ForgetThumbs();
int ThumbCount();

// Visualizador: desenha num alvo de size x size e devolve a textura (nullptr se nao deu).
ImTextureID DrawViewer(const mesh::Mesh& m, const View& v, int size);

// Texturas do RenderWare: largura, altura, formato e mipmaps (do Direct3D).
struct TexInfo {
    int width = 0, height = 0, levels = 0;
    const char* format = "?";
};
bool Describe(void* d3dTexture, TexInfo* out);

} // namespace render
