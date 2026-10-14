#!/usr/bin/env bash
# Imagens de conceito do Trok Hitmarker (Linux, sem o jogo). Uso: test/concept.sh [pasta de saida]
#   TROK_FONT: fonte da casa (sem ela, Inter Medium, a mais parecida com a Gotham Medium aqui)
#   NUMBER_FONT: fonte dos numeros (no jogo, Arial Black; aqui, Inter ExtraBold)
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(realpath -m "${1:-test/out}")
FONT=${TROK_FONT:-/usr/share/fonts/opentype/inter/Inter-Medium.otf}
NUMBERS=${NUMBER_FONT:-/usr/share/fonts/opentype/inter/Inter-ExtraBold.otf}
ASI=../trok-ui/asi
I=$ASI/third_party/imgui
if [ ! -d "$I" ]; then
    (cd "$ASI" && ./build.sh >/dev/null)
fi
mkdir -p "$OUT"
python3 -I test/scene.py "$OUT/fundo.png"
python3 -I -c "
import sys
from PIL import Image
open(sys.argv[2], 'wb').write(Image.open(sys.argv[1]).convert('RGB').tobytes())
" "$OUT/fundo.png" "$OUT/fundo.rgb"
g++ -std=c++17 -O2 -DTROK_UI_TEST_FONT="\"$FONT\"" -DNUMBER_FONT="\"$NUMBERS\"" -I$ASI/test/stub -I$I -I$ASI/src -Isrc \
    test/concept.cpp src/hud.cpp src/menu.cpp src/options.cpp $ASI/src/trok_ui.cpp \
    $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp -o "$OUT/concept"
"$OUT/concept" "$OUT" "$OUT/fundo.rgb"
python3 -I -c "
import glob, os, sys
from PIL import Image
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.ppm'))):
    Image.open(f).save(f[:-4] + '.png'); os.remove(f)
" "$OUT"
rm -f -- "$OUT/concept" "$OUT/fundo.rgb"
