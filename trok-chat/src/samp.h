// Trok Chat (.asi) -- Victor_Trok
// O SA-MP 0.3.7 R1 por dentro: as mensagens do chat dele, a caixa de digitacao e o /trokchat.
#pragma once

namespace samp {

// Thread do desenho: espera o samp.dll 0.3.7 R1 e os objetos do chat. false = o Trok Chat fica desligado.
bool Start();

// No Present, antes do chat: na primeira vez, liga os desvios, traz as mensagens que o SA-MP ja tinha e registra
// o /trokchat (tudo na thread do jogo, sem disputar com ela).
void FrameStart();

bool Ready();        // ligado no SA-MP

// No Present, a cada quadro com o desenho do Trok Chat pronto. Sem isso o chat do SA-MP continua (ou volta) na tela.
void Heartbeat();

bool Active();       // o Trok Chat no lugar do chat do SA-MP (ligado no /trokchat e desenhando)

// A janela do jogo esta passando o T ou o F6 para os outros (o SA-MP, la no fim): se o SA-MP abrir a caixa dele agora,
// e o jogador pedindo o chat, e a barra do Trok Chat abre no lugar.
void ChatKey(bool passing);

} // namespace samp
