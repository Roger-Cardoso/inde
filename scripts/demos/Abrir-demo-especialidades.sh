#!/usr/bin/env bash
set -euo pipefail
native_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$native_root"
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-nativo --target inde --parallel 2
native_demo="$native_root/dados/obj01-especialidades-r1"
mkdir -p "$native_demo/perfil"
export XDG_DATA_HOME="$native_demo/perfil/dados"
export XDG_CACHE_HOME="$native_demo/perfil/cache"
export GSK_RENDERER="${GSK_RENDERER:-cairo}"
if [[ ! -e "$native_demo/Especialidades.inde" ]]; then
  python3 ambiente.py --perfil "$native_demo/perfil" --recursos "$native_root" \
    "$native_root/build-nativo/inde-especial" --objetos-smoke \
    "$native_demo/Especialidades.inde" "$native_demo/folha.png"
fi
printf '%s\n' 'No Documento Matemática na narrativa, use Ferramentas ou F4, selecione um trecho e clique direito. Em Estrutura, arraste os círculos ou use Ctrl+seta acima/abaixo.'
exec python3 ambiente.py --perfil "$native_demo/perfil" --recursos "$native_root" \
  "$native_root/build-nativo/inde-especial" "$native_demo/Especialidades.inde"
