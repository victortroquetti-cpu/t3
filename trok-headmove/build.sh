#!/usr/bin/env bash
# Compila o Trok Headmove.asi com MinGW (i686, 32 bits como o gta_sa.exe).
# Requer: i686-w64-mingw32-g++ / -gcc e git. Usa o mesmo MinHook dos mods da casa (../trok-ui/asi/third_party).
set -euo pipefail
cd "$(dirname "$0")"

KIT=../trok-ui/asi
MINHOOK=$KIT/third_party/minhook
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

# O .asi sem simbolos vai para dist/; a copia com simbolos (build/) serve para os testes.
$CXX -std=c++17 -O2 -DNDEBUG -Wall -Wextra -I$MINHOOK/include -shared -o "build/Trok Headmove.asi" \
    src/main.cpp src/config.cpp $MINHOOK_OBJECTS -static -static-libgcc -static-libstdc++
i686-w64-mingw32-strip -o "dist/Trok Headmove.asi" "build/Trok Headmove.asi"

echo "ok: dist/Trok Headmove.asi ($(stat -c %s "dist/Trok Headmove.asi") bytes)"
