// Trok Hitmarker (.asi) -- Victor_Trok
// O lado do gta_sa.exe 1.0 US: onde a mira esta (o GTA desenha a dele e o mod le onde ela caiu), esconder a mira do
// jogo quando a do Trok esta ligada, a mira estatica e o tamanho (so o desenho), o jogador local, os ossos dos alvos,
// a projecao na tela e para onde a camera olha.
#pragma once

#include <windows.h>

#include "imgui.h"

namespace game {

struct Vec3 {
    float x, y, z;
};

bool VersionOk(); // gta_sa.exe 1.0 US (o mesmo teste dos plugins do GTA)
bool Install();   // ganchos da mira; false = nada foi alterado

// Como desenhar a mira do jogo nos proximos quadros (o Present chama todo quadro). hide: a do Trok esta na tela no
// lugar dela (o GTA nao desenha a dele, mas o mod ainda le onde ela cairia). carRule: no carro, a mira so aparece no
// drive-by. Sem chamada recente (overlay parado), a mira do jogo volta ao normal sozinha.
void SetCrosshair(bool hide, bool fixed, float size, bool carRule);

struct Aim {
    bool drawn = false; // o GTA desenhou (ou ia desenhar) a mira neste quadro: o jogador esta mirando
    ImVec2 center;      // o centro dela, em pixels da tela
    float spread = 1;   // o quanto a mira esta aberta pelos tiros (raio agora / raio parado; 1 = parada)
};
Aim TakeAim();                    // o que os ganchos viram desde a ultima chamada
ImVec2 DefaultAim(ImVec2 screen); // onde o GTA poe a mira (CCamera::m_f3rdPersonCHairMultX/Y)

bool ExcludedWeapon(int weapon); // sniper, bazucas e camera: a mira e a do GTA (luneta), sempre

struct Local {
    DWORD ped = 0;
    bool alive = false;
    bool inVehicle = false;
    bool followed = false; // a camera esta no proprio personagem (nao em espectador, selecao de classe, cutscene)
    bool driveBy = false;  // no carro, mirando de passageiro (a camera de mira do carro)
    int weapon = 0;
    float health = 0, armour = 0;
    Vec3 pos = {0, 0, 0};
};
Local LocalPlayer();

bool PedPos(DWORD ped, Vec3* out);
// A parte do corpo (como o SA-MP manda: 3 tronco ... 9 cabeca) na tela. false: atras da camera ou sem modelo.
bool PartOnScreen(DWORD ped, int part, ImVec2* out);
// A parte do corpo no mundo (o osso dela; sem o osso, o corpo).
bool PartWorld(DWORD ped, int part, Vec3* out);
// O ponto exato onde a sua bala pegou neste personagem (CWeapon::DoBulletImpact), se foi ha pouco; consome.
bool TakeImpact(DWORD ped, Vec3* point);
bool Project(const Vec3& world, ImVec2* out);
// Angulo de um ponto do mundo em relacao a para onde a camera olha (0 = na frente, + = a direita), visto de "from".
float AngleFromCamera(const Vec3& from, const Vec3& target);

bool Paused();     // menu de pausa
bool HudHidden();  // cutscene (tela larga) ou HUD desligado

} // namespace game
