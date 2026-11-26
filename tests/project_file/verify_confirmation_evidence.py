"""Check the exact sources and retained evidence used for P5-D1's report."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
manifest = ROOT / 'docs/validacao/evidencias-p5-d1/manifesto.json'
spec = json.loads(manifest.read_text())
count = 0
for group in ['sources', 'evidence']:
    for relative, expected in spec[group].items():
        path = (ROOT / relative).resolve()
        if not path.is_relative_to(ROOT):
            raise SystemExit('Caminho de evidência fora do repositório: ' + relative)
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit('Hash divergente: ' + relative)
        count += 1
print(json.dumps(dict(format=spec['format'], hashes_checked=count, valid=True)))
