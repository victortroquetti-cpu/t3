// Stub minimo do windows.h para o teste do Linux (o ifp.cpp e o pose.cpp, sem o jogo).
#pragma once
#include <sys/stat.h>
#include <cstdint>
#include <cstring>
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef int BOOL;
typedef void* HANDLE;
#define WINAPI
#define MAX_PATH 260
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
// Os caminhos do mod usam \ (Windows): aqui viram /
inline void TrokUnixPath(const char* in, char* out, size_t n) {
    size_t i = 0;
    for (; in[i] && i + 1 < n; ++i) {
        out[i] = in[i] == '\\' ? '/' : in[i];
    }
    out[i] = 0;
}
inline DWORD GetFileAttributesA(const char* p) {
    char path[1024];
    TrokUnixPath(p, path, sizeof(path));
    struct stat st;
    return stat(path, &st) == 0 ? 0 : INVALID_FILE_ATTRIBUTES;
}
// O fopen do ifp.cpp tambem passa por aqui (os caminhos com \)
#include <cstdio>
inline FILE* TrokFopen(const char* p, const char* mode) {
    char path[1024];
    TrokUnixPath(p, path, sizeof(path));
    return std::fopen(path, mode);
}
#define fopen TrokFopen
inline HANDLE CreateThread(void*, size_t, DWORD (*)(void*), void*, DWORD, DWORD*) { return nullptr; } // roda na hora
inline BOOL CloseHandle(HANDLE) { return 1; }
