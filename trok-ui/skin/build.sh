#!/usr/bin/env bash
# Compila o Trok Skin.asi com MinGW (i686, 32 bits como o gta_sa.exe).
# Requer: i686-w64-mingw32-g++ / -gcc e git.
set -euo pipefail
cd "$(dirname "$0")"

# Headers do Dear ImGui no MESMO commit que o mimgui 1.7.1 usa (1.72): a skin le e escreve os structs do
# ImGui da DLL do mimgui, entao o layout tem que ser identico. Nada do ImGui e compilado aqui.
IMGUI=third_party/imgui-1.72
IMGUI_REV=ecb9b1e2eba5becc38019b5c9f75fba711cd881b
if [ ! -f "$IMGUI/imgui.h" ]; then
    rm -rf "$IMGUI"
    git init -q "$IMGUI"
    git -C "$IMGUI" fetch -q --depth 1 https://github.com/ocornut/imgui "$IMGUI_REV"
    git -C "$IMGUI" checkout -q FETCH_HEAD
fi
grep -q '#define IMGUI_VERSION               "1.72"' "$IMGUI/imgui.h"

# MinHook: o mesmo do Trok UI Showcase.
MINHOOK=../asi/third_party/minhook
MINHOOK_REV=8af6b4acae5a9388fd742b56fa79ece89d96f823
if [ ! -d "$MINHOOK" ]; then
    git clone https://github.com/TsudaKageyu/minhook "$MINHOOK"
    git -C "$MINHOOK" checkout -q "$MINHOOK_REV"
fi

mkdir -p build/minhook dist
CXX=${CXX:-i686-w64-mingw32-g++}
CC=${CC:-i686-w64-mingw32-gcc}

MINHOOK_OBJECTS=""
for f in buffer.c hook.c trampoline.c hde/hde32.c; do
    obj="build/minhook/$(basename "$f" .c).o"
    $CC -O2 -DNDEBUG -I$MINHOOK/include -c "$MINHOOK/src/$f" -o "$obj"
    MINHOOK_OBJECTS="$MINHOOK_OBJECTS $obj"
done

$CXX -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Wno-unused-function -I$IMGUI -I$MINHOOK/include -shared \
    -o "dist/Trok Skin.asi" src/skin.cpp $MINHOOK_OBJECTS \
    -static -static-libgcc -static-libstdc++ -s -luser32

echo "ok: dist/Trok Skin.asi ($(stat -c %s "dist/Trok Skin.asi") bytes)"
