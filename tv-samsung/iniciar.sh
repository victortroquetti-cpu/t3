#!/bin/sh
# Liga o servidor da Minha TV no macOS ou Linux. Deixe o terminal aberto enquanto usa a TV.
# No Linux a porta 80 pede administrador: rode com sudo ./iniciar.sh
cd "$(dirname "$0")" || exit 1
exec python3 servidor/servidor.py "$@"
