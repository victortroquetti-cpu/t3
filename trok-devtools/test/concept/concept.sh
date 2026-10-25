#!/usr/bin/env bash
# Imagens de conceito do Trok DevTools (Linux, sem o jogo): o menu de verdade sobre uma rua de mentira, com modelos
# low-poly desenhados aqui no lugar dos do jogo. Uso: test/concept/concept.sh [pasta de saida]
#   TROK_FONT: fonte da casa (sem ela, Inter Medium, a mais parecida com a Gotham Medium aqui)
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=$(realpath -m "${1:-test/out/concept}")
FONT=${TROK_FONT:-/usr/share/fonts/opentype/inter/Inter-Medium.otf}
KIT=../trok-ui/asi
I=$KIT/third_party/imgui
if [ ! -d "$I" ]; then
    git clone --depth 1 --branch v1.89.9 https://github.com/ocornut/imgui "$I"
fi
mkdir -p "$OUT/dados"
python3 -I test/concept/assets.py "$OUT/dados"
g++ -std=c++17 -O2 -DTROK_UI_TEST_FONT="\"$FONT\"" -Itest/concept/stub -I$KIT/src -I$I -Isrc \
    test/concept/concept.cpp $KIT/src/trok_ui.cpp $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp \
    -o "$OUT/dados/concept"
"$OUT/dados/concept" "$OUT/dados" "$OUT"
python3 -I -c "
import glob, os, sys
from PIL import Image
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.ppm'))):
    Image.open(f).save(f[:-4] + '.png'); os.remove(f)
" "$OUT"
