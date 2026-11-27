#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 0 ]]; then
  printf '%s\n' 'Esta demonstração cria somente um projeto fictício novo, sem argumentos.' >&2
  exit 2
fi
p5d3_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$p5d3_root"
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release -DINDE_BUILD_CORE_TESTS=ON
cmake --build build-nativo --target inde inde_project_file_cli inde_project_file_native_tests --parallel 2
mkdir -p "$p5d3_root/dados"
p5d3_demo="$p5d3_root/dados/p5d3-demo-$(date +%Y%m%d-%H%M%S)-$RANDOM"
P5_TRANSPORT_BLOCKS=1 build-nativo/inde_project_file_native_tests "$p5d3_demo"
build-nativo/inde_project_file reabrir "$p5d3_demo/Projeto-portatil.inde" "$p5d3_demo/Copia-ficticia.inde" > "$p5d3_demo/materializacao.json"
python3 - "$p5d3_demo/Copia-ficticia.inde" <<'PY'
from pathlib import Path
import random
import sys
root=Path(sys.argv[1]);folder=root/'observacao';folder.mkdir()
generator=random.Random(3701)
with (folder/'recurso-ficticio.bin').open('xb') as out:
    for _ in range(256):out.write(generator.randbytes(65536))
PY
build-nativo/inde_project_file converter-blocos "$p5d3_demo/Copia-ficticia.inde" "$p5d3_demo/Confirmacao.inde" > "$p5d3_demo/arquivo-inicial.json"
export XDG_STATE_HOME="$p5d3_demo/perfil/estado"
export XDG_DATA_HOME="$p5d3_demo/perfil/dados"
export XDG_CACHE_HOME="$p5d3_demo/perfil/cache"
export GSK_RENDERER="${GSK_RENDERER:-cairo}"
printf '%s\n' \
  'P5-D3 — preparação do texto e dos objetos no INDE nativo.' \
  'O projeto fictício contém texto, objetos e um recurso de observação de 16 MiB.' \
  'Abra Uma obra transportável, altere uma frase e use Ctrl+S.' \
  'A janela apresenta a preparação do texto, dos objetos e do arquivo, com a edição pausada.' \
  'Cancelar salvamento preserva o trabalho. O cancelamento fica indisponível ao iniciar a confirmação final.' \
  'Ctrl+Alt+S: copie com histórico ou continue em uma cópia do estado atual.' \
  'Ctrl+Alt+H: consulte os estados e retome um deles sem apagar o trabalho posterior.' \
  'Ctrl+Alt+N e Ctrl+Alt+O: crie e abra arquivos. Ctrl+Q aguarda a operação em andamento.' \
  "Arquivo fictício: $p5d3_demo/Confirmacao.inde"
exec python3 ambiente.py --perfil "$p5d3_demo/perfil" --recursos "$p5d3_root" \
  "$p5d3_root/build-nativo/inde-especial" "$p5d3_demo/Confirmacao.inde"
