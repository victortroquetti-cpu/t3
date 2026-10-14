// Trok Hitmarker (.asi) -- Victor_Trok
// Os sons da pasta "Trok Hitmarker\sons": um .mp3 ou .wav com o nome de cada evento; sem o arquivo, o som padrao que
// vem no .asi. Tocados pelo DirectSound (varios ao mesmo tempo, com o volume do menu). Trocou o arquivo, vale na hora.
#pragma once

#include <windows.h>

namespace sound {

enum Id { HIT, HEAD, TAKEN, KILL, KILL_HEAD, DEATH, COUNT };
extern const char* const NAMES[COUNT]; // acerto, cabeca, dano_levado, abate, abate_cabeca, morte

bool Init(HWND window, const char* folder);
void Update(float dt);       // confere a pasta (1x por segundo) e toca a fila do "Tocar"
void SetVolume(int percent); // 0..100
void Play(Id id);
void PlayAll();              // todos em sequencia (o "Tocar" do menu)

} // namespace sound
