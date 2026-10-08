#!/usr/bin/env bash
# Compila o Trok Shadows.asi com MinGW (i686, 32 bits como o gta_sa.exe).
# Requer: i686-w64-mingw32-g++ (pacote g++-mingw-w64-i686-posix). Nao usa nenhuma biblioteca de fora.
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p build dist
CXX=${CXX:-i686-w64-mingw32-g++}

# O .asi sem simbolos vai para dist/; a copia com simbolos (build/) serve para os testes.
$CXX -std=c++17 -O2 -DNDEBUG -Wall -Wextra -shared -o "build/Trok Shadows.asi" \
    src/main.cpp src/hooks.cpp src/stubs.cpp src/config.cpp src/patch.cpp src/log.cpp src/shaders.cpp \
    -static -static-libgcc -static-libstdc++ -luser32 -Wl,-Map=build/trok_shadows.map
i686-w64-mingw32-strip -o "dist/Trok Shadows.asi" "build/Trok Shadows.asi"

echo "ok: dist/Trok Shadows.asi ($(stat -c %s "dist/Trok Shadows.asi") bytes)"
