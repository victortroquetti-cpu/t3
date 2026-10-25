// Trok DevTools (.asi) -- Victor_Trok
// A malha de um modelo carregado, copiada do RenderWare do jogo (o que o jogo carregou: o original ou o modificado):
// vertices ja no espaco do modelo (cada atomico na posicao do frame dele), uma parte por material com a textura do
// Direct3D do RenderWare, as texturas usadas e as do resto da TXD, e as linhas da colisao.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "gta.h"

namespace mesh {

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct Part {
    void* texture = nullptr; // IDirect3DTexture9* do RenderWare (vale enquanto o modelo esta carregado)
    DWORD color = 0xFFFFFFFF; // ARGB do material
    bool alpha = false;
    int first = 0, count = 0; // indices
};

struct Batch {
    std::vector<Vertex> verts;
    std::vector<WORD> indices;
    std::vector<Part> parts;
};

struct Texture {
    std::string name, mask;
    void* d3d = nullptr; // IDirect3DTexture9*
    int width = 0, height = 0, depth = 0;
    bool alpha = false;
    bool used = false; // usada pelo modelo (false = so esta na TXD)
};

// O esqueleto de um personagem (o RpSkin e o RpHAnimHierarchy do atomico com pele), para a previa das animacoes.
struct Bone {
    int tag = -1;    // a etiqueta do osso (nodeID): e por ela que as animacoes acham o osso
    int parent = -1; // indice do pai (-1 = raiz)
    std::string name;
    float skinToBone[16]; // RwMatrix: direita, cima, frente, posicao (4 floats cada)
};
struct SkinBatch {
    int batch = -1;                     // em Mesh::batches (os vertices na mesma ordem)
    std::vector<gta::Vec3> pos, normal; // no espaco da pele (antes do frame do atomico)
    std::vector<BYTE> index;            // 4 ossos por vertice
    std::vector<float> weight;          // 4 pesos por vertice
    gta::Vec3 toModel[4];               // o frame do atomico: direita, cima, frente, posicao
};
struct Skin {
    std::vector<Bone> bones;
    std::vector<SkinBatch> batches;
};

struct Mesh {
    bool ok = false;
    std::string error;
    int id = -1;
    std::vector<Batch> batches;
    gta::Vec3 min = {0, 0, 0}, max = {0, 0, 0};
    int vertices = 0, triangles = 0, materials = 0, atomics = 0;
    std::vector<Texture> textures;
    // Colisao (do CColModel do modelo)
    bool hasCol = false, colLoaded = false;
    gta::Vec3 colMin = {0, 0, 0}, colMax = {0, 0, 0};
    int colSpheres = 0, colBoxes = 0, colTriangles = 0;
    std::vector<gta::Vec3> colLines; // pares de pontos
    Skin skin; // so personagens
};

// O modelo tem que estar carregado (gta::Load).
bool Extract(int id, bool vehicle, Mesh* out);

// A TXD (carregada) tem uma textura com esse nome? (sem diferenciar maiusculas, como o jogo)
bool TxdHas(int txdIndex, const std::string& name);
// Algum material do modelo (carregado) usa essa textura? Conta todos os atomicos, ate os escondidos.
bool UsesTexture(int id, const std::string& name);

} // namespace mesh
