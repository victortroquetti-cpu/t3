#!/usr/bin/env bash
# Testa o Trok Shadows.asi no Wine 32 bits, sem o jogo: o launcher.exe ocupa a faixa do gta_sa.exe e o host.dll
# monta nela a memoria do 1.0 US que o mod confere, carrega o .asi, dispara os eventos e chama cada gancho (ver o
# comentario no topo de test/host.cpp).
# Requer: wine (com wine32), xvfb-run (o device D3D9 do Wine valida os shaders), MinGW i686.
# Uso: test/run.sh [pasta de saida]
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(realpath -m "${1:-test/out}")
mkdir -p "$OUT"

./build.sh >/dev/null
i686-w64-mingw32-gcc -O1 -Wall -o "$OUT/launcher.exe" test/launcher.c -Wl,--image-base=0x400000 \
    -Wl,--disable-dynamicbase
i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -shared -static -o "$OUT/host.dll" test/host.cpp \
    -ld3d9 -Wl,--image-base=0x30000000
# O "shadows.asi" do teste de conflito: qualquer DLL com esse nome.
printf 'int trok_dummy;\n' > "$OUT/dummy.c"
i686-w64-mingw32-gcc -shared -o "$OUT/dummy.dll" "$OUT/dummy.c"

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi

status=0
run() {
    local mode=$1
    local dir="$OUT/$mode"
    rm -rf "$dir"
    mkdir -p "$dir"
    cp "$OUT/launcher.exe" "$OUT/host.dll" "dist/Trok Shadows.asi" "$dir/"
    case $mode in
        completo) cp test/shadows.ini "$dir/" ;; # o shadows.ini do Shadows Extender: o mod importa os valores
        conflito) cp "$OUT/dummy.dll" "$dir/shadows.asi" ;;
    esac
    echo "=== $mode"
    (cd "$dir" && timeout 120 xvfb-run -a -s "-screen 0 640x480x24" wine launcher.exe "$mode") 2>/dev/null |
        tr -d '\r' || status=1
}
run completo
run errado
run conflito
run atualizar
exit $status
