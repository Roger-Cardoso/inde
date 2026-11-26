#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 0 ]]; then
  printf '%s\n' 'Esta demonstração cria somente uma obra fictícia nova, sem argumentos.' >&2
  exit 2
fi
p5_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$p5_root"
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release -DINDE_BUILD_CORE_TESTS=ON
cmake --build build-nativo --target inde inde_project_file_cli inde_project_file_native_tests --parallel 2
mkdir -p "$p5_root/dados"
p5_demo="$p5_root/dados/p5-demo-$(date +%Y%m%d-%H%M%S)-$RANDOM"
"$p5_root/build-nativo/inde_project_file_native_tests" "$p5_demo"
p5_file="$p5_demo/Projeto-portatil.inde"
p5_work="$p5_demo/Edicao-da-demo.inde"
p5_cli="$p5_root/build-nativo/inde_project_file"
"$p5_cli" reabrir "$p5_file" "$p5_work" > "$p5_demo/reabertura.json"
"$p5_cli" inspecionar "$p5_file" > "$p5_demo/estado-antes-da-edicao.json"
read -r p5_sequence p5_hash < <(python3 - "$p5_demo/estado-antes-da-edicao.json" <<'PY'
import json,sys
with open(sys.argv[1]) as f: state=json.load(f)
print(state['sequence'],state['hash'])
PY
)
export XDG_DATA_HOME="$p5_demo/perfil/dados"
export XDG_CACHE_HOME="$p5_demo/perfil/cache"
export GSK_RENDERER="${GSK_RENDERER:-cairo}"
printf '%s\n' \
  'DEMONSTRAÇÃO P5-A — projeto fictício; nenhum projeto autoral é utilizado.' \
  "Arquivo único: $p5_file" \
  'O editor abre uma pasta materializada. Salvar grava essa pasta.' \
  'Depois de encerrar normalmente o editor, este demonstrador registra a pasta no arquivo único.' \
  'O histórico já contém estado inicial, alteração e restauração com a alteração posterior preservada.'
python3 ambiente.py --perfil "$p5_demo/perfil" --recursos "$p5_root" \
  "$p5_root/build-nativo/inde-especial" "$p5_work"
"$p5_cli" registrar "$p5_file" "$p5_work" "$p5_sequence" "$p5_hash" \
  'Edição da demonstração no aplicativo nativo' > "$p5_demo/estado-apos-edicao.json"
"$p5_cli" verificar "$p5_file" > "$p5_demo/integridade-final.json"
"$p5_cli" historico "$p5_file" > "$p5_demo/historico-final.json"
printf '%s\n' "Gravação e integridade conferidas. Todos os arquivos foram preservados em $p5_demo"
