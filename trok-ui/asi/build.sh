#!/usr/bin/env bash
# Compila o Trok UI Showcase.asi com MinGW (i686, 32 bits como o gta_sa.exe).
# Requer: i686-w64-mingw32-g++ (pacote g++-mingw-w64-i686-posix) e git.
set -euo pipefail
cd "$(dirname "$0")"

IMGUI=third_party/imgui
if [ ! -d "$IMGUI" ]; then
    git clone --depth 1 --branch v1.89.9 https://github.com/ocornut/imgui "$IMGUI"
fi

mkdir -p build dist
CXX=${CXX:-i686-w64-mingw32-g++}
FLAGS="-std=c++17 -O2 -DNDEBUG -DIMGUI_DISABLE_DEMO_WINDOWS -DIMGUI_DISABLE_DEBUG_TOOLS -I$IMGUI -I$IMGUI/backends -Isrc"

SOURCES="src/main.cpp src/showcase.cpp src/trok_ui.cpp \
 $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp \
 $IMGUI/backends/imgui_impl_dx9.cpp $IMGUI/backends/imgui_impl_win32.cpp"

$CXX $FLAGS -shared -o "dist/Trok UI Showcase.asi" $SOURCES \
    -static -static-libgcc -static-libstdc++ -s \
    -ld3d9 -limm32 -ldwmapi -lgdi32 -luser32

echo "ok: dist/Trok UI Showcase.asi ($(stat -c %s "dist/Trok UI Showcase.asi") bytes)"
