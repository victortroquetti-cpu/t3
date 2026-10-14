// Trok Hitmarker (.asi) -- Victor_Trok
// A pasta "Trok Hitmarker" ao lado do .asi: criada na primeira vez com as imagens e os sons padrao.

#include "res.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

void HitLog(const char* fmt, ...); // main.cpp

namespace res {

void Unpack(const char* folder, const File* files, int count) {
    if (GetFileAttributesA(folder) != INVALID_FILE_ATTRIBUTES) {
        return;
    }
    char parent[MAX_PATH];
    snprintf(parent, sizeof(parent), "%s", folder);
    char* slash = strrchr(parent, '\\');
    if (slash) {
        *slash = 0;
        CreateDirectoryA(parent, nullptr);
    }
    if (!CreateDirectoryA(folder, nullptr)) {
        HitLog("nao deu para criar a pasta %s (%lu) -- valem as imagens e sons de dentro do .asi", folder,
               GetLastError());
        return;
    }
    int written = 0;
    for (int i = 0; i < count; ++i) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", folder, files[i].name);
        HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) {
            continue;
        }
        DWORD done = 0;
        if (WriteFile(f, files[i].data, files[i].size, &done, nullptr) && done == files[i].size) {
            ++written;
        }
        CloseHandle(f);
    }
    HitLog("pasta criada: %s (%d arquivos padrao)", folder, written);
}

bool ReadAll(const char* path, std::vector<unsigned char>* out) {
    out->clear();
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD size = GetFileSize(f, nullptr);
    bool ok = size != INVALID_FILE_SIZE && size > 0 && size < 32 * 1024 * 1024;
    if (ok) {
        out->resize(size);
        DWORD done = 0;
        ok = ReadFile(f, out->data(), size, &done, nullptr) && done == size;
    }
    CloseHandle(f);
    if (!ok) {
        out->clear();
    }
    return ok;
}

unsigned long long Stamp(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return 0;
    }
    unsigned long long t = (static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                           data.ftLastWriteTime.dwLowDateTime;
    return (t ^ (static_cast<unsigned long long>(data.nFileSizeLow) * 0x9E3779B97F4A7C15ull)) | 1; // nunca 0
}

} // namespace res
