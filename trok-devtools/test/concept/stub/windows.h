// Stub minimo do windows.h para as imagens de conceito no Linux (o mesmo do kit + o que o menu do DevTools usa).
#pragma once
#include <cstring>
#include <unistd.h>
typedef void* HMODULE;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef int BOOL;
#define MAX_PATH 260
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
inline DWORD GetFileAttributesA(const char* p) { return access(p, R_OK) == 0 ? 0 : INVALID_FILE_ATTRIBUTES; }
inline DWORD GetModuleFileNameA(HMODULE, char* out, DWORD n) { strncpy(out, "C:\\GTA\\gta_sa.exe", n); return 1; }
inline unsigned GetPrivateProfileIntA(const char*, const char*, int def, const char*) { return static_cast<unsigned>(def); }
inline BOOL WritePrivateProfileStringA(const char*, const char*, const char*, const char*) { return 1; }
inline DWORD GetPrivateProfileStringA(const char*, const char*, const char* def, char* out, DWORD size, const char*) {
    strncpy(out, def ? def : "", size);
    return static_cast<DWORD>(strlen(out));
}
struct SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
};
inline DWORD GetTickCount() { return 0; }
