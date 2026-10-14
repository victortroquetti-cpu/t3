#!/usr/bin/env bash
# Smoke test do Trok Hitmarker.asi no Wine 32 bits com Xvfb (ver o comentario no topo de fake_gta.cpp).
# Requer: wine (com wine32), xvfb-run, Mesa 32 bits, o MinGW i686 e python3.
# Uso: test/smoke/run.sh [asi] [pasta de saida]
#   TROK_FONT / TROK_ICONS: font.ttf e lucide.ttf da casa (opcionais; sem elas o .asi usa a Arial do Wine)
set -euo pipefail
cd "$(dirname "$0")/../.."
ASI=$(realpath "${1:-dist/Trok Hitmarker.asi}")
OUT=$(realpath -m "${2:-test/out/smoke}")
mkdir -p "$OUT"

i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -static -s -o "$OUT/fake_gta.exe" test/smoke/fake_gta.cpp -ld3d9

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi

status=0
run() {
    local name=$1
    local dir="$OUT/$name"
    echo "== $name"
    mkdir -p "$dir/moonloader/resource/trok"
    find "$dir" -maxdepth 1 -type f -delete
    rm -rf "$dir/Trok Hitmarker"
    cp "$OUT/fake_gta.exe" "$dir/"
    cp "$ASI" "$dir/Trok Hitmarker.asi"
    python3 test/smoke/make_fake_samp.py "$dir/samp.dll" $([ "$name" = outra-versao ] && echo r3)
    if [ "$name" = pasta-propria ]; then
        # o que o usuario poe na pasta: um mp3 no lugar do acerto.wav, um PNG retangular no lugar do acerto.png e,
        # durante o jogo, uma mira.png nova (o fake_gta.exe copia por cima)
        mkdir -p "$dir/Trok Hitmarker/mira" "$dir/Trok Hitmarker/sons"
        ffmpeg -loglevel error -y -f lavfi -i "sine=frequency=900:duration=0.25" -ac 2 -ar 44100 -b:a 128k \
            "$dir/Trok Hitmarker/sons/acerto.mp3"
        python3 -I test/smoke/make_png.py "$dir/Trok Hitmarker/mira/acerto.png" 300 200 255 40 40
        python3 -I test/smoke/make_png.py "$dir/nova_mira.png" 64 64 255 0 255
    fi
    case $name in
        desligado) printf '[padrao]\r\nligado=0\r\n' >"$dir/Trok Hitmarker.ini" ;;
        local-do-tiro) printf '[padrao]\r\nnumeros=3\r\nnumeros_tamanho=50\r\n' >"$dir/Trok Hitmarker.ini" ;;
        pvp-20 | pvp-20-sem-mensagem) printf '[padrao]\r\nanotar_todos=1\r\n' >"$dir/Trok Hitmarker.ini" ;;
        mira-do-jogo) printf '[padrao]\r\nmira=0\r\nmira_estatica=1\r\nmira_tamanho=150\r\n' >"$dir/Trok Hitmarker.ini" ;;
    esac
    if [ -n "${TROK_FONT:-}" ]; then cp "$TROK_FONT" "$dir/moonloader/resource/trok/font.ttf"; fi
    if [ -n "${TROK_ICONS:-}" ]; then cp "$TROK_ICONS" "$dir/moonloader/resource/trok/lucide.ttf"; fi
    (cd "$dir" && timeout 120 xvfb-run -a -s "-screen 0 1600x900x24" wine fake_gta.exe "Trok Hitmarker.asi" "$name") \
        2>/dev/null | tr -d '\r' || status=1
    sed 's/^/   log| /' "$dir/Trok Hitmarker.log" 2>/dev/null | tr -d '\r' || echo "   (sem log do .asi)"
}
for scenario in ${SCENARIOS:-trok mira-do-jogo pasta-propria local-do-tiro desligado outra-versao gta-errado pvp-20 pvp-20-sem-mensagem}; do
    run "$scenario"
done
exit $status
