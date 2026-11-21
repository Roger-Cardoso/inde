#!/usr/bin/env bash
set -euo pipefail
pilot_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$pilot_root"
python3 verificar_base.py --integrado --extrair
cmake -S tests/object_composite_pilot -B build-obj01-p2a -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-obj01-p2a --target inde_object_composite_pilot --parallel 2
mkdir -p "$pilot_root/dados"
pilot_seed=()
pilot_autoseed=1
for pilot_arg in "$@"; do
  if [[ "$pilot_arg" == --workspace || "$pilot_arg" == --seed-p2c ]]; then pilot_autoseed=0; fi
done
if [[ "$pilot_autoseed" == 1 && ! -e "$pilot_root/dados/obj01-piloto-p2d/obj01-piloto.sqlite3" && -f "$pilot_root/dados/obj01-piloto-p2c/obj01-piloto.sqlite3" ]]; then
  pilot_seed=(--seed-p2c "$pilot_root/dados/obj01-piloto-p2c")
fi
exec python3 tools/obj01-pilot/serve.py --profile p2d --workspace "$pilot_root/dados/obj01-piloto-p2d" "${pilot_seed[@]}" "$@"
