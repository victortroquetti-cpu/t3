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
# as imagens padrao (res/mira), em .rgba para o teste: largura, altura (int32) e os pixels RGBA
python3 -I res/make_images.py "$OUT/mira"
python3 -I -c "
import os, struct, sys
from PIL import Image
d = sys.argv[1]
for n in ('mira', 'acerto', 'colete', 'cabeca', 'abate', 'corpo'):
    im = Image.open(os.path.join(d, 'mira', n + '.png')).convert('RGBA')
    open(os.path.join(d, n + '.rgba'), 'wb').write(struct.pack('<ii', *im.size) + im.tobytes())
" "$OUT"
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
# imagens em duas metades, lado a lado com legenda: o local do acerto (logo depois e 1,5 s depois) e a mira seguida
python3 -I -c "
import os, sys
from PIL import Image, ImageDraw, ImageFont
d, font = sys.argv[1], sys.argv[2]
def pair(a, b, out, la, lb):
    a, b = Image.open(os.path.join(d, a)), Image.open(os.path.join(d, b))
    img = Image.new('RGB', (a.width + b.width + 16, a.height + 44), (20, 22, 26))
    img.paste(a, (0, 44)); img.paste(b, (a.width + 16, 44))
    f = ImageFont.truetype(font, 17)
    dr = ImageDraw.Draw(img)
    dr.text((14, 13), la, font=f, fill=(225, 225, 225))
    dr.text((a.width + 30, 13), lb, font=f, fill=(225, 225, 225))
    img.save(os.path.join(d, out))
    for n in (a.filename, b.filename): os.remove(n)
pair('06a.png', '06b.png', '06-local-do-acerto.png', 'Logo depois: perna, peito e cabeça', '1,5 s depois')
pair('08a.png', '08b.png', '08-mira-seguida.png', 'Mira do GTA (53% x 40% da tela)',
     'Mod que centraliza a mira (50% x 50%): o sinal vai junto')
ims = [Image.open(os.path.join(d, n)) for n in ('15a.png', '15b.png', '15c.png')]
f = ImageFont.truetype(font, 17)
img = Image.new('RGB', (3 * 640 + 32, 360 + 44), (20, 22, 26))
dr = ImageDraw.Draw(img)
for i, (im, label) in enumerate(zip(ims, ['Levou na vida (vermelho)', 'Levou no colete (preto)', 'Levou na cabe\u00e7a (azul)'])):
    img.paste(im.resize((640, 360), Image.LANCZOS), (i * 656, 44))
    dr.text((i * 656 + 14, 13), label, font=f, fill=(225, 225, 225))
    os.remove(im.filename)
img.save(os.path.join(d, '15-borda.png'))
" "$OUT" "$FONT"
# os quadros das animacoes viram GIF (19: o sinal na mira, 20: os numeros)
python3 -I -c "
import glob, os, sys
from PIL import Image
d = sys.argv[1]
for prefix, name in (('gif19', '19-animacao-sinal.gif'), ('gif20', '20-animacao-numeros.gif'),
                     ('gif23', '23-local-do-tiro.gif')):
    files = sorted(glob.glob(os.path.join(d, prefix + '-*.png')))
    frames = [Image.open(f).convert('RGB').quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
              for f in files]
    frames[0].save(os.path.join(d, name), save_all=True, append_images=frames[1:], duration=50, loop=0, optimize=True)
    for f in files:
        os.remove(f)
" "$OUT"
rm -f -- "$OUT/concept" "$OUT/fundo.rgb" "$OUT"/*.rgba
rm -rf -- "$OUT/mira"
