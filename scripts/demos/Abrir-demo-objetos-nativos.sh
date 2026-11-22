#!/usr/bin/env bash
set -euo pipefail
native_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$native_root"
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-nativo --target inde --parallel 2
native_demo="$native_root/dados/obj01-gtk-nativo-final"
mkdir -p "$native_demo"
if [[ ! -e "$native_demo/Projeto.inde" ]]; then
  python3 ambiente.py --perfil "$native_demo/perfil" --recursos "$native_root" "$native_root/build-nativo/inde-especial" --objetos-smoke "$native_demo/Projeto.inde" "$native_demo/demonstracao.png"
fi
printf '%s\n' 'Na Escrita, selecione Matemática na narrativa e use Objetos no cabeçalho.'
exec python3 ambiente.py --perfil "$native_demo/perfil" --recursos "$native_root" "$native_root/build-nativo/inde-especial" "$native_demo/Projeto.inde"
