// Trok DevTools (.asi) -- Victor_Trok
// O player da aba Audios: toca um som do SFX (o PCM lido do pacote) pelo waveOut do Windows, por fora do audio do jogo.
// Repetir = tocar de novo quando acaba. Tudo na thread do menu (Update todo quadro).
#pragma once

namespace player {

bool Play(int bank, int sound); // para o que estava tocando
void Stop();
void Update(); // fim do som, repetir
void SetRepeat(bool on);
bool Repeat();
bool Playing();
int Bank();  // o ultimo escolhido (-1 = nenhum)
int Sound();
float Position(); // segundos
float Length();
const char* Error(); // "" = sem erro

} // namespace player
