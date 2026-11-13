#!/usr/bin/env sh
set -eu
PASTA_DEMO=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
exec python3 "$PASTA_DEMO/publicacao.py" "$@"
