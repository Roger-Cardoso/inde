#!/usr/bin/env sh
set -eu
PASTA_IMPORTACAO=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -x "$PASTA_IMPORTACAO/build-nativo/inde-especial" ]; then
  sh "$PASTA_IMPORTACAO/Preparar-INDE-especial.sh"
fi
if [ ! -f "$PASTA_IMPORTACAO/demo-importacao/revisao/conferencia.json" ]; then
  python3 "$PASTA_IMPORTACAO/importacao.py" --demo "$PASTA_IMPORTACAO/demo-importacao"
fi
exec python3 "$PASTA_IMPORTACAO/ambiente.py" --perfil "$PASTA_IMPORTACAO/perfil-importacao" --recursos "$PASTA_IMPORTACAO" "$PASTA_IMPORTACAO/build-nativo/inde-especial" --importacao "$PASTA_IMPORTACAO/demo-importacao/revisao/conferencia.json"
