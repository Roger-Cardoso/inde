#!/usr/bin/env sh
set -eu
PASTA_OBRA=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ ! -x "$PASTA_OBRA/build-nativo/inde-especial" ]; then
  sh "$PASTA_OBRA/Preparar-INDE-especial.sh"
fi
if [ ! -f "$PASTA_OBRA/demo-obra-importada/revisao-obra/conferencia-obra.json" ]; then
  python3 "$PASTA_OBRA/importar_obra.py" --demo "$PASTA_OBRA/demo-obra-importada"
fi
exec python3 "$PASTA_OBRA/ambiente.py" --perfil "$PASTA_OBRA/perfil-obra-importada" --recursos "$PASTA_OBRA" "$PASTA_OBRA/build-nativo/inde-especial" --importacao "$PASTA_OBRA/demo-obra-importada/revisao-obra/conferencia-obra.json"
