// Trok DevTools (.asi) -- Victor_Trok
// O que roda na thread do jogo: carregar os modelos das miniaturas (alguns por quadro, so os cards na tela), segurar o
// modelo do card aberto, criar e apagar no mundo, e o "informacoes ao apontar". GameTick vem do CGame::Process;
// BeforeUi vem do Present, antes do menu.
#pragma once

#include <d3d9.h>

#include <string>

#include "gta.h"
#include "mesh.h"

namespace core {

void SetGameDir(const char* dir); // o catalogo comeca a ser lido no primeiro quadro do jogo
void GameTick();
void BeforeUi(IDirect3DDevice9* device);

// Miniaturas
void SetBrowsing(bool on); // a grade esta na tela (sem ela, nada carrega)
void WantThumb(int id);    // o card esta na tela e ainda nao tem miniatura
bool ThumbFailed(int id);  // nao da para ter miniatura (sem arquivo, sem malha)
int ThumbsQueued();
void SetThumbsPerFrame(int n);

// Card aberto
void Open(int id);
void Close();
int OpenId();
const mesh::Mesh* OpenMesh(); // nullptr enquanto carrega
bool OpenFailed(std::string* why);

// Mundo
void Spawn(int id);
int SpawnedCount();
void DespawnAll();
bool TakeSpawnMessage(std::string* text, bool* ok); // o resultado do ultimo pedido (uma vez)

// Informacoes ao apontar
struct Look {
    bool valid = false;
    int model = -1;
    int type = 0; // 1 predio, 2 veiculo, 3 personagem, 4 objeto, 5 dummy
    int area = 0;
    int lodModel = -1;
    gta::Vec3 pos = {0, 0, 0}, hit = {0, 0, 0};
    float distance = 0;
    bool hasBox = false;
    gta::Vec3 box[8];
};
void SetInspect(bool on);
bool InspectOn();
const Look& Looking();

} // namespace core
