#!/usr/bin/env bash
set -euo pipefail
if [[ $# -gt 1 ]]; then
  printf '%s\n' 'Uso: Abrir-demo-folha-comum.sh [caminho-do-projeto.inde]' >&2
  exit 2
fi
native_project="${1:-}"
if [[ -n "$native_project" && "$native_project" != /* ]]; then
  native_project="$PWD/$native_project"
fi
native_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$native_root"
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-nativo --target inde --parallel 2
native_demo="$native_root/dados/obj01-folha-comum-r6"
mkdir -p "$native_demo"
export XDG_DATA_HOME="$native_demo/perfil/dados"
export XDG_CACHE_HOME="$native_demo/perfil/cache"
export GSK_RENDERER="${GSK_RENDERER:-cairo}"
if [[ -z "$native_project" && ! -e "$native_demo/Projeto.inde" ]]; then
  python3 ambiente.py --perfil "$native_demo/perfil" --recursos "$native_root" "$native_root/build-nativo/inde-especial" --objetos-smoke "$native_demo/Projeto.inde" "$native_demo/demonstracao.png"
fi
native_project="${native_project:-$native_demo/Projeto.inde}"
printf '%s\n' 'Na Escrita, abra Matemática na narrativa: todas as especialidades mostram a mesma folha. Digite diretamente nos parágrafos; use clique direito para os objetos.'
exec python3 ambiente.py --perfil "$native_demo/perfil" --recursos "$native_root" "$native_root/build-nativo/inde-especial" "$native_project"
