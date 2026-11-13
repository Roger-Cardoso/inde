"""Apply reviewed changes to a build-local copy; never modify the pinned base."""
import json
from pathlib import Path
import sys


def materialize(base, target, performance):
    patches = json.loads(Path(__file__).with_name('base_patches.json').read_text())
    changed = []
    for source in sorted(base.rglob('*')):
        if not source.is_file():
            continue
        name = source.relative_to(base).as_posix()
        data = source.read_bytes()
        for group in ('diagnostics', 'performance') if performance else ('diagnostics',):
            for replacement in patches[group].get(name, []):
                before, after = replacement[:2]
                expected = replacement[2] if len(replacement) == 3 else 1
                old, new = before.encode(), after.encode()
                if data.count(old) != expected:
                    raise RuntimeError(f'Patch requires {expected} exact matches: {group}/{name}: {before[:80]!r}')
                data = data.replace(old, new, expected)
        if data != source.read_bytes():
            changed.append(name)
        out = target / name
        out.parent.mkdir(parents=True, exist_ok=True)
        if not out.exists() or out.read_bytes() != data:
            out.write_bytes(data)
    (target.parent / 'base-transformada.json').write_text(json.dumps({
        'base': str(base), 'performance': performance, 'changed': changed,
    }, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__':
    materialize(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), sys.argv[3] == 'ON')
