#!/usr/bin/env bash
# Teste do Linux (sem o jogo) do leitor dos .ifp (ifp.cpp) e da pose (pose.cpp): as marcas MOD/NOVA pelos nomes, pela
# pasta de originais e pelo .txt de referencia, os quadros-chave (ANP3 comprimido e nao, ANPK) e a conta da pele.
# Uso: test/anim/run.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=$(realpath -m test/out/anim)
mkdir -p "$OUT"
g++ -std=c++17 -O1 -Wall -Itest/anim/stub -Isrc -o "$OUT/anim_test" test/anim/anim_test.cpp src/ifp.cpp src/pose.cpp
status=0
for mode in nomes pasta txt pose; do
    "$OUT/anim_test" "$mode" "$OUT/jogo_$mode" || status=1
done
exit $status
