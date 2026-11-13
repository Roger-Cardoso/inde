"""Empacota somente os componentes do laboratório, sem dados nem credenciais."""
import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

BASE = Path(__file__).resolve().parent
FILES = ['evidencias/otimizacao01/ambiente-benchmark.json', 'evidencias/otimizacao01/comparacao-estatisticas-descartada/resultado.json', 'evidencias/otimizacao01/gtk/workspace-comic.png', 'evidencias/otimizacao01/gtk/workspace-novel.png', 'evidencias/otimizacao01/flags-verificadas.json', 'evidencias/otimizacao01/base-alteracoes.diff', 'evidencias/otimizacao01/pgo-verificado.json', 'evidencias/otimizacao01/comparacao-final/resultado.json', 'evidencias/otimizacao01/gtk/resultado.json', 'Compilar-PGO.py', 'OTIMIZACAO-CXX.md', 'VALIDACAO-OPT-CXX-01.md', 'CONTRATO-COMIC-02.md', 'VALIDACAO-COMIC-02.md', 'evidencias/comic02/workspace-comic.png', 'evidencias/comic02/escrita-retorno-comic02.png', 'evidencias/comic02/workspace-novel.png', 'Abrir-demo-comic.sh', 'ROTEIRO-COMIC.md', 'CONTRATO-COMIC-01.md', 'VALIDACAO-COMIC-01.md', 'importar_obra.py', 'identidade_publica.py', 'Abrir-demo-obra-importada.sh', 'Revisar-obra.sh', 'ROTEIRO-OBRA-IMPORTADA.md', 'ambiente.py', 'importacao.py', 'Abrir-demo-importacao.sh', 'Revisar-importacao.sh', 'ROTEIRO-IMPORTACAO.md', 'DIAGNOSTICO-ABERTURA.md', 'publicacao.py', 'crud.py', 'ponte_inde.py', 'verificar_base.py', 'empacotar.py', 'Abrir-demo.sh', 'Preparar-ponte.sh', 'Preparar-INDE-especial.sh', 'Abrir-INDE-especial.sh', 'Abrir-demo-catalogo.sh', 'ROTEIRO-CATALOGO.md', 'ROTEIRO-GTK.md', 'README.md', 'CONTRATO-INTEGRACAO.md',
         'ESTADO.json', 'PROVENIENCIA.json', 'BASE-INDE.json', 'VALIDACAO.md', 'MANIFESTO-FONTES.json',
         'fontes/inde-schema28-89b47f27.tar.gz']
FOLDERS = ['motor', 'recursos', 'interface', 'tests', 'cpp']
FILES += ['INTEGRACAO-INDE.json', 'INTEGRACAO-01.md', 'VALIDACAO-INTEGRACAO01.md',
          'ROTEIRO-INTEGRACAO.md', 'Abrir-demo-integrada.sh', 'atualizar_fontes_integradas.py',
          'fontes/inde-P03c-schema37-original.tar.gz', 'fontes/inde-integrado-schema37.tar.gz']
FILES += ['evidencias/integracao01/resumo.json', 'evidencias/integracao01/bases-verificadas.json', 'evidencias/integracao01/migracao-verificada.json',
          'evidencias/integracao01/pgo-verificado.json', 'evidencias/integracao01/equivalencia.json',
          'evidencias/integracao01/flags-verificadas.json', 'evidencias/integracao01/base-alteracoes.diff',
          'evidencias/integracao01/testes.log', 'evidencias/integracao01/gtk-percursos.log',
          'evidencias/integracao01/navegador.txt', 'evidencias/integracao01/comparacao/resultado.json',
          'evidencias/integracao01/gtk/workspace-comic.png', 'evidencias/integracao01/gtk/workspace-novel.png',
          'evidencias/integracao01/editor-claro/02-estrutura-filtrada.png',
          'evidencias/integracao01/editor-claro/03-semantica-inspetor.png',
          'evidencias/integracao01/visual-claro/04-interior-expandido.png',
          'evidencias/integracao01/visual-claro/09-revisao-sequencia.png']


def package(target):
    paths = [BASE / name for name in FILES]
    for name in FOLDERS:
        paths.extend(p for p in (BASE / name).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix != '.pyc')
    if any(not p.is_file() for p in paths):
        raise ValueError('A entrega ainda não contém todos os arquivos do contrato.')
    manifest = json.loads((BASE / 'MANIFESTO-FONTES.json').read_text())
    names={str(p.relative_to(BASE)) for p in paths if p.name!='MANIFESTO-FONTES.json'}
    if names!=set(manifest['arquivos']):raise ValueError('O inventário de fontes diverge do manifesto.')
    for name, expected in manifest['arquivos'].items():
        path = (BASE / name).resolve()
        if not path.is_relative_to(BASE) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('Arquivo diferente do manifesto: ' + name)
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(target, 'x', ZIP_DEFLATED) as archive:
        for path in sorted(paths):
            archive.write(path, Path('INDE-Criacao-0.10') / path.relative_to(BASE))
    checksum = hashlib.sha256(target.read_bytes()).hexdigest()
    print(str(target.resolve()))
    print('SHA-256: ' + checksum)
    return checksum


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('destino', type=Path)
    package(parser.parse_args().destino)
