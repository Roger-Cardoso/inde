"""Second, contextual UI-literal pass. Never rerun the first migration.

This mechanical pass records exact before/after literals and skips the glossary,
comments, raw JSON, lower-case technical keys and persisted/core contracts.
"""
import json
from pathlib import Path
from apply_author_vocabulary import ROOT, TOKEN, LITERAL

GRAMMAR = [
 ("o categoria de elemento", "a categoria de elemento"),
 ("do categoria de elemento", "da categoria de elemento"),
 ("Categoria de elemento criado", "Categoria de elemento criada"),
 ("Categoria de elemento atualizado", "Categoria de elemento atualizada"),
 ("Categoria de elemento removido", "Categoria de elemento excluída"),
 ("categoria de elemento selecionado", "categoria de elemento selecionada"),
 ("da universo selecionada", "do universo selecionado"),
 ("esta universo", "este universo"), ("desta universo", "deste universo"),
 ("uma universo", "um universo"), ("a universo", "o universo"),
 ("da elemento do manuscrito", "do elemento do manuscrito"),
 ("uma elemento do manuscrito", "um elemento do manuscrito"),
 ("uma elemento da história", "um elemento da história"),
 ("da elemento da história", "do elemento da história"),
 ("Elemento da história vinculada", "Elemento da história associado"),
 ("elemento da história selecionada", "elemento da história selecionado"),
 ("elemento da história continua sendo a mesma", "elemento da história continua sendo o mesmo"),
 ("as trechos marcados", "os trechos marcados"),
 ("das trechos marcados", "dos trechos marcados"),
 ("esta trecho marcado", "este trecho marcado"),
 ("desta trecho marcado", "deste trecho marcado"),
 ("uma trecho marcado", "um trecho marcado"),
 ("a trecho marcado", "o trecho marcado"),
 ("da trecho marcado", "do trecho marcado"),
 ("trecho marcado selecionada", "trecho marcado selecionado"),
 ("Trecho marcado criada", "Trecho marcado criado"),
 ("trecho marcado removida", "trecho marcado removido"),
 ("Tipo de nome alternativo", "Tipo de nome alternativo"),
 ("Tipos autorais", "Tipos de marcação personalizados"),
 ("tipos autorais", "tipos de marcação personalizados"),
 ("Registrar tipo autoral", "Criar tipo de marcação personalizado"),
 ("Escolher papel narrativo", "Escolher tipo de marcação"),
 ("escolha de papel narrativo", "escolha de tipo de marcação"),
 ("um papel narrativo", "um tipo de marcação"),
 ("Todos os papéis", "Todos os tipos de marcação"),
 ("Filtrar camadas por papel", "Filtrar marcações por tipo"),
 ("Escopo das marcações do texto", "Onde procurar marcações"),
 ("Escopo", "Onde se aplica"),
 ("escopo", "área da consulta"),
 ("Declarações globais — sem trecho marcado", "Para todo o documento — sem trecho específico"),
 ("Declaração global do Documento — sem trecho marcado", "Para todo o documento — sem trecho específico"),
 ("de intervalo (contexto local)", "deste trecho"),
 ("global do Documento", "para todo o documento"),
 ("Perspectiva no cursor", "Ponto de vista aqui"),
 ("Perspectiva do Documento", "Ponto de vista do documento"),
 ("Sem colocação editorial", "Ainda fora do manuscrito"),
 ("Todas as colocações editoriais", "Todas as posições no manuscrito"),
 ("Nome inverso", "Como se lê no sentido contrário"),
 ("Associar materialização textual", "Associar trecho à ficha"),
 ("Associar como materialização, evidência ou informação (marcação do texto)", "Escolher como o trecho se relaciona com a ficha"),
 ("Apresenta a ficha no texto — o trecho apresenta o elemento da história/informação", "Apresenta o elemento ou informação"),
 ("Evidência textual — associação declarada, não fato confirmado", "Serve de evidência — não confirma um fato automaticamente"),
 ("Entidades herdadas de elementos filhos", "Elementos da história apresentados nas partes internas"),
 ("Elementos da história herdadas de elementos internos", "Elementos da história apresentados nas partes internas"),
 ("Biblioteca de Tempo", "Calendários e relógios"),
 ("Ocorrência de composição", "Uso de bloco na montagem"),
 ("ocorrência de composição", "uso de bloco na montagem"),
 ("Reconciliação textual", "Conferir texto e planejamento"),
 ("presença canônica", "presença registrada no mundo da história"),
 ("Presença canônica", "Presença registrada no mundo da história"),
 ("relações canônicas", "relações registradas no mundo da história"),
 ("Relações canônicas", "Relações registradas no mundo da história"),
 ("relação canônica", "relação registrada no mundo da história"),
 ("fato canônico", "fato no cadastro do mundo da história"),
 ("fatos canônicos", "fatos no cadastro do mundo da história"),
 ("Sugestão canônica", "Sugestão a partir dos cadastros da história"),
 ("Informação canônica", "Informação do cadastro da história"),
 ("Informação canônica", "Informação do cadastro da história"),
 ("Nomes canônicos", "Nomes principais"),
 ("semânticas resolvidas", "do texto localizadas"),
]
TIME = [
 ("Nome do sistema", "Nome do calendário e relógio"),
 ("Novo sistema", "Novo calendário e relógio"),
 ("Sistema da data", "Calendário da data"),
 ("Sistema inicial", "Calendário inicial"),
 ("Sistema sucessor", "Novo calendário"),
 ("Salvar sistema", "Salvar calendário e relógio"),
 ("Criar sistema próprio", "Criar calendário e relógio próprios"),
 ("Excluir este sistema", "Excluir este calendário e relógio"),
 ("Excluir sistema", "Excluir calendário e relógio"),
 ("Nenhum sistema cadastrado", "Nenhum calendário e relógio cadastrado"),
 ("este sistema", "este calendário e relógio"),
 ("deste sistema", "deste calendário e relógio"),
 ("sistema inicial", "calendário inicial"),
 ("sistema de calendário", "calendário"),
 ("sistema de origem", "calendário de origem"),
 ("sistema na Biblioteca", "calendário na biblioteca"),
 ("outro sistema", "outro calendário e relógio"),
 ("o sistema", "o calendário e relógio"),
 ("um sistema", "um calendário e relógio"),
 ("seu sistema", "seu calendário e relógio"),
 ("sistemas independe", "calendários e relógios independe"),
 ("Sistemas com datas", "Calendários e relógios com datas"),
 ("Sistema salvo", "Calendário e relógio salvos"),
 ("Sistema excluído", "Calendário e relógio excluídos"),
 ("o eixo", "a linha do tempo"),
 ("O eixo", "A linha do tempo"),
 ("do eixo", "da linha do tempo"),
 ("Do eixo", "Da linha do tempo"),
 ("ao eixo", "à linha do tempo"),
 ("deste eixo", "desta linha do tempo"),
 ("este eixo", "esta linha do tempo"),
 ("Neste eixo", "Nesta linha do tempo"),
 ("neste eixo", "nesta linha do tempo"),
 ("um eixo", "uma linha do tempo"),
 ("nenhum eixo", "nenhuma linha do tempo"),
 ("Nenhum eixo", "Nenhuma linha do tempo"),
 ("de eixo", "de linha do tempo"),
 ("dos eixos", "das linhas do tempo"),
 ("eixos", "linhas do tempo"),
 ("Eixo do marco", "Linha do tempo do marco"),
 ("Eixo", "Linha do tempo"),
 ("Ocorrência", "Quando acontece"),
 ("Nome da referência", "Nome da data ou período"),
 ("Referência existente", "Data ou período já registrado"),
]
LINGUISTIC = [
 ("acepções", "sentidos da palavra"), ("acepção", "sentido da palavra"),
 ("Acepções", "Sentidos da palavra"), ("Acepção", "Sentido da palavra"),
 ("Construções e frames", "Construções e cenários de significado"),
 ("quarentena Onto.PT", "base experimental Onto.PT"),
 ("QUARENTENA", "BASE EXPERIMENTAL — EM AVALIAÇÃO"),
 ("CATÁLOGO ACEITO", "BASE REVISADA"),
 ("catálogo aceito", "base revisada"),
 ("catálogo local aceito", "base local revisada"),
 ("definições promovidas", "definições aprovadas na revisão do catálogo"),
 ("Definições promovidas", "Definições aprovadas na revisão do catálogo"),
 ("sem binding automático", "sem associação automática"),
 ("score semântico", "critério de ordenação"),
 ("POS da fonte", "Classe gramatical da fonte"),
 (" · synset: ", " · grupo de sinônimos deste sentido: "),
]
record=[]
if (ROOT.parent/'checkpoint/vocabulary-polish-2026-10-12.json').exists():
 raise SystemExit('One-time polish already recorded; refusing to replay it.')
