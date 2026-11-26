#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 0 ]]; then
  printf '%s\n' 'Esta demonstração cria somente um projeto fictício novo, sem argumentos.' >&2
  exit 2
fi
p5b_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$p5b_root"
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release -DINDE_BUILD_CORE_TESTS=ON
cmake --build build-nativo --target inde inde_project_file_native_tests --parallel 2
mkdir -p "$p5b_root/dados"
p5b_demo="$p5b_root/dados/p5b-demo-$(date +%Y%m%d-%H%M%S)-$RANDOM"
P5_TRANSPORT_BLOCKS=1 build-nativo/inde_project_file_native_tests "$p5b_demo"
export XDG_STATE_HOME="$p5b_demo/perfil/estado"
export XDG_DATA_HOME="$p5b_demo/perfil/dados"
export XDG_CACHE_HOME="$p5b_demo/perfil/cache"
export GSK_RENDERER="${GSK_RENDERER:-cairo}"
printf '%s\n' \
  'P5-B — o próprio INDE abre e confirma o arquivo único.' \
  'Use Salvar (Ctrl+S) para confirmar o trabalho no .inde.' \
  'Projeto → Estados do arquivo único (Ctrl+Alt+H) permite retomar uma versão.' \
  'Projeto → Salvar cópia em arquivo único (Ctrl+Alt+S) oferece cópia com histórico ou do estado atual.' \
  "Arquivo fictício: $p5b_demo/Projeto-portatil.inde"
exec python3 ambiente.py --perfil "$p5b_demo/perfil" --recursos "$p5b_root" \
  "$p5b_root/build-nativo/inde-especial" "$p5b_demo/Projeto-portatil.inde"
