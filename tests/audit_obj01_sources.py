#!/usr/bin/env python3
"""Verify all three pinned source archives and optionally extract into a NEW directory.
No project is opened. Only regular files/directories, unique relative paths, no links.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def audit(destination=None):
    base = json.loads((ROOT / 'metadata/BASE-INDE.json').read_text())
    integrated = json.loads((ROOT / 'metadata/INTEGRACAO-INDE.json').read_text())
    specs = [
        ('schema28', base['arquivo_fonte_local'], base['arquivo_fonte_sha256']),
        ('original37', integrated['arquivo_original'], integrated['sha256_original']),
        ('integrated37', integrated['arquivo_integrado'], integrated['sha256_integrado']),
    ]
    inventories, result = {}, {'archives': [], 'comparisons': {}}
    # Verify every archive before extracting any of them.
    for label, path, expected in specs:
        raw = (ROOT / path).read_bytes()
        actual = digest(raw)
        if actual != expected:
            raise ValueError('Archive hash mismatch: ' + path)
        files, names = {}, set()
        with tarfile.open(ROOT / path) as source:
            for member in source:
                name = PurePosixPath(member.name)
                if name.is_absolute() or '..' in name.parts or not name.parts:
                    raise ValueError('Unsafe path: ' + member.name)
                normalized = name.as_posix()
                if normalized in names or not (member.isfile() or member.isdir()):
                    raise ValueError('Duplicate path or unsupported member: ' + member.name)
                names.add(normalized)
                if member.isfile():
                    files[normalized] = digest(source.extractfile(member).read())
        if label == 'integrated37' and files != integrated['arquivos_integrados']:
            raise ValueError('Integrated archive inventory mismatch')
        inventories[label] = files
        result['archives'].append({'label': label, 'path': path, 'sha256': actual,
                                   'files': len(files), 'manifest_match': True})
    for old, new in [('schema28', 'original37'), ('original37', 'integrated37')]:
        a, b = inventories[old], inventories[new]
        result['comparisons'][old + '..' + new] = {
            'added': sorted(b.keys() - a.keys()), 'removed': sorted(a.keys() - b.keys()),
            'changed': sorted(k for k in a.keys() & b.keys() if a[k] != b[k]),
            'identical': sum(a[k] == b[k] for k in a.keys() & b.keys())}
    if destination is not None:
        destination.mkdir(parents=True, exist_ok=False)
        for label, path, _ in specs:
            target = destination / label
            target.mkdir()
            with tarfile.open(ROOT / path) as source:
                source.extractall(target, filter='data')
            extracted = {p.relative_to(target).as_posix(): digest(p.read_bytes())
                         for p in target.rglob('*') if p.is_file()}
            if extracted != inventories[label]:
                raise ValueError('Extracted bytes mismatch: ' + label)
        result['extracted_bytes_match'] = True
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extract-to', type=Path)
    args = parser.parse_args()
    print(json.dumps(audit(args.extract_to), ensure_ascii=False, indent=2))
