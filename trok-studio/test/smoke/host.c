/* O "gta_sa.exe" do teste: um .exe minimo (em 0x400000, como o jogo, sem a biblioteca do C) cuja memoria zerada
 * (.bss) cobre os enderecos fixos do GTA SA 1.0 US de ~0x403000 ate ~0xD63000. Toda a logica do GTA de mentira mora no
 * fake_core.dll (fake_gta.cpp), que recebe essa faixa, confere se ela cobre o que o Trok Studio usa e le a linha de
 * comando sozinho. */
#include <windows.h>

unsigned char g_gtaMemory[0x960000];

typedef int (*RunFn)(unsigned, unsigned);

void start(void) {
    HMODULE core = LoadLibraryA("fake_core.dll");
    RunFn run = core ? (RunFn)GetProcAddress(core, "Run") : 0;
    ExitProcess(run ? (UINT)run((unsigned)g_gtaMemory, (unsigned)sizeof(g_gtaMemory)) : 3);
}
