#!/usr/bin/env sh
set -eu
PASTA_COMIC=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# Rebuilds incrementally, including the new GTK page; no network or package install.
sh "$PASTA_COMIC/Preparar-INDE-especial.sh"
if [ ! -d "$PASTA_COMIC/demo-comic-workspace/Publicacao.inde" ]; then
  "$PASTA_COMIC/build-nativo/inde_web_bridge" --demo "$PASTA_COMIC/demo-comic-workspace"
fi
"$PASTA_COMIC/build-nativo/inde_comic_demo" "$PASTA_COMIC/demo-comic-workspace/Publicacao.inde"
exec python3 "$PASTA_COMIC/ambiente.py" --perfil "$PASTA_COMIC/perfil-comic-workspace" --recursos "$PASTA_COMIC" "$PASTA_COMIC/build-nativo/inde-especial" --comic "$PASTA_COMIC/demo-comic-workspace/Publicacao.inde"
