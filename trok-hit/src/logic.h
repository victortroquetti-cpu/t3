// Trok Hitmarker (.asi) -- Victor_Trok
// Do que o SA-MP avisa (dano dado e levado, mensagem de morte) ao que o hud desenha: o dano real (a vida e o colete
// que o alvo perdeu no sync), os abates, os numeros, o informer com combo, as setas e a borda do dano levado, e os sons.
#pragma once

#include "game.h"
#include "hud.h"
#include "options.h"

namespace logic {

void Reset(); // trocou de servidor ou saiu dele

// Eventos do samp.dll (na thread do jogo, entre os quadros).
void OnGive(int id, float damage, int weapon, int part);
void OnTake(int id, float damage, int weapon, int part);
void OnDeath(const char* killer, const char* victim, int weapon);
void OnOtherHit(int target, int weapon); // outro jogador acertou este alvo (bullet sync)

// Um quadro (no Present): avanca o tempo, mede o dano real e monta o que o hud desenha.
void Frame(float dt, ImVec2 screen, const game::Aim& aim, const opt::Options& o, hud::State* st);

} // namespace logic
