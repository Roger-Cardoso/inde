#!/usr/bin/env sh
set -eu
PASTA_PONTE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -f "$PASTA_PONTE/fontes/inde-schema28/CMakeLists.txt" ]; then
  python3 "$PASTA_PONTE/verificar_base.py" --extrair
else
  python3 "$PASTA_PONTE/verificar_base.py"
fi
if [ ! -f "$PASTA_PONTE/fontes/inde-integrado-schema37/CMakeLists.txt" ]; then
  python3 "$PASTA_PONTE/verificar_base.py" --integrado --extrair
else
  python3 "$PASTA_PONTE/verificar_base.py" --integrado
fi
cmake -S "$PASTA_PONTE/cpp" -B "$PASTA_PONTE/build-nativo" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$PASTA_PONTE/build-nativo" --target inde_web_bridge --parallel 2
