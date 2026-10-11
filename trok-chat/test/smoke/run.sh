#!/usr/bin/env bash
# Smoke test do Trok Chat.asi no Wine 32 bits com Xvfb (ver o comentario no topo de fake_gta.cpp).
# Requer: wine (com wine32), xvfb-run, Mesa 32 bits e o MinGW i686.
# Uso: test/smoke/run.sh [asi] [pasta de saida]
#   TROK_FONT / TROK_ICONS: font.ttf e lucide.ttf da casa (opcionais; sem elas o .asi usa a Arial do Wine)
set -euo pipefail
cd "$(dirname "$0")/../.."
if [ -z "${1:-}" ]; then
    ./build.sh >/dev/null
fi
ASI=$(realpath "${1:-dist/Trok Chat.asi}")
OUT=$(realpath -m "${2:-test/out/smoke}")
mkdir -p "$OUT"

i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -masm=intel -static -s -o "$OUT/fake_gta.exe" test/smoke/fake_gta.cpp -ld3d9

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi

status=0
run() {
    local mode=$1
    local dir="$OUT/$mode"
    mkdir -p "$dir/moonloader/resource/trok"
    find "$dir" -maxdepth 1 -type f -delete
    cp "$OUT/fake_gta.exe" "$dir/"
    cp "$ASI" "$dir/Trok Chat.asi"
    case $mode in
        r1 | teclas | desligado) python3 test/smoke/make_fake_samp.py "$dir/samp.dll" ;;
        outra-versao) python3 test/smoke/make_fake_samp.py "$dir/samp.dll" --outra-versao ;;
    esac
    case $mode in
        teclas) printf '%s\r\n' "[TROK_CHAT]" "Tecla1=Y" "Tecla2=Enter" "Linhas=15" "Hora=1" "ModoF7=1" \
            > "$dir/Trok Chat.ini" ;;
        desligado) printf '%s\r\n' "[TROK_CHAT]" "Ligado=0" > "$dir/Trok Chat.ini" ;;
    esac
    if [ -n "${TROK_FONT:-}" ]; then cp "$TROK_FONT" "$dir/moonloader/resource/trok/font.ttf"; fi
    if [ -n "${TROK_ICONS:-}" ]; then cp "$TROK_ICONS" "$dir/moonloader/resource/trok/lucide.ttf"; fi
    echo "== $mode"
    (cd "$dir" && timeout 180 xvfb-run -a -s "-screen 0 1600x900x24" wine fake_gta.exe "Trok Chat.asi" "$mode") \
        2>/dev/null | tr -d '\r' || status=1
    sed 's/^/   log| /' "$dir/Trok Chat.log" 2>/dev/null | tr -d '\r' || echo "   (sem log do .asi)"
}
run r1
run teclas
run desligado
run outra-versao
run sem-samp
exit $status
