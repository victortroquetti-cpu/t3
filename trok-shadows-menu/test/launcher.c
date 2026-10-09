// Lancador do teste: um exe em 0x400000 cuja imagem vai ate 0xD00000, como o gta_sa.exe. Sem isso o Wine poe
// outras coisas nessa faixa antes do teste rodar. A memoria "do jogo" e o array abaixo; o teste em si fica no
// host.dll (test/host.cpp).
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

static unsigned char g_game[0x900000];

int main(int argc, char** argv) {
    const uintptr_t begin = (uintptr_t)g_game, end = begin + sizeof(g_game);
    if (begin > 0x40FD10 || end < 0xD00000) {
        printf("FALHA a memoria do jogo ficou em %08lx-%08lx\n", (unsigned long)begin, (unsigned long)end);
        return 2;
    }
    DWORD old;
    VirtualProtect(g_game, sizeof(g_game), PAGE_EXECUTE_READWRITE, &old);
    HMODULE host = LoadLibraryA("host.dll");
    if (!host) {
        printf("FALHA host.dll (%lu)\n", GetLastError());
        return 2;
    }
    typedef int (*HostMainFn)(const char*, uintptr_t);
    HostMainFn hostMain = (HostMainFn)GetProcAddress(host, "HostMain");
    return hostMain(argc > 1 ? argv[1] : "completo", begin);
}
