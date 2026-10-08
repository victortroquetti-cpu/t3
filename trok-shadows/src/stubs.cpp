// Trok Shadows (.asi) -- Victor_Trok
// Os trechos em assembly: ganchos no meio de funcoes do jogo, onde valores vivem em registradores, e os eventos
// (que chamam a funcao original com os argumentos do jogo intactos e so depois o mod).

#include "trok.h"

extern "C" {

// Eventos: destino original de cada chamada e endereco de retorno guardado durante a chamada.
uintptr_t g_trokEventOriginal[EVENT_COUNT];
uintptr_t g_trokEventReturn[EVENT_COUNT];

// CRealTimeShadow::Update, logo depois de desenhar o clump na camera da sombra (0x706676).
uintptr_t g_trokExtrasReturn = 0x70667B;
void TrokShadowExtras(void* shadow);

// CShadows::StoreRealTimeShadow, onde o jogo le a altura do sol (0x707E2B).
uintptr_t g_trokSunReturn = 0x707E30;
float g_trokSunZLimit = 0.6f;
float g_trokHalf = 0.5f; // sem const: so o assembly usa, e precisa sair como simbolo externo

} // extern "C"

// Chama a original (o jogo ja empilhou os argumentos), guarda o retorno dela, roda o evento do mod e volta.
#define TROK_EVENT_STUB(id)                                                                                      \
    extern "C" __attribute__((naked)) void TrokEventStub##id() {                                                 \
        __asm__(".intel_syntax noprefix\n"                                                                       \
                "pop dword ptr [_g_trokEventReturn + 4*" #id "]\n"                                               \
                "call dword ptr [_g_trokEventOriginal + 4*" #id "]\n"                                            \
                "push eax\n"                                                                                     \
                "push " #id "\n"                                                                                 \
                "call _TrokOnEvent\n"                                                                            \
                "add esp, 4\n"                                                                                   \
                "pop eax\n"                                                                                      \
                "jmp dword ptr [_g_trokEventReturn + 4*" #id "]\n"                                               \
                ".att_syntax prefix\n");                                                                         \
    }

TROK_EVENT_STUB(0)
TROK_EVENT_STUB(1)
TROK_EVENT_STUB(2)
TROK_EVENT_STUB(3)

// esi = CRealTimeShadow*, edi = &shadow->m_camera. Repete as duas instrucoes que o jmp cobriu.
extern "C" __attribute__((naked)) void TrokExtrasStub() {
    __asm__(".intel_syntax noprefix\n"
            "pushad\n"
            "push esi\n"
            "call _TrokShadowExtras\n"
            "add esp, 4\n"
            "popad\n"
            "mov eax, dword ptr [edi]\n"
            "mov cl, byte ptr [esi+0x10]\n"
            "jmp dword ptr [_g_trokExtrasReturn]\n"
            ".att_syntax prefix\n");
}

// eax = vetor ate o sol. O jogo faria "mov ecx, edi; fld [eax+8]"; aqui o z carregado nunca fica abaixo de
// ShadowSunZLimit + 0.5 * noite (de noite a luz fica em cima). eax e edx ficam intactos (o Shadows Extender
// estragava o eax).
extern "C" __attribute__((naked)) void TrokSunStub() {
    __asm__(".intel_syntax noprefix\n"
            "mov ecx, edi\n"
            "fld dword ptr ds:0x8D12C0\n"
            "fmul dword ptr [_g_trokHalf]\n"
            "fadd dword ptr [_g_trokSunZLimit]\n"
            "fld dword ptr [eax+8]\n"
            "fcomi st, st(1)\n"
            "jae 1f\n"
            "fxch st(1)\n"
            "1:\n"
            "fstp st(1)\n"
            "jmp dword ptr [_g_trokSunReturn]\n"
            ".att_syntax prefix\n");
}
