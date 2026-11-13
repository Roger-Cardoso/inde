#!/usr/bin/env sh
set -eu
PASTA_ESPECIAL=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -x "$PASTA_ESPECIAL/build-nativo/inde-especial" ]; then
  sh "$PASTA_ESPECIAL/Preparar-INDE-especial.sh"
fi
if [ "${1:-}" = "--vazio" ]; then
  shift
  exec python3 "$PASTA_ESPECIAL/ambiente.py" --perfil "$PASTA_ESPECIAL/perfil-especial" --recursos "$PASTA_ESPECIAL" "$PASTA_ESPECIAL/build-nativo/inde-especial" "$@"
fi
if [ "$#" -gt 0 ]; then
  exec python3 "$PASTA_ESPECIAL/ambiente.py" --perfil "$PASTA_ESPECIAL/perfil-especial" --recursos "$PASTA_ESPECIAL" "$PASTA_ESPECIAL/build-nativo/inde-especial" "$@"
fi
if [ ! -d "$PASTA_ESPECIAL/demo-nativa/Publicacao.inde" ]; then
  "$PASTA_ESPECIAL/build-nativo/inde_web_bridge" --demo "$PASTA_ESPECIAL/demo-nativa"
fi
exec python3 "$PASTA_ESPECIAL/ambiente.py" --perfil "$PASTA_ESPECIAL/perfil-especial" --recursos "$PASTA_ESPECIAL" "$PASTA_ESPECIAL/build-nativo/inde-especial" "$PASTA_ESPECIAL/demo-nativa/Publicacao.inde"
