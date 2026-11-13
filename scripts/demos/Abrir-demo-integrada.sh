#!/usr/bin/env sh
set -eu
PASTA_INTEGRADA=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ ! -x "$PASTA_INTEGRADA/build-nativo/inde_integration_tests" ]; then
  sh "$PASTA_INTEGRADA/Preparar-INDE-especial.sh"
fi
if [ ! -f "$PASTA_INTEGRADA/demo-integrada/concluida" ]; then
  if [ -e "$PASTA_INTEGRADA/demo-integrada" ]; then
    echo 'A criação anterior da demo ficou incompleta. Preserve essa pasta para diagnóstico antes de criar outra.' >&2
    exit 1
  fi
  "$PASTA_INTEGRADA/build-nativo/inde_web_bridge" --demo "$PASTA_INTEGRADA/demo-integrada"
  "$PASTA_INTEGRADA/build-nativo/inde_integration_tests" "$PASTA_INTEGRADA/demo-integrada"
  touch "$PASTA_INTEGRADA/demo-integrada/concluida"
fi
exec python3 "$PASTA_INTEGRADA/ambiente.py" --perfil "$PASTA_INTEGRADA/perfil-integracao" --recursos "$PASTA_INTEGRADA" "$PASTA_INTEGRADA/build-nativo/inde-especial" "$PASTA_INTEGRADA/demo-integrada/Publicacao.inde"
