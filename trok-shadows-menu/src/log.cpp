// Trok Shadows Menu (.asi) -- Victor_Trok
// Log em "Trok Shadows Menu.log", ao lado do .asi. Recriado a cada vez que o jogo abre.

#include "tsm.h"

#include <cstdarg>
#include <cstdio>

namespace {
char g_logPath[MAX_PATH] = "";
}

void LogOpen(const char* path) {
    lstrcpynA(g_logPath, path, MAX_PATH);
    FILE* f = fopen(g_logPath, "w");
    if (f) {
        fclose(f);
    }
}

void Log(const char* fmt, ...) {
    if (!g_logPath[0]) {
        return;
    }
    FILE* f = fopen(g_logPath, "a");
    if (!f) {
        return;
    }
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}
