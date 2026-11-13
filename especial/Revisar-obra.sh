#!/usr/bin/env sh
set -eu
PASTA_OBRA=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$#" -ne 1 ]; then
  echo 'Uso: bash Revisar-obra.sh /caminho/obra-literature.json' >&2
  exit 2
fi
if [ ! -x "$PASTA_OBRA/build-nativo/inde-especial" ]; then
  sh "$PASTA_OBRA/Preparar-INDE-especial.sh"
fi
mkdir -p "$PASTA_OBRA/importacoes"
CONFERENCIA_OBRA=$(mktemp -d "$PASTA_OBRA/importacoes/obra-XXXXXXXX")
rmdir "$CONFERENCIA_OBRA"
python3 "$PASTA_OBRA/importar_obra.py" --manifesto "$1" --saida "$CONFERENCIA_OBRA"
exec python3 "$PASTA_OBRA/ambiente.py" --perfil "$PASTA_OBRA/perfil-obra-importada" --recursos "$PASTA_OBRA" "$PASTA_OBRA/build-nativo/inde-especial" --importacao "$CONFERENCIA_OBRA/conferencia-obra.json"
