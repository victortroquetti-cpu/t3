// Trok Shadows Menu (.asi) -- Victor_Trok
// Os trechos em assembly: o evento de quadro e a entrada no meio de CRealTimeShadow::Update.

#include "tsm.h"

extern "C" {

// Idle chamando CGame::Process (0x53E981): destino original e endereco de retorno guardado durante a chamada.
uintptr_t g_tsmFrameOriginal = 0;
uintptr_t g_tsmFrameReturn = 0;
void TsmOnFrame();

// CRealTimeShadow::Update, logo depois de desenhar o dono na camera da sombra (0x706676): o trecho do Shadows
// Extender que desenha a arma e fecha a camera.
uintptr_t g_tsmSeExtras = 0;
void TsmVehicleExtras(void* shadow);

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

// esi = CRealTimeShadow*, com a camera da sombra ainda aberta. Desenha quem esta no veiculo e segue para o trecho
// do Shadows Extender com todos os registradores e flags como o jogo deixou.
extern "C" __attribute__((naked)) void TsmExtrasStub() {
    __asm__(".intel_syntax noprefix\n"
            "pushad\n"
            "pushfd\n"
            "push esi\n"
            "call _TsmVehicleExtras\n"
            "add esp, 4\n"
            "popfd\n"
            "popad\n"
            "jmp dword ptr [_g_tsmSeExtras]\n"
            ".att_syntax prefix\n");
}
