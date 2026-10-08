#!/usr/bin/env bash
# Testa o Trok Skin no Wine 32 bits com o cimguidx9.dll do mimgui 1.7.1 (compilado aqui das fontes dele).
# Requer: wine (com wine32), xvfb-run, Mesa 32 bits, MinGW i686, git, python3 + Pillow.
# Uso: test/run.sh [pasta de saida]
#   TROK_FONT: font.ttf da casa (sem ela, a fonte nao e testada)
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(realpath -m "${1:-test/out}")
mkdir -p "$OUT"

./build.sh >/dev/null

# cimguidx9.dll do mimgui 1.7.1 (mesmo commit, mesmos submodulos), com MinGW. O ImFileOpen do ImGui so usa
# caminhos em UTF-8 quando compilado com MSVC (como a DLL oficial): a copia abaixo liga esse caminho tambem.
MIMGUI=third_party/mimgui
MIMGUI_REV=a2fa7b1f589a2c5cb3f2aa042dd3066262cb4c90
if [ ! -f "$OUT/cimguidx9.dll" ] || [ ! -f "$OUT/imgui_utf8.cpp" ]; then
    if [ ! -d "$MIMGUI" ]; then
        git clone -q https://github.com/THE-FYP/mimgui "$MIMGUI"
        git -C "$MIMGUI" checkout -q "$MIMGUI_REV"
        git -C "$MIMGUI" submodule update -q --init --recursive
    fi
    C=$MIMGUI/LuaJIT-ImGui/cimgui
    mkdir -p "$OUT/shim"
    echo '#include <xinput.h>' > "$OUT/shim/XInput.h" # o fonte usa o nome com maiusculas do Windows
    sed 's/defined(_WIN32) \&\& !defined(__CYGWIN__) \&\& !defined(__GNUC__)/defined(_WIN32)/' \
        $C/imgui/imgui.cpp > "$OUT/imgui_utf8.cpp"
    i686-w64-mingw32-g++ -std=c++11 -O2 -shared -o "$OUT/cimguidx9.dll" \
        -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1 '-DIMGUI_IMPL_API=extern "C" __declspec(dllexport)' -D_WIN32_WINNT=0x0501 \
        -I"$OUT/shim" -I$C -I$C/imgui -I$MIMGUI/imgui-impl \
        $C/cimgui.cpp "$OUT/imgui_utf8.cpp" $C/imgui/imgui_draw.cpp $C/imgui/imgui_demo.cpp $C/imgui/imgui_widgets.cpp \
        $MIMGUI/imgui-impl/dx9_win32/imgui_impl_dx9.cpp $MIMGUI/imgui-impl/dx9_win32/imgui_impl_win32.cpp \
        -static -static-libgcc -static-libstdc++ -ld3d9 -ld3dx9 -lxinput9_1_0 -luser32 -limm32
fi

i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -Ithird_party/imgui-1.72 -static -s -o "$OUT/skin_host.exe" \
    test/skin_host.cpp -ld3d9

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi
# A fonte padrao do mimgui e a Trebuchet Bold do Windows; no teste, a Liberation Sans Bold no lugar dela.
cp /usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf "$WINEPREFIX/drive_c/windows/Fonts/trebucbd.ttf"

# Outra versao do ImGui no mimgui: a mesma DLL dizendo "1.99" no igGetVersion. A skin tem que recusar.
python3 - "$OUT/cimguidx9.dll" "$OUT/cimguidx9_v199.dll" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
assert data.count(b'\x001.72\x00') >= 1
open(sys.argv[2], 'wb').write(data.replace(b'\x001.72\x00', b'\x001.99\x00'))
PY

run() {
    local name=$1 skin=$2 ini=$3 dll=${4:-cimguidx9.dll}
    local dir="$OUT/$name"
    rm -rf "$dir"
    mkdir -p "$dir/moonloader/resource/trok"
    cp "$OUT/skin_host.exe" "dist/Trok Skin.asi" "$dir/"
    if [ "${LAYOUT_ASI:-}" = 1 ]; then cp "dist/Trok Skin Layout.asi" "$dir/Trok Skin.asi"; fi # versao de teste
    cp test/moonloader/*.lua "$dir/moonloader/" # a skin le o arquivo de cada script (mod da casa ou nao)
    cp "$OUT/$dll" "$dir/cimguidx9.dll"
    python3 ../asi/test/smoke/make_fake_samp.py "$dir/samp.dll"
    if [ -n "${TROK_FONT:-}" ]; then cp "$TROK_FONT" "$dir/moonloader/resource/trok/font.ttf"; fi
    printf '[skin]\n%s\n' "$ini" > "$dir/Trok Skin.ini"
    (cd "$dir" && timeout 120 xvfb-run -a -s "-screen 0 1280x800x24" wine skin_host.exe out $skin ${5:-}) 2>/dev/null | tr -d '\r'
}
run base "" ""
run tema skin $'tema=1\nfonte=0'
run completo skin $'tema=1\nfonte=1'
run desligado skin $'tema=0\nfonte=0'
run manter skin $'tema=1\nfonte=0\nmanter=Painel B'
run versao skin $'tema=1\nfonte=1' cimguidx9_v199.dll
run alternar skin $'tema=1\nfonte=0' cimguidx9.dll alternar
run "acentuação" skin $'tema=1\nfonte=1'
run manter_script skin $'tema=1\nfonte=1\nmanter_scripts=painel_b.lua'
# Versao de teste (Trok Skin Layout.asi, layout=1 de fabrica): tamanho de fonte e espacamentos da casa.
LAYOUT_ASI=1 run layout skin $'tema=1\nfonte=1'

python3 test/compare.py "$OUT"
