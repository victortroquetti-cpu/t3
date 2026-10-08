#!/usr/bin/env bash
# Testa o Trok Skin no imgui antigo do moonloader (moon_imgui 1.1.5: lib\imgui.lua + lib\MoonImGui.dll, Dear ImGui
# 1.52) no Wine 32 bits: o imgui.lua e o MoonImGui.dll de verdade, cada script no seu lua_State do LuaJIT
# (lua51.dll, compilado aqui) como no moonloader.
# Requer: wine (com wine32), xvfb-run, Mesa 32 bits, MinGW i686, gcc com multilib (-m32, para gerar o LuaJIT),
#         git, curl, python3 + Pillow.
# Uso: test/moon/run.sh [pasta de saida]
#   TROK_FONT: font.ttf da casa (sem ela, a fonte nao e testada)
#   MOONIMGUI_DIR: pasta com o imgui.lua e o MoonImGui.dll (o moonloader\lib do jogo). Sem ela, baixa uma copia
#                  publica do 1.1.5 e confere o SHA-256 (o moon_imgui e fechado: nao vai no repositorio).
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=$(realpath -m "${1:-test/out/moon}")
mkdir -p "$OUT"

./build.sh >/dev/null

# LuaJIT 2.1 (o lua51.dll do moonloader e um LuaJIT), 32 bits para Windows.
LUAJIT=third_party/luajit
LUAJIT_REV=c6ffc141a8762b41703f9287d63d93622a13dd8f
if [ ! -f "$OUT/lua51.dll" ]; then
    if [ ! -f "$LUAJIT/src/luajit.c" ]; then
        rm -rf "$LUAJIT"
        git init -q "$LUAJIT"
        git -C "$LUAJIT" fetch -q --depth 1 https://github.com/LuaJIT/LuaJIT "$LUAJIT_REV"
        git -C "$LUAJIT" checkout -q FETCH_HEAD
    fi
    make -C "$LUAJIT" -s clean >/dev/null
    make -C "$LUAJIT" -s -j8 HOST_CC="gcc -m32" CROSS=i686-w64-mingw32- TARGET_SYS=Windows BUILDMODE=dynamic \
        LDFLAGS=-static-libgcc >/dev/null
    cp "$LUAJIT/src/lua51.dll" "$OUT/"
fi

MOON=${MOONIMGUI_DIR:-third_party/moonimgui}
if [ -z "${MOONIMGUI_DIR:-}" ] && [ ! -f "$MOON/MoonImGui.dll" ]; then
    mkdir -p "$MOON"
    URL=https://raw.githubusercontent.com/SoMiK3/FastLinkLibs/main
    curl -sSfL -o "$MOON/imgui.lua" "$URL/imgui.lua"
    curl -sSfL -o "$MOON/MoonImGui.dll" "$URL/MoonImGui.dll"
    printf '%s  %s\n' 8da7a6498d493cc36f413b810b171e5d7e44834b9db4cfbe19e412bb1040e614 "$MOON/imgui.lua" \
        6cc52dfe21df3725cb1c5aa02d4041d830c540bc1e00299a6de6e9c735ab9b1a "$MOON/MoonImGui.dll" | sha256sum -c --quiet
fi

# Outra versao do ImGui no moon_imgui: o mesmo DLL dizendo "1.99" no GetVersion. A skin tem que recusar.
python3 - "$MOON/MoonImGui.dll" "$OUT/MoonImGui_v199.dll" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
assert data.count(b'\x001.52\x00') == 1
open(sys.argv[2], 'wb').write(data.replace(b'\x001.52\x00', b'\x001.99\x00'))
PY

i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -static -s -o "$OUT/moon_host.exe" test/moon/moon_host.cpp -ld3d9

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi
# A fonte padrao do moon_imgui e a Trebuchet Bold do Windows; no teste, a Liberation Sans Bold no lugar dela.
cp /usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf "$WINEPREFIX/drive_c/windows/Fonts/trebucbd.ttf"

run() {
    local name=$1 skin=$2 ini=$3 dll=${4:-$MOON/MoonImGui.dll}
    local dir="$OUT/$name"
    rm -rf "$dir"
    mkdir -p "$dir/moonloader/resource/trok"
    cp "$OUT/moon_host.exe" "$OUT/lua51.dll" "dist/Trok Skin.asi" "$dir/"
    cp -r test/moon/moonloader/. "$dir/moonloader/"
    cp "$MOON/imgui.lua" "$dir/moonloader/lib/imgui.lua"
    cp "$dll" "$dir/moonloader/lib/MoonImGui.dll"
    python3 ../asi/test/smoke/make_fake_samp.py "$dir/samp.dll"
    if [ -n "${TROK_FONT:-}" ]; then cp "$TROK_FONT" "$dir/moonloader/resource/trok/font.ttf"; fi
    printf '[skin]\n%s\n' "$ini" > "$dir/Trok Skin.ini"
    # Um assert do ImGui 1.52 abre uma caixa de mensagem e trava: o timeout derruba e o compare.py acusa.
    (cd "$dir" && timeout 120 xvfb-run -a -s "-screen 0 1280x800x24" wine moon_host.exe out $skin ${5:-}) 2>/dev/null |
        tr -d '\r' || true
}
run base "" ""
run tema skin $'tema=1\nfonte=0'
run completo skin $'tema=1\nfonte=1'
run desligado skin $'tema=0\nfonte=0'
run manter skin $'tema=1\nfonte=0\nmanter=Painel B'
run versao skin $'tema=1\nfonte=1' "$OUT/MoonImGui_v199.dll"
run alternar skin $'tema=1\nfonte=0' "" alternar
run "acentuação" skin $'tema=1\nfonte=1'
run manter_script skin $'tema=1\nfonte=1\nmanter_scripts=painel_b.lua'

python3 test/moon/compare.py "$OUT"
