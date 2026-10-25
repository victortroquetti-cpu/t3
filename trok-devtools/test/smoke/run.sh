#!/usr/bin/env bash
# Smoke test do Trok DevTools.asi no Wine 32 bits com Xvfb (ver o comentario no topo de fake_gta.cpp).
# Requer: wine (com wine32), xvfb-run, Mesa 32 bits, o MinGW i686 e python3 (para converter as capturas em PNG).
# Uso: test/smoke/run.sh [asi] [pasta de saida]
#   TROK_FONT / TROK_ICONS: font.ttf e lucide.ttf da casa (opcionais; sem eles o .asi usa outra fonte)
set -euo pipefail
cd "$(dirname "$0")/../.."
ASI=$(realpath "${1:-build/Trok DevTools.asi}")
OUT=$(realpath -m "${2:-test/out/smoke}")
mkdir -p "$OUT"
find "$OUT" -maxdepth 1 -type f -delete

# O .exe minimo (a memoria do gta_sa.exe) e o GTA de mentira numa DLL ao lado.
i686-w64-mingw32-gcc -O2 -s -nostdlib -e _start -o "$OUT/fake_gta.exe" test/smoke/host.c -lkernel32
i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -shared -static -s -o "$OUT/fake_core.dll" test/smoke/fake_gta.cpp -ld3d9
i686-w64-mingw32-objdump -h "$OUT/fake_gta.exe" | awk '$2 == ".bss" {print "   memoria falsa do GTA a partir de 0x" $4}'

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi

cp "$ASI" "$OUT/Trok DevTools.asi"
mkdir -p "$OUT/moonloader/resource/trok"
FONT=${TROK_FONT:-}
if [ -z "$FONT" ]; then
    for f in /usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf; do
        if [ -f "$f" ]; then FONT=$f; break; fi
    done
fi
if [ -n "$FONT" ]; then cp "$FONT" "$OUT/moonloader/resource/trok/font.ttf"; fi
if [ -n "${TROK_ICONS:-}" ]; then cp "$TROK_ICONS" "$OUT/moonloader/resource/trok/lucide.ttf"; fi
# O inspetor e a colisao ligados, como se o usuario ja tivesse escolhido.
printf '[devtools]\r\ninspecionar=1\r\nvisual_colisao=1\r\n' >"$OUT/Trok DevTools.ini"

status=0
(cd "$OUT" && timeout 300 xvfb-run -a -s "-screen 0 1920x1080x24" wine fake_gta.exe "Trok DevTools.asi" .) 2>/dev/null \
    | tr -d '\r' || status=1
sed 's/^/   log| /' "$OUT/Trok DevTools.log" 2>/dev/null | tr -d '\r' || echo "   (sem log do .asi)"
for bmp in "$OUT"/*.bmp; do
    [ -f "$bmp" ] || continue
    python3 -I -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])" "$bmp" "${bmp%.bmp}.png" \
        && rm -f "$bmp"
done
exit $status
