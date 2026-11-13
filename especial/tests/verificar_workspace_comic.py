"""Percursos GTK isolados: navegação Comic e regressão da escrita/publicação."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
BASE = Path(__file__).resolve().parents[1]

def run(args):
    return subprocess.run([str(a) for a in args],check=True,text=True,capture_output=True)

def verify(destination):
    destination=Path(destination).resolve()
    destination.mkdir(parents=True,exist_ok=False)
    binary=BASE/'build-nativo'
    for name in ('inde-especial','inde_web_bridge','inde_comic_demo'):
        if not (binary/name).is_file():raise RuntimeError('Compile o INDE antes de executar a verificação GTK.')
    results={}
    for label,flag in [('comic','--comic01-smoke'),('novel','--pub02-smoke')]:
        fixture=destination/label
        run([binary/'inde_web_bridge','--demo',fixture])
        if label=='comic':run([binary/'inde_comic_demo',fixture/'Publicacao.inde'])
        picture=destination/('workspace-'+label+'.png')
        result=run([sys.executable,BASE/'ambiente.py','--perfil',fixture/'perfil-gtk',
                    '--recursos',BASE,binary/'inde-especial',flag,fixture/'Publicacao.inde',picture])
        (destination/(label+'.log')).write_text(result.stdout+result.stderr)
        results[label]={'resultado':'aprovado','captura':picture.name,'saida':result.stdout.strip()}
    (destination/'resultado.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(results,ensure_ascii=False,indent=2))
    return results

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('resultado',type=Path,help='Pasta nova para dados sintéticos e capturas.')
    verify(parser.parse_args().resultado)
