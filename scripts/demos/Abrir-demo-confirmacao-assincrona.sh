#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 0 ]]; then
  printf '%s\n' 'Esta demonstração cria somente um projeto fictício novo, sem argumentos.' >&2
  exit 2
fi
p5d_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$p5d_root"
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release -DINDE_BUILD_CORE_TESTS=ON
cmake --build build-nativo --target inde inde_project_file_cli inde_project_file_native_tests --parallel 2
mkdir -p "$p5d_root/dados"
p5d_demo="$p5d_root/dados/p5d-demo-$(date +%Y%m%d-%H%M%S)-$RANDOM"
P5_TRANSPORT_BLOCKS=1 build-nativo/inde_project_file_native_tests "$p5d_demo"
build-nativo/inde_project_file reabrir "$p5d_demo/Projeto-portatil.inde" "$p5d_demo/Copia-ficticia.inde" > "$p5d_demo/materializacao.json"
python3 - "$p5d_demo/Copia-ficticia.inde" <<'PY'
from pathlib import Path
import random
import sys
root=Path(sys.argv[1]);folder=root/'observacao';folder.mkdir()
generator=random.Random(3701)
with (folder/'recurso-ficticio.bin').open('xb') as out:
    for _ in range(256):out.write(generator.randbytes(65536))
PY
build-nativo/inde_project_file converter-blocos "$p5d_demo/Copia-ficticia.inde" "$p5d_demo/Confirmacao.inde" > "$p5d_demo/arquivo-inicial.json"
export XDG_STATE_HOME="$p5d_demo/perfil/estado"
export XDG_DATA_HOME="$p5d_demo/perfil/dados"
export XDG_CACHE_HOME="$p5d_demo/perfil/cache"
export GSK_RENDERER="${GSK_RENDERER:-cairo}"
printf '%s\n' \
  'P5-D1 — confirmação em segundo plano no próprio INDE.' \
  'O projeto fictício contém texto, objetos e um recurso de observação de 16 MiB.' \
  'Altere uma frase e use Ctrl+S. A janela mostra a conferência, com a edição pausada.' \
  'Cancelar salvamento preserva o trabalho. O cancelamento fica indisponível ao iniciar a confirmação final.' \
  'Sair após salvar pode ser retirado por Continuar no editor. Ctrl+Q e fechar a janela aguardam o resultado.' \
  "Arquivo fictício: $p5d_demo/Confirmacao.inde"
exec python3 ambiente.py --perfil "$p5d_demo/perfil" --recursos "$p5d_root" \
  "$p5d_root/build-nativo/inde-especial" "$p5d_demo/Confirmacao.inde"
