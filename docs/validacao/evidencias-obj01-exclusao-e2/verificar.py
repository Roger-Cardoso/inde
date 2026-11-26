#!/usr/bin/env python3
"""Verify retained evidence, source bytes, pinned archives and recorded results."""
import gzip
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    entries=(HERE/'SHA256SUMS').read_text().splitlines()
    for line in entries:
        sha,name=line.split('  ',1)
        path=(HERE/name).resolve()
        assert path.is_relative_to(HERE) and path.is_file(),name
        assert digest(path)==sha,name
    source=json.loads((HERE/'codigo.json').read_text())
    for name,sha in source['files'].items():
        path=(ROOT/name).resolve()
        assert path.is_relative_to(ROOT) and digest(path)==sha,name
    archives=json.loads((HERE/'fontes-compactadas.json').read_text())['archives']
    for entry in archives:
        assert digest(ROOT/entry['path'])==entry['sha256'],entry['path']
    observed=json.loads(subprocess.check_output([sys.executable,str(ROOT/'verificar_base.py'),'--integrado'],cwd=ROOT,text=True))
    assert observed==json.loads((HERE/'base-schema37.json').read_text()),'Pinned extracted base'
    for name,count in [('ctest-nativo-65.txt',65),('compatibilidade-23.txt',23),('compatibilidade-asan-23.txt',23)]:
        text=(HERE/name).read_text()
        assert re.search(rf'100% tests passed(?:, 0 tests failed)? out of {count}\b',text),name
    log=(HERE/'python-103.txt').read_text()
    assert re.search(r'Ran 103 tests in [\d.]+s\s+OK',log), 'Python regression'
    result=json.loads((HERE/'resultados.json').read_text())
    assert result['local']['python']['passed']==103
    for bank in json.loads((HERE/'integridade.json').read_text()):
        assert bank['integrity']=='ok' and bank['foreign_keys']==[],bank['file']
    ci=json.loads((HERE/'ci-final.json').read_text())
    assert ci['code_commit']==source['commit']
    assert len(ci['jobs'])==26 and all(j['conclusion']=='success' for j in ci['jobs'])
    for name in ['primeira-suite-103.txt.gz','segunda-suite-103.txt.gz']:
        assert 'FAILED (failures=2)' in gzip.decompress((HERE/name).read_bytes()).decode(),name
    print(f'OK: {len(entries)} evidence files; {len(source["files"])} source files; '
          f'{len(archives)} archives; 103 Python, 65 native, 23 compatibility, '
          '23 ASan/UBSan tests; 26 CI jobs; prior failures retained.')
if __name__=='__main__': main()
