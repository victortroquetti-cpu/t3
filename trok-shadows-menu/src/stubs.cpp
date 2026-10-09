// Trok Shadows Menu (.asi) -- Victor_Trok
// O trecho em assembly do evento de quadro.

#include "tsm.h"

extern "C" {

// Idle chamando CGame::Process (0x53E981): destino original e endereco de retorno guardado durante a chamada.
uintptr_t g_tsmFrameOriginal = 0;
uintptr_t g_tsmFrameReturn = 0;
void TsmOnFrame();

} // extern "C"

// Chama a original (o jogo ja empilhou os argumentos), guarda o retorno dela, roda o mod e volta.
extern "C" __attribute__((naked)) void TsmFrameStub() {
    __asm__(".intel_syntax noprefix\n"
            "pop dword ptr [_g_tsmFrameReturn]\n"
            "call dword ptr [_g_tsmFrameOriginal]\n"
            "push eax\n"
            "call _TsmOnFrame\n"
            "pop eax\n"
            "jmp dword ptr [_g_tsmFrameReturn]\n"
            ".att_syntax prefix\n");
}
