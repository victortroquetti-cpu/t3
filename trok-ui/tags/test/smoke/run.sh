#!/usr/bin/env bash
# Smoke test do Trok Tags.asi no Wine 32 bits com Xvfb (ver o comentario no topo de fake_gta.cpp).
# Requer: wine (com wine32), xvfb-run, Mesa 32 bits, o MinGW i686 e python3.
# Uso: test/smoke/run.sh [asi] [pasta de saida]
#   TROK_FONT / TROK_ICONS: font.ttf e lucide.ttf da casa (opcionais; sem elas o .asi usa a Arial do Wine)
set -euo pipefail
cd "$(dirname "$0")/../.."
ASI=$(realpath "${1:-dist/Trok Tags.asi}")
OUT=$(realpath -m "${2:-test/out/smoke}")
mkdir -p "$OUT/moonloader/resource/trok"

i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -static -s -o "$OUT/fake_gta.exe" test/smoke/fake_gta.cpp -ld3d9
cp "$ASI" "$OUT/Trok Tags.asi"
if [ -n "${TROK_FONT:-}" ]; then cp "$TROK_FONT" "$OUT/moonloader/resource/trok/font.ttf"; fi
if [ -n "${TROK_ICONS:-}" ]; then cp "$TROK_ICONS" "$OUT/moonloader/resource/trok/lucide.ttf"; fi

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi

status=0
run() {
    local name=$1
    echo "== $name"
    rm -f "$OUT/Trok Tags.log" "$OUT/Trok Tags.ini" "$OUT"/*.bmp
    python3 test/smoke/make_fake_samp.py "$OUT/samp.dll" $([ "$name" = outra-versao ] && echo r3)
    if [ "$name" = desligado ]; then printf '[padrao]\r\nligado=0\r\n' >"$OUT/Trok Tags.ini"; fi
    if [ "$name" = sigla-id ]; then
        printf '[padrao]\r\nsigla_fixo=1\r\nid_fixo=1\r\npalavra_fixo=2\r\n' >"$OUT/Trok Tags.ini"
    fi
    (cd "$OUT" && timeout 120 xvfb-run -a -s "-screen 0 1600x900x24" wine fake_gta.exe "Trok Tags.asi" "$name") 2>/dev/null ||
        status=1
    mkdir -p "$OUT/$name"
    mv -f "$OUT"/*.bmp "$OUT/$name/" 2>/dev/null || true
    cp "$OUT/Trok Tags.log" "$OUT/$name/" 2>/dev/null || true
    sed 's/^/   log| /' "$OUT/$name/Trok Tags.log" 2>/dev/null || echo "   (sem log do .asi)"
}
for scenario in ${SCENARIOS:-direto samp-mods desligado outra-versao baloes-estranhos sigla-id}; do
    run "$scenario"
done
exit $status
