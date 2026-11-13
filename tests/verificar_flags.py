"""Check actual compile commands, object coverage and the transformed base."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shlex
import re

BASE = Path(__file__).resolve().parents[1]
REQUIRED = {'-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Werror', '-O3'}


def verify(build, output):
    commands = json.loads((build / 'compile_commands.json').read_text())
    absent = []
    for row in commands:
        flags = shlex.split(row['command'])
        missing = REQUIRED - set(flags)
        if missing or not any(flag.startswith('-flto') for flag in flags):
            raise RuntimeError(f'Incomplete strict/LTO profile: {row["file"]}: {missing}')
        if any(flag == '-w' or flag.startswith('-Wno-') for flag in flags):
            raise RuntimeError('Suppressed diagnostic: ' + row['file'])
        obj = Path(flags[flags.index('-o') + 1])
        if not obj.is_absolute():
            obj = Path(row['directory']) / obj
        if not obj.is_file():
            absent.append(str(obj.relative_to(build)))
    active, frozen = build / 'inde-source', BASE / 'fontes/inde-integrado-schema37'
    ninja = (build / 'build.ninja').read_text()
    for target in ('inde-especial', 'inde_web_bridge'):
        block = re.search(r'^build ' + re.escape(target) + r':[^\n]*\n((?:[^\n]+\n)*)', ninja, re.M)
        if not block:
            raise RuntimeError('Missing application link rule: ' + target)
        link = next(line.split('=', 1)[1].split() for line in block[1].splitlines()
                    if line.strip().startswith('LINK_FLAGS ='))
        if '-flto=2' not in link or '-fno-lto' in link:
            raise RuntimeError('Application LTO is disabled: ' + target)
    differences, records = [], {}
    for source in sorted(frozen.rglob('*')):
        if not source.is_file():
            continue
        relative = source.relative_to(frozen)
        after = active / relative
        if source.read_bytes() != after.read_bytes():
            records[relative.as_posix()] = {
                'before_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                'after_sha256': hashlib.sha256(after.read_bytes()).hexdigest(),
            }
            differences.extend(difflib.unified_diff(source.read_text().splitlines(True),
                after.read_text().splitlines(True), fromfile='base/' + relative.as_posix(),
                tofile='optimized/' + relative.as_posix()))
    output.mkdir(parents=True, exist_ok=True)
    (output / 'base-alteracoes.diff').write_text(''.join(differences))
    report = {'translation_units_configured': len(commands), 'strict_flags': sorted(REQUIRED),
              'lto_on_all_configured_units': True, 'suppressed_warnings': False,
              'application_lto_links': ['inde-especial', 'inde_web_bridge'],
              'objects_not_built': absent, 'base_changes': records}
    (output / 'flags-verificadas.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    if absent:
        raise SystemExit('Some configured targets were not compiled; inspect objects_not_built.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    verify(args.build.resolve(), args.output.resolve())
