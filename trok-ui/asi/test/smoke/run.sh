#!/usr/bin/env bash
# Smoke test do .asi no Wine 32 bits com Xvfb (ver o comentario no topo de fake_gta.cpp).
# Requer: wine (com wine32), xvfb-run, Mesa 32 bits e o MinGW i686.
# Uso: test/smoke/run.sh [asi] [pasta de saida]
#   TROK_FONT / TROK_ICONS: font.ttf e lucide.ttf da casa (opcionais; sem elas o .asi usa a Arial do Wine)
set -euo pipefail
cd "$(dirname "$0")/../.."
ASI=$(realpath "${1:-dist/Trok UI Showcase.asi}")
OUT=$(realpath -m "${2:-test/out/smoke}")
mkdir -p "$OUT/moonloader/resource/trok"

i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -static -s -o "$OUT/fake_gta.exe" test/smoke/fake_gta.cpp -ld3d9
python3 test/smoke/make_fake_samp.py "$OUT/samp.dll"
cp "$ASI" "$OUT/showcase.asi"
if [ -n "${TROK_FONT:-}" ]; then cp "$TROK_FONT" "$OUT/moonloader/resource/trok/font.ttf"; fi
if [ -n "${TROK_ICONS:-}" ]; then cp "$TROK_ICONS" "$OUT/moonloader/resource/trok/lucide.ttf"; fi

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi

status=0
run() {
    local name=$1
    shift
    echo "== $name"
    rm -f "$OUT/Trok UI Showcase.log"
    (cd "$OUT" && timeout 120 xvfb-run -a -s "-screen 0 1600x900x24" wine fake_gta.exe showcase.asi "$@") 2>/dev/null || status=1
    mkdir -p "$OUT/$name"
    mv -f "$OUT"/menu_*.bmp "$OUT/$name/" 2>/dev/null || true
    cp "$OUT/Trok UI Showcase.log" "$OUT/$name/" 2>/dev/null || true
    sed 's/^/   log| /' "$OUT/$name/Trok UI Showcase.log" 2>/dev/null || echo "   (sem log do .asi)"
}
run direto direto
run samp-mods samp-mods
run so-janela-do-d3d samp-mods so-janela-do-d3d
run so-classe samp-mods so-classe
run wrapper wrapper
exit $status
