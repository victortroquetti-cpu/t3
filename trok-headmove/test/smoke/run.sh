#!/usr/bin/env bash
# Smoke test do Trok Headmove.asi no Wine 32 bits (ver o comentario no topo de fake_gta.cpp). Nao precisa de janela.
# Requer: wine (com wine32), o MinGW i686 e python3.
# Uso: test/smoke/run.sh [asi] [pasta de saida]
set -euo pipefail
cd "$(dirname "$0")/../.."
ASI=$(realpath "${1:-dist/Trok Headmove.asi}")
OUT=$(realpath -m "${2:-test/out/smoke}")
mkdir -p "$OUT"

i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -static -s -o "$OUT/fake_gta.exe" test/smoke/fake_gta.cpp

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    wineboot -i >/dev/null 2>&1
fi

status=0
run() {
    local name=$1
    local dir="$OUT/$name"
    echo "== $name"
    mkdir -p "$dir"
    find "$dir" -maxdepth 1 -type f -delete
    cp "$OUT/fake_gta.exe" "$dir/"
    cp "$ASI" "$dir/Trok Headmove.asi"
    python3 -I test/smoke/make_fake_samp.py "$dir/samp.dll" $([ "$name" = outra-versao ] && echo r3)
    case $name in
        atraso-300) printf '[headmove]\r\natraso_ms=300\r\n' >"$dir/Trok Headmove.ini" ;;
        outros) printf '[headmove]\r\natraso_outros_ms=100\r\n' >"$dir/Trok Headmove.ini" ;;
        outros-desligado) printf '[headmove]\r\noutros_jogadores=0\r\n' >"$dir/Trok Headmove.ini" ;;
    esac
    local asi="Trok Headmove.asi"
    if [ "$name" = sa-mp ]; then asi=-; fi
    (cd "$dir" && timeout 60 wine fake_gta.exe "$asi" "$name") 2>/dev/null | tr -d '\r' || status=1
    sed 's/^/   log| /' "$dir/Trok Headmove.log" 2>/dev/null | tr -d '\r' || echo "   (sem log do .asi)"
}
for scenario in ${SCENARIOS:-sa-mp zero atraso-300 outros outros-desligado recarregar headmove-off outra-versao gta-errado}; do
    run "$scenario"
done
exit $status
