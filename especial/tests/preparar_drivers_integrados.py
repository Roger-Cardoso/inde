"""Link upstream GTK regressions against the actual special application's objects."""
import argparse
from pathlib import Path
import shlex
import subprocess

BASE = Path(__file__).resolve().parents[1]


def prepare(build, driver):
    commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'inde-especial'], text=True)
    link = next(line for line in reversed(commands.splitlines()) if ' -o inde-especial ' in line)
    tokens = shlex.split(link)
    objects = [token for token in tokens if token.endswith('.o') and not token.endswith('/src/main.cpp.o')]
    libraries = tokens[tokens.index('-o') + 2:]
    if '&&' in libraries:
        libraries = libraries[:libraries.index('&&')]
    assert objects and (build / 'inde-especial').is_file(), 'Compile o aplicativo primeiro.'
    source = build / 'inde-source'
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', 'gtkmm-4.0'], text=True))
    includes = [BASE / 'cpp/native/include', BASE / 'cpp/overlay/include', BASE / 'cpp',
                source / 'include', source / 'src']
    output = build / ('inde_integrated_' + driver + '_smoke')
    subprocess.run(['c++', '-std=c++20', '-O3', '-Wall', '-Wextra', '-Wpedantic', '-fno-lto',
                    *['-I' + str(path) for path in includes], *flags,
                    str(source / ('tests/ui_' + driver + '_smoke.cpp')),
                    *objects, *libraries, '-o', str(output)], cwd=build, check=True)
    print(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('driver', choices=['editor_specializations', 'contextual_visual', 'contextual_delivery'])
    parser.add_argument('--build', type=Path, default=BASE / 'build-nativo')
    args = parser.parse_args()
    prepare(args.build.resolve(), args.driver)
