#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 0 ]]; then
  printf '%s\n' 'Esta demonstração usa somente seu projeto fictício, sem argumentos.' >&2
  exit 2
fi
planning_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$planning_root"
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-nativo --target inde --parallel 2
planning_demo="$planning_root/dados/obj01-demo-planejamento-exclusao"
mkdir -p "$planning_demo"
export XDG_DATA_HOME="$planning_demo/perfil/dados"
export XDG_CACHE_HOME="$planning_demo/perfil/cache"
export GSK_RENDERER="${GSK_RENDERER:-cairo}"
if [[ ! -e "$planning_demo/Demo-planejamento.inde" ]]; then
  python3 ambiente.py --perfil "$planning_demo/perfil" --recursos "$planning_root" \
    "$planning_root/build-nativo/inde-especial" --planejamento-exclusao-smoke \
    "$planning_demo/Preparacao.inde" "$planning_demo/verificacao.png"
fi
printf '%s\n' 'Abra Planejar → Planejamento visual e escolha Obra. Há duas composições fictícias, um trecho reutilizado e uma faixa.'
printf '%s\n' 'Selecione um elemento e use Delete ou o menu de contexto. Para excluir o conjunto, use Montagem → Excluir composição visual.'
exec python3 ambiente.py --perfil "$planning_demo/perfil" --recursos "$planning_root" \
  "$planning_root/build-nativo/inde-especial" "$planning_demo/Demo-planejamento.inde"
