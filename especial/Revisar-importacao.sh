#!/usr/bin/env sh
set -eu
PASTA_REVISAO=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$#" -ne 2 ]; then
  echo 'Uso: bash Revisar-importacao.sh CAPITULO_APLICADO.html AUTORIA_SALVA.narrproj' >&2
  exit 2
fi
if [ ! -x "$PASTA_REVISAO/build-nativo/inde-especial" ]; then
  sh "$PASTA_REVISAO/Preparar-INDE-especial.sh"
fi
mkdir -p "$PASTA_REVISAO/importacoes"
REVISAO_IMPORTACAO=$(mktemp -d "$PASTA_REVISAO/importacoes/conferencia-XXXXXXXX")
rmdir "$REVISAO_IMPORTACAO"
python3 "$PASTA_REVISAO/importacao.py" --html "$1" --autoria "$2" --saida "$REVISAO_IMPORTACAO"
exec python3 "$PASTA_REVISAO/ambiente.py" --perfil "$PASTA_REVISAO/perfil-importacao" --recursos "$PASTA_REVISAO" "$PASTA_REVISAO/build-nativo/inde-especial" --importacao "$REVISAO_IMPORTACAO/conferencia.json"
