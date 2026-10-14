#!/usr/bin/env bash
# Compila o Trok Hitmarker.asi com MinGW (i686, 32 bits como o gta_sa.exe).
# Requer: i686-w64-mingw32-g++ / -gcc, python3 e git. Usa o kit da casa (../trok-ui/asi/src/trok_ui.cpp) e o mesmo
# Dear ImGui e MinHook da vitrine do Trok UI. As imagens e os sons padrao (dist/Trok Hitmarker) vao para dentro do .asi.
set -euo pipefail
cd "$(dirname "$0")"

KIT=../trok-ui/asi
IMGUI=$KIT/third_party/imgui
if [ ! -d "$IMGUI" ]; then
    git clone --depth 1 --branch v1.89.9 https://github.com/ocornut/imgui "$IMGUI"
fi
MINHOOK=$KIT/third_party/minhook
MINHOOK_REV=8af6b4acae5a9388fd742b56fa79ece89d96f823
if [ ! -d "$MINHOOK" ]; then
    git clone https://github.com/TsudaKageyu/minhook "$MINHOOK"
    git -C "$MINHOOK" checkout -q "$MINHOOK_REV"
fi

mkdir -p build/minhook build/gen dist
CXX=${CXX:-i686-w64-mingw32-g++}
CC=${CC:-i686-w64-mingw32-gcc}
FLAGS="-std=c++17 -O2 -DNDEBUG -DIMGUI_DISABLE_DEMO_WINDOWS -DIMGUI_DISABLE_DEBUG_TOOLS -I$IMGUI -I$IMGUI/backends \
 -I$MINHOOK/include -I$KIT/src -Isrc -Ithird_party/minimp3 -Ithird_party/stb"

python3 -I res/embed.py "dist/Trok Hitmarker" build/gen/res_data.cpp

MINHOOK_OBJECTS=""
for f in buffer.c hook.c trampoline.c hde/hde32.c; do
    obj="build/minhook/$(basename "$f" .c).o"
    $CC -O2 -DNDEBUG -I$MINHOOK/include -c "$MINHOOK/src/$f" -o "$obj"
    MINHOOK_OBJECTS="$MINHOOK_OBJECTS $obj"
done

SOURCES="src/main.cpp src/samp.cpp src/game.cpp src/logic.cpp src/hud.cpp src/menu.cpp src/options.cpp src/config.cpp \
 src/images.cpp src/sound.cpp src/res.cpp build/gen/res_data.cpp $KIT/src/trok_ui.cpp \
 $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp \
 $IMGUI/backends/imgui_impl_dx9.cpp $IMGUI/backends/imgui_impl_win32.cpp"

# O .asi sem simbolos vai para dist/; a copia com simbolos (build/) serve para os testes.
$CXX $FLAGS -Wall -shared -o "build/Trok Hitmarker.asi" $SOURCES $MINHOOK_OBJECTS \
    -static -static-libgcc -static-libstdc++ -ld3d9 -ldsound -limm32 -ldwmapi -lgdi32 -luser32 -lshell32 -lwinmm
i686-w64-mingw32-strip -o "dist/Trok Hitmarker.asi" "build/Trok Hitmarker.asi"

echo "ok: dist/Trok Hitmarker.asi ($(stat -c %s "dist/Trok Hitmarker.asi") bytes)"
