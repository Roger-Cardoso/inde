"""Exercise the real CMake profile with INDE's Unicode text-search source."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

BASE = Path(__file__).resolve().parents[1]


def verify(source):
    with tempfile.TemporaryDirectory(prefix='inde-pgo-verification-') as folder:
        root = Path(folder)
        (root / 'main.cpp').write_text('''#include "inde/application/text_search.hpp"
#include <iostream>
int main() {
  const auto r = inde::application::find_text("Straße STRASSE água ÁGUA 日本語 العربية", {"strasse", false, true});
  if (r.matches.size() != 2 || r.matches[0].end != 6) return 1;
  std::cout << r.matches.size() << '\\n';
}
''')
        module = BASE / 'cpp/cmake/Optimization.cmake'
        (root / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.22)
project(inde_pgo_verification LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include("{module}")
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
add_executable(probe main.cpp "{source / 'src/application/text_search.cpp'}")
target_include_directories(probe PRIVATE "{source / 'include'}")
target_link_libraries(probe PRIVATE PkgConfig::GLIB)
''')
        build = root / 'build'
        logs = []

        def run(args, success=True):
            result = subprocess.run([str(x) for x in args], text=True, capture_output=True)
            logs.append(result.stdout + result.stderr)
            if success and result.returncode:
                raise RuntimeError('\n'.join(logs))
            return result

        def config(mode, success=True):
            return run(['cmake', '-S', root, '-B', build, '-G', 'Ninja',
                        '-DCMAKE_BUILD_TYPE=Release', '-DINDE_PGO_MODE=' + mode], success)

        denied = config('USE', False)
        assert denied.returncode and 'No PGO data' in denied.stderr
        config('GENERATE')
        run(['cmake', '--build', build, '--parallel', '2'])
        before = run([build / 'probe']).stdout
        profiles = list((build / 'pgo-data').rglob('*.gcda'))
        assert len(profiles) >= 2
        config('USE')
        run(['cmake', '--build', build, '--parallel', '2'])
        assert run([build / 'probe']).stdout == before
        commands = json.loads((build / 'compile_commands.json').read_text())
        for command in commands:
            assert all(flag in command['command'] for flag in
                       ['-Wconversion', '-Werror', '-O3', '-std=c++20', '-fprofile-use=', '-flto'])
        return {'missing_profile_rejected': True, 'profile_files': len(profiles),
                'instrumented_run': 'ok', 'profile_use_build': 'ok', 'unicode_result_unchanged': True,
                'scope': 'CMake workflow and real text_search.cpp, not full application training'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Materialized INDE C++ source tree')
    args = parser.parse_args()
    print(json.dumps(verify(args.source.resolve()), indent=2))
