// Trok Headmove (.asi) -- Victor_Trok
// Trok Headmove.ini, ao lado do .asi. O mod confere o arquivo a cada segundo: mudou, vale na hora, com o jogo aberto.
#pragma once

namespace cfg {

struct Options {
    bool enabled = true;     // false: a cabeca fica como o SA-MP faz
    int delayMs = 0;         // seu personagem: tempo para a cabeca chegar onde a camera olha (0 = na hora)
    bool others = true;      // os outros jogadores tambem (como voce os ve)
    int othersDelayMs = 100; // o atraso deles
    float turn = 1.0f;       // giro do GTA por quadro (0.1 = o do SA-MP, 1.0 = na hora)
};

const Options& Get();
// Le o arquivo (e cria um com os padroes e as explicacoes, se ele nao existir).
void Init(const char* path);
// true = o arquivo mudou desde a ultima leitura e foi lido de novo.
bool Reload();
// "atraso 0 ms, outros 100 ms, giro 1.00" (para o log e o chat)
void Describe(char* out, int size);

} // namespace cfg
