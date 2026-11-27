"""Verify the exact P5-D2 source and evidence set; earlier cuts are historical."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = json.loads((ROOT/'docs/validacao/evidencias-p5-d2/manifesto.json').read_text())
assert spec['format'] == 'INDE-P5-D2-EVIDENCE-1'
count = 0
for group in ['sources', 'evidence']:
    for relative, expected in spec[group].items():
        path = (ROOT/relative).resolve()
        if not path.is_relative_to(ROOT):
            raise SystemExit('Caminho fora do repositório: '+relative)
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise SystemExit('Hash divergente: '+relative)
        count += 1
ci = json.loads((ROOT/'docs/validacao/evidencias-p5-d2/ci.json').read_text())
assert ci['head_sha'] == spec['validated_commit']
assert ci['conclusion'] == 'success' and len(ci['jobs']) == 6
assert all(j['status'] == 'completed' and j['conclusion'] == 'success' for j in ci['jobs'])
print(json.dumps(dict(format=spec['format'], hashes_checked=count, ci_jobs=6, valid=True)))
