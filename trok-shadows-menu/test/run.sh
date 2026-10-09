#!/usr/bin/env bash
# Testa o Trok Shadows Menu.asi no Wine 32 bits, sem o jogo, com o Shadows Extender 2.0 de verdade: o launcher.exe
# ocupa a faixa do gta_sa.exe e o host.dll monta nela a memoria do 1.0 US, carrega o shadows.asi original e o
# complemento, dispara os eventos e chama cada gancho (ver o comentario no topo de test/host.cpp).
# Requer: wine (com wine32), xvfb-run (o device D3D9 do Wine), MinGW i686, e o Shadows Extender 2.0 (DK22Pac):
# shadows.asi, shadows_pixel.fx e shadows_pixel_stencil.fx em test/original/ (ou na pasta de SHADOWS_EXTENDER).
# Esses arquivos sao do autor dele e nao vao para o git.
# Uso: test/run.sh [pasta de saida]
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(realpath -m "${1:-test/out}")
ORIGINAL=$(realpath -m "${SHADOWS_EXTENDER:-test/original}")
for f in shadows.asi shadows_pixel.fx shadows_pixel_stencil.fx; do
    if [ ! -f "$ORIGINAL/$f" ]; then
        echo "falta $ORIGINAL/$f (Shadows Extender 2.0 do DK22Pac)" >&2
        exit 2
    fi
done
mkdir -p "$OUT"

./build.sh >/dev/null
i686-w64-mingw32-gcc -O1 -Wall -o "$OUT/launcher.exe" test/launcher.c -Wl,--image-base=0x400000 \
    -Wl,--disable-dynamicbase
i686-w64-mingw32-g++ -std=c++17 -O1 -Wall -shared -static -o "$OUT/host.dll" test/host.cpp \
    -ld3d9 -Wl,--image-base=0x30000000

export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-trok32} WINEARCH=win32 WINEDEBUG=-all
if [ ! -d "$WINEPREFIX" ]; then
    xvfb-run -a wineboot -i >/dev/null 2>&1
fi

# Tudo desligado no shadows.ini, menos o stencil no grafico baixo (so na secao dele).
TOGGLES_INI='[STENCIL_SHADOWS]
MaxShadows=64
MaxDistance=50.0
FlagIgnoreSomeShadows=0
DisableBuildingShadows=0
DisplayShadowsAtLowSettings=1
[STENCIL_SHADOWS_COLOR]
R=0
G=0
B=0
A=50
[REALTIME_SHADOWS_COLOR]
R=0
G=0
B=0
A=50
[REALTIME_SHADOWS]
DisplayShadowsAtLowSettings=0
CombineRealTimeShadowsWithStencil=1
MaxDistance=15.0
CreateBlur1=1
BlurLevel=4
CreateBlur2=1
RasterSize=7
BlurRasterSize=6
RasterSize2=6
BlurRasterSize2=6
GradientMax=128
GradientMin=64
ShadowBoundSphere=2.0
ShadowBoundSphereInAir=2.0
ShadowZDistanceLimit=4.0
ShadowZDistanceLimitInAir=4.0
ShadowSunZLimit=0.6
DrawVehicleDefaultShadowWithRealTime=0
DisableVehicleDefaultShadow=0
EnableShadowsShader=1
ShadowIntensityNightFactor=0.2
ShadowIntensityCloudsFactor=0.4
MoreThanOnePlayer=0'

status=0
run() {
    local mode=$1
    local dir="$OUT/$mode"
    rm -rf "$dir"
    mkdir -p "$dir"
    cp "$OUT/launcher.exe" "$OUT/host.dll" "build/Trok Shadows Menu.asi" "$dir/"
    case $mode in
        completo | renomeado) cp test/shadows.ini "$dir/" ;; # o shadows.ini do Victor_Trok
        liga_desliga) printf '%s\n' "$TOGGLES_INI" | sed 's/$/\r/' > "$dir/shadows.ini" ;;
    esac
    if [ "$mode" != sem_original ]; then
        cp "$ORIGINAL/shadows.asi" "$ORIGINAL/shadows_pixel.fx" "$ORIGINAL/shadows_pixel_stencil.fx" "$dir/"
    fi
    echo "=== $mode"
    (cd "$dir" && timeout 120 xvfb-run -a -s "-screen 0 640x480x24" wine launcher.exe "$mode") 2>/dev/null |
        tr -d '\r' || status=1
}
run completo
run liga_desliga
run renomeado
run sem_original
exit $status
