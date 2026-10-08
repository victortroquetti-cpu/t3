#!/usr/bin/env bash
# Compila o Trok UI Showcase.asi com MinGW (i686, 32 bits como o gta_sa.exe).
# Requer: i686-w64-mingw32-g++ / -gcc (pacotes g++-mingw-w64-i686-posix e gcc-mingw-w64-i686-posix) e git.
set -euo pipefail
cd "$(dirname "$0")"

IMGUI=third_party/imgui
if [ ! -d "$IMGUI" ]; then
    git clone --depth 1 --branch v1.89.9 https://github.com/ocornut/imgui "$IMGUI"
fi

# MinHook desvia o Present/Reset do d3d9.dll (ver o comentario no topo de src/main.cpp).
MINHOOK=third_party/minhook
MINHOOK_REV=8af6b4acae5a9388fd742b56fa79ece89d96f823
if [ ! -d "$MINHOOK" ]; then
    git clone https://github.com/TsudaKageyu/minhook "$MINHOOK"
    git -C "$MINHOOK" checkout -q "$MINHOOK_REV"
fi

mkdir -p build/minhook dist
CXX=${CXX:-i686-w64-mingw32-g++}
CC=${CC:-i686-w64-mingw32-gcc}
FLAGS="-std=c++17 -O2 -DNDEBUG -DIMGUI_DISABLE_DEMO_WINDOWS -DIMGUI_DISABLE_DEBUG_TOOLS -I$IMGUI -I$IMGUI/backends -I$MINHOOK/include -Isrc"

MINHOOK_OBJECTS=""
for f in buffer.c hook.c trampoline.c hde/hde32.c; do
    obj="build/minhook/$(basename "$f" .c).o"
    $CC -O2 -DNDEBUG -I$MINHOOK/include -c "$MINHOOK/src/$f" -o "$obj"
    MINHOOK_OBJECTS="$MINHOOK_OBJECTS $obj"
done

SOURCES="src/main.cpp src/showcase.cpp src/trok_ui.cpp \
 $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp \
 $IMGUI/backends/imgui_impl_dx9.cpp $IMGUI/backends/imgui_impl_win32.cpp"

$CXX $FLAGS -shared -o "dist/Trok UI Showcase.asi" $SOURCES $MINHOOK_OBJECTS \
    -static -static-libgcc -static-libstdc++ -s \
    -ld3d9 -limm32 -ldwmapi -lgdi32 -luser32

echo "ok: dist/Trok UI Showcase.asi ($(stat -c %s "dist/Trok UI Showcase.asi") bytes)"
