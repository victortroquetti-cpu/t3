// Trok DevTools (.asi) -- Victor_Trok
// O que roda na thread do jogo: carregar os modelos das miniaturas (alguns por quadro, so os cards na tela), segurar o
// modelo do card aberto, criar e apagar no mundo, e o "informacoes ao apontar". GameTick vem do CGame::Process;
// BeforeUi vem do Present, antes do menu.
#pragma once

#include <d3d9.h>

#include <string>
#include <vector>

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

// Quem usa uma textura: os modelos da TXD onde ela esta (e das TXDs filhas dela) que tem um material com ela. A busca
// carrega os candidatos aos poucos, como as miniaturas, e solta cada um logo depois.
struct TextureUsers {
    int model = -1;         // o modelo do card quando a busca comecou
    std::string texture;    // o nome da textura
    int txd = -1;           // a TXD que tem a textura (-1 = nao achou nas TXDs do modelo)
    std::string txdName;    // o nome dela, pelo catalogo
    std::vector<int> users; // em ordem de id
    int checked = 0, total = 0;
    bool capped = false; // mais candidatos do que o limite (so os primeiros foram vistos)
    bool done = false;
};
// Chamada todo quadro enquanto o painel da textura esta na tela; so recomeca se a textura mudar ou se o modelo do card
// nao estiver entre os que ja usam a textura.
void FindTextureUsers(int model, const std::string& texture);
const TextureUsers& TextureUsersNow();

// Animacoes: o personagem da previa (preso enquanto a aba pede, todo quadro) e tocar no personagem do jogador.
void PreviewPed(int id);          // chamar todo quadro com a aba na tela
const mesh::Mesh* PreviewMesh(); // nullptr enquanto carrega
bool PreviewFailed(std::string* why);
int PlayerSkin();                 // o modelo do personagem do jogador (-1 = sem personagem)
void PlayAnim(int anim, bool loop, bool move); // indice em ifp::Anims()
void StopAnim();
int PlayingAnim(); // a ultima pedida (-1 = parado)
bool TakeAnimMessage(std::string* text, bool* ok);

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
    gta::Vec3 camPos = {0, 0, 0}, camForward = {0, 1, 0}; // para cortar a caixa no plano da camera
};
void SetInspect(bool on);
bool InspectOn();
const Look& Looking();

} // namespace core
