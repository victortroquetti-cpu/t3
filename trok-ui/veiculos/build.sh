#!/usr/bin/env bash
# Compila o Trok Veiculos.asi com MinGW (i686, 32 bits como o gta_sa.exe).
# Requer: i686-w64-mingw32-g++ / -gcc e git. Usa o kit e as dependencias da vitrine .asi (../asi).
set -euo pipefail
cd "$(dirname "$0")"

ASI=../asi
# Dear ImGui 1.89.9 e MinHook: os mesmos da vitrine (o build dela baixa os dois).
if [ ! -d "$ASI/third_party/imgui" ] || [ ! -d "$ASI/third_party/minhook" ]; then
    (cd "$ASI" && ./build.sh >/dev/null)
fi
IMGUI=$ASI/third_party/imgui
MINHOOK=$ASI/third_party/minhook

mkdir -p build/minhook dist
CXX=${CXX:-i686-w64-mingw32-g++}
CC=${CC:-i686-w64-mingw32-gcc}
FLAGS="-std=c++17 -O2 -DNDEBUG -DIMGUI_DISABLE_DEMO_WINDOWS -DIMGUI_DISABLE_DEBUG_TOOLS -I$IMGUI -I$IMGUI/backends -I$MINHOOK/include -I$ASI/src -Isrc"

MINHOOK_OBJECTS=""
for f in buffer.c hook.c trampoline.c hde/hde32.c; do
    obj="build/minhook/$(basename "$f" .c).o"
    $CC -O2 -DNDEBUG -I$MINHOOK/include -c "$MINHOOK/src/$f" -o "$obj"
    MINHOOK_OBJECTS="$MINHOOK_OBJECTS $obj"
done

SOURCES="src/main.cpp src/samp.cpp src/overlay.cpp src/menu.cpp src/config.cpp src/names.cpp $ASI/src/trok_ui.cpp \
 $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp \
 $IMGUI/backends/imgui_impl_dx9.cpp $IMGUI/backends/imgui_impl_win32.cpp"

$CXX $FLAGS -shared -o "dist/Trok Veiculos.asi" $SOURCES $MINHOOK_OBJECTS \
    -static -static-libgcc -static-libstdc++ -s \
    -ld3d9 -limm32 -ldwmapi -lgdi32 -luser32 -lwinmm

echo "ok: dist/Trok Veiculos.asi ($(stat -c %s "dist/Trok Veiculos.asi") bytes)"
