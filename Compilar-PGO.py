"""Compile instrumented/final GCC builds in the same dedicated directory.

Training is an explicit use session between gerar and usar. Changing source,
compiler or build directory invalidates the collected data instead of silently
using an unrelated profile. Never overwrites the regular INDE executable.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

BASE = Path(__file__).resolve().parent


def fingerprint():
    files = sorted(p for p in (BASE / 'cpp').rglob('*')
                   if p.is_file() and p.suffix in {'.cpp', '.hpp', '.py', '.json', '.cmake', '.txt'}
                   and 'overlay' not in p.parts and '__pycache__' not in p.parts)
    return {
        'compiler': subprocess.check_output(['g++', '--version'], text=True),
        'source': {p.relative_to(BASE).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
        'base': hashlib.sha256((BASE / 'fontes/inde-schema28-89b47f27.tar.gz').read_bytes()).hexdigest(),
        'integrated_source': hashlib.sha256((BASE / 'fontes/inde-integrado-schema37.tar.gz').read_bytes()).hexdigest(),
        'configuration': 'Release;C++20;strict;O3;LTO;code-optimizations',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fase', choices=['gerar', 'usar'])
    parser.add_argument('--pasta', type=Path, default=BASE / 'build-pgo')
    args = parser.parse_args()
    build = args.pasta.resolve()
    record = build / 'pgo-origem.json'
    profiles = build / 'pgo-data'
    current = fingerprint()
    current['build_directory'] = str(build)
    if args.fase == 'gerar':
        if build.exists():
            raise SystemExit('Escolha uma pasta nova para não misturar perfis de sessões anteriores.')
        build.mkdir(parents=True)
        record.write_text(json.dumps(current, ensure_ascii=False, indent=2) + '\n')
    elif not record.is_file() or json.loads(record.read_text()) != current:
        raise SystemExit('Fontes, compilador ou pasta divergentes do treinamento. Gere um novo perfil.')
    if args.fase == 'usar' and not list(profiles.rglob('*.gcda')):
        raise SystemExit('Sem perfil: execute o INDE instrumentado e feche-o normalmente antes de usar.')
    subprocess.run(['python3', str(BASE / 'verificar_base.py')], check=True)
    subprocess.run(['python3', str(BASE / 'verificar_base.py'), '--integrado'], check=True)
    subprocess.run(['cmake', '-S', str(BASE / 'cpp'), '-B', str(build), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_COMPILER=g++',
                    '-DINDE_ENABLE_LTO=ON', '-DINDE_STRICT_WARNINGS=ON',
                    '-DINDE_CODE_OPTIMIZATIONS=ON', '-DINDE_BUILD_CORE_TESTS=OFF',
                    '-DINDE_PGO_MODE=' + ('GENERATE' if args.fase == 'gerar' else 'USE'),
                    '-DINDE_PGO_DIRECTORY=' + str(profiles)], check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'inde', '--parallel', '2'], check=True)
    print(str(build / 'inde-especial'))
    if args.fase == 'gerar':
        print('Abra este executável, exercite escrita, busca, planejamento, revisão, comic e publicação; '
              'feche normalmente. Depois execute a fase usar com esta mesma pasta.')


if __name__ == '__main__':
    main()
