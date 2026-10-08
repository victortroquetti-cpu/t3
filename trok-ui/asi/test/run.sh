#!/usr/bin/env bash
# Renderiza o showcase em PNGs (Linux, sem o jogo). Uso: test/run.sh <pasta de saida>
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${1:-test/out}
# Use a fonte e o lucide da casa (moonloader\resource\trok) se tiver uma copia local.
FONT=${TROK_FONT:-/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf}
ICONS=${TROK_ICONS:-}
I=third_party/imgui
mkdir -p "$OUT"
ICON_FLAG=""
if [ -n "$ICONS" ]; then ICON_FLAG="-DTROK_UI_TEST_ICONS=\"$ICONS\""; fi
g++ -std=c++17 -O2 -DTROK_UI_TEST_FONT="\"$FONT\"" $ICON_FLAG -Itest/stub -I$I -Isrc test/render_test.cpp src/showcase.cpp \
    src/trok_ui.cpp $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp -o "$OUT/render_test"
"$OUT/render_test" "$OUT"
python3 -c "
import glob, os
from PIL import Image
for f in sorted(glob.glob('$OUT/*.ppm')):
    Image.open(f).save(f[:-4] + '.png'); os.remove(f)
"
rm -f "$OUT/render_test"
