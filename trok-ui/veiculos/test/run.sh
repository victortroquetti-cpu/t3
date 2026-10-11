#!/usr/bin/env bash
# Renderiza o Trok Veiculos em PNGs (Linux, sem o jogo). Uso: test/run.sh <pasta de saida> [fundo.png]
#   TROK_FONT / TROK_ICONS: font.ttf e lucide.ttf da casa (sem eles, Liberation Sans e sem icones)
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(realpath -m "${1:-test/out}")
FONT=${TROK_FONT:-/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf}
ICONS=${TROK_ICONS:-}
ASI=../asi
I=$ASI/third_party/imgui
if [ ! -d "$I" ]; then
    (cd "$ASI" && ./build.sh >/dev/null)
fi
mkdir -p "$OUT"
ICON_FLAG=""
if [ -n "$ICONS" ]; then ICON_FLAG="-DTROK_UI_TEST_ICONS=\"$ICONS\""; fi
g++ -std=c++17 -O2 -DTROK_UI_TEST_FONT="\"$FONT\"" $ICON_FLAG -I$ASI/test/stub -I$I -I$ASI/src -Isrc \
    test/render_test.cpp src/overlay.cpp src/menu.cpp $ASI/src/trok_ui.cpp \
    $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp -o "$OUT/render_test"
BG=""
if [ -n "${2:-}" ]; then
    python3 -I -c "
import sys
from PIL import Image
open(sys.argv[2], 'wb').write(Image.open(sys.argv[1]).convert('RGB').resize((1920, 1080)).tobytes())
" "$2" "$OUT/fundo.rgb"
    BG="$OUT/fundo.rgb"
fi
"$OUT/render_test" "$OUT" $BG
python3 -I -c "
import glob, os, sys
from PIL import Image
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.ppm'))):
    Image.open(f).save(f[:-4] + '.png'); os.remove(f)
" "$OUT"
rm -f "$OUT/render_test" "$OUT/fundo.rgb"
