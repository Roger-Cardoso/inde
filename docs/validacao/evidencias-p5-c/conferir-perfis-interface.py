"""Read synthetic GTK artifacts; never change projects or their databases."""
import json
from pathlib import Path
import sqlite3
import sys

base=Path(sys.argv[1]).absolute()
rows=[]
for label in ['legacy','blocks']:
    folder=base/f'p5c-gtk-{label}-20261126'
    for relative,expected in [('fixture/Projeto-portatil.inde','legacy-project' if label=='legacy' else 'chunked-project'),('Com-historico.inde','legacy-project' if label=='legacy' else 'chunked-project'),('Estado-atual.inde','chunked-project'),('Novo.inde','chunked-project')]:
        path=folder/relative
        with sqlite3.connect(f'file:{path}?mode=ro',uri=True) as db:
            kind,payload=db.execute("SELECT v.kind,v.payload FROM head h JOIN entries e ON e.seq=h.seq JOIN record_versions v ON v.id=e.version_id WHERE e.key='project'").fetchone()
        assert kind==expected,(path,kind,expected)
        assert json.loads(payload)['format']==('INDE-DIRECTORY-37' if kind=='legacy-project' else 'INDE-DIRECTORY-37-CHUNKS-1')
        rows.append(dict(file=str(path),kind=kind,verified=True))
print(json.dumps(dict(format='INDE-P5C-GTK-PROFILES-1',checks=rows),ensure_ascii=False,indent=2))
