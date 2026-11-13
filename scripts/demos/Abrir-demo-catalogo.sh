#!/usr/bin/env sh
set -eu
PASTA_CATALOGO=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ ! -x "$PASTA_CATALOGO/build-nativo/inde-especial" ] || [ ! -x "$PASTA_CATALOGO/build-nativo/inde_web_bridge" ]; then
  sh "$PASTA_CATALOGO/Preparar-INDE-especial.sh"
fi
if [ ! -d "$PASTA_CATALOGO/demo-catalogo/Publicacao.inde" ]; then
  "$PASTA_CATALOGO/build-nativo/inde_web_bridge" --demo-catalogo "$PASTA_CATALOGO/demo-catalogo"
fi
exec python3 "$PASTA_CATALOGO/ambiente.py" --perfil "$PASTA_CATALOGO/perfil-catalogo" --recursos "$PASTA_CATALOGO" "$PASTA_CATALOGO/build-nativo/inde-especial" "$PASTA_CATALOGO/demo-catalogo/Publicacao.inde"
