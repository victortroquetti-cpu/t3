#!/usr/bin/env bash
# Desenha o menu em PNGs (Linux, sem o jogo), com os valores do shadows.ini do Victor_Trok.
# Uso: test/render.sh [pasta de saida]
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(realpath -m "${1:-test/out/menu}")
KIT=../trok-ui/asi
I=$KIT/third_party/imgui
if [ ! -d "$I" ]; then
    git clone --depth 1 --branch v1.89.9 https://github.com/ocornut/imgui "$I"
fi
# A fonte do jogo e a Arial (ou a da casa, se houver): a Liberation Sans tem as mesmas medidas.
FONT=${TROK_FONT:-/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf}
ICONS=${TROK_ICONS:-}
ICON_FLAG=""
if [ -n "$ICONS" ]; then ICON_FLAG="-DTROK_UI_TEST_ICONS=\"$ICONS\""; fi
mkdir -p "$OUT"
rm -f "$OUT"/*.png
g++ -std=c++17 -O2 -DTROK_UI_TEST_FONT="\"$FONT\"" $ICON_FLAG -I$KIT/test/stub -I$I -I$KIT/src -Isrc \
    test/render_test.cpp src/menu.cpp $KIT/src/trok_ui.cpp $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp \
    $I/imgui_widgets.cpp -o "$OUT/render_test"
"$OUT/render_test" "$OUT"
python3 -c "
import glob, os
from PIL import Image
for f in sorted(glob.glob('$OUT/*.ppm')):
    Image.open(f).save(f[:-4] + '.png'); os.remove(f)
"
rm -f "$OUT/render_test"
