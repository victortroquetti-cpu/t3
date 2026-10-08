// Stub minimo do windows.h para o teste de renderizacao no Linux.
#pragma once
#include <cstring>
#include <unistd.h>
typedef void* HMODULE;
typedef unsigned long DWORD;
#define MAX_PATH 260
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
inline DWORD GetFileAttributesA(const char* p) { return access(p, R_OK) == 0 ? 0 : INVALID_FILE_ATTRIBUTES; }
inline DWORD GetModuleFileNameA(HMODULE, char* out, DWORD n) { strncpy(out, "C:\\GTA\\gta_sa.exe", n); return 1; }
