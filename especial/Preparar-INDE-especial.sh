#!/usr/bin/env sh
set -eu
PASTA_ESPECIAL=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sh "$PASTA_ESPECIAL/Preparar-ponte.sh"
cmake --build "$PASTA_ESPECIAL/build-nativo" --target inde inde_comic_tests inde_comic_demo inde_publication_tests inde_catalog_tests inde_import_tests inde_work_import_tests inde_publication_ui_smoke inde_integration_tests --parallel 2