files=sorted([*ROOT.joinpath('src/ui').glob('*.cpp'),*ROOT.joinpath('src/ui').glob('*.hpp'),*ROOT.joinpath('include/inde/ui').glob('*.hpp')])
for path in files:
 if path.name.startswith('author_vocabulary'): continue
 before=path.read_text();changes=[]
 def replace(match):
  token=match.group()
  if not token.startswith('"'):return token
  try: value=''.join(json.loads(v) for v in LITERAL.findall(token))
  except ValueError:return token
  if not any(c.isspace() or ord(c)>127 for c in value) and not value[:1].isupper():return token
  result=value
  rules=list(GRAMMAR)
  if path.name.startswith(('temporal_','planning_workspace_time','planning_workspace_temporal','calendar_','graphs_workspace')):rules+=TIME
  if path.name=='writing_workspace_linguistic.cpp':rules+=LINGUISTIC
  if path.name.startswith('narrative_'):
   rules += [('Ações da estrutura','Ações do plano'),('Salvar como Modelo','Salvar como modelo de planejamento'),('Ativar','Usar como plano atual'),('Ordem própria da linha selecionada','Ordem nesta linha narrativa')]
  # Longest-first, non-recursive, like the first pass.
  mapping=dict(rules)
  import re
  pattern=re.compile('|'.join((r'(?<!\w)' if v[0].isalnum() else '')+re.escape(v)+(r'(?!\w)' if v[-1].isalnum() else '') for v in sorted(mapping,key=len,reverse=True)))
  result=pattern.sub(lambda m:mapping[m.group()],value)
  if result==value:return token
  changes.append({'line':before.count('\n',0,match.start())+1,'before':value,'after':result})
  return json.dumps(result,ensure_ascii=False)
 after=TOKEN.sub(replace,before)
 if changes:
  path.write_text(after)
  record.append({'path':str(path.relative_to(ROOT)),'literals':changes})
target=ROOT.parent/'checkpoint/vocabulary-polish-2026-10-12.json'
target.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'files':len(record),'literals':sum(len(f['literals']) for f in record)}))
