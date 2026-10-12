"""Controlled mechanical migration of UI literals, never persisted keys or data.

Ordinary C++ string literals only, with file-context rules. Comments, raw JSON,
identifiers, SQL and core/persistence files are outside the migration. This is
also the audit inventory: --check is read-only; --apply records changed literals.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMON = [
    ("Nova propriedade intelectual", "Novo universo"),
    ("novo tipo de entidade", "nova categoria de elemento"),
    ("Novo tipo de entidade", "Nova categoria de elemento"),
    ("uma unidade editorial", "um elemento do manuscrito"),
    ("Uma unidade editorial", "Um elemento do manuscrito"),
    ("A entidade", "O elemento da história"),
    ("Esta entidade", "Este elemento da história"),
    ("Uma entidade", "Um elemento da história"),
    ("Entidade criada", "Elemento criado"),
    ("entidade criada", "elemento criado"),
    ("entidade removida", "elemento excluído"),
    ("entidade de origem", "elemento de origem"),
    ("Entidade de origem", "De"),
    ("Entidade de destino", "Para"),
    ("Conclua a composição do teclado", "Conclua a entrada do teclado"),
    ("Universo / Propriedade Intelectual", "Universo"),
    ("universo / Propriedade Intelectual", "universo"),
    ("universo / propriedade intelectual", "universo"),
    ("Propriedades intelectuais", "Universos"),
    ("propriedades intelectuais", "universos"),
    ("Propriedade Intelectual", "Universo"),
    ("Propriedade intelectual", "Universo"),
    ("propriedade intelectual", "universo"),
    ("Pesquisar IPs e Obras", "Pesquisar universos e obras"),
    ("Catálogo do projeto", "Obras e universos"),
    ("Título da propriedade", "Nome do universo"),
    ("tipos de entidade", "categorias de elemento"),
    ("Tipos de entidade", "Categorias de elemento"),
    ("tipo de entidade", "categoria de elemento"),
    ("Tipo de entidade", "Categoria de elemento"),
    ("Tipo da entidade", "Categoria do elemento"),
    ("Explorar entidades", "Personagens e mundo"),
    ("entidade narrativa", "elemento da história"),
    ("entidades narrativas", "elementos da história"),
    ("entidade selecionada", "elemento selecionado"),
    ("entidade atual", "elemento atual"),
    ("entidade relacionada", "elemento relacionado"),
    ("entidade vinculada", "elemento associado"),
    ("entidades vinculadas", "elementos associados"),
    ("Nenhuma entidade", "Nenhum elemento"),
    ("Outra entidade", "Outro elemento"),
    ("outra entidade", "outro elemento"),
    ("Nova entidade", "Novo elemento"),
    ("nova entidade", "novo elemento"),
    ("a entidade", "o elemento da história"),
    ("da entidade", "do elemento da história"),
    ("esta entidade", "este elemento da história"),
    ("desta entidade", "deste elemento da história"),
    ("uma entidade", "um elemento da história"),
    ("Entidades", "Elementos da história"),
    ("entidades", "elementos da história"),
    ("Entidade", "Elemento da história"),
    ("entidade", "elemento da história"),
    ("Obras e escopo", "Em quais obras aparece"),
    ("Obras da entidade", "Em quais obras aparece"),
    ("Contraparte", "Outro elemento da relação"),
    ("contraparte", "outro elemento da relação"),
    ("Direcionalidade", "Como a relação funciona"),
    ("Direcional", "Tem um sentido"),
    ("Simétrica", "Vale nos dois sentidos"),
    ("Nome de origem para destino", "Como se lê de um elemento para o outro"),
    ("Nome inverso", "Como se lê no sentido contrário"),
    ("Nome canônico", "Nome principal"),
    ("nome canônico", "nome principal"),
    ("Editar alias", "Editar nome alternativo"),
    ("Novo alias", "Novo nome alternativo"),
    ("Salvar alias", "Salvar nome alternativo"),
    ("Categoria do alias", "Tipo de nome alternativo"),
    ("Modelar a realidade ficcional", "Organizar personagens e o mundo da história"),
    ("modelar a realidade ficcional", "organizar personagens e o mundo da história"),
    ("Camadas Semânticas", "Marcações do texto"),
    ("Camadas semânticas", "Marcações do texto"),
    ("camadas semânticas", "marcações do texto"),
    ("camada semântica", "marcação do texto"),
    ("Lente Semântica", "Realçar marcações"),
    ("Lente semântica", "Realçar marcações"),
    ("lente semântica", "realce das marcações"),
    ("Distribuição semântica", "Distribuição das marcações"),
    ("distribuição semântica", "distribuição das marcações"),
    ("anotação semântica", "marcação do texto"),
    ("Anotação semântica", "Marcação do texto"),
    ("âncoras semânticas", "trechos das marcações"),
    ("Âncoras textuais", "Trechos marcados"),
    ("Esta âncora", "Este trecho marcado"),
    ("esta âncora", "este trecho marcado"),
    ("desta âncora", "deste trecho marcado"),
    ("uma âncora", "um trecho marcado"),
    ("a âncora", "o trecho marcado"),
    ("da âncora", "do trecho marcado"),
    ("Nova âncora", "Marcar trecho"),
    ("Usos desta âncora", "Onde este trecho é usado"),
    ("Âncora ambígua", "Há mais de um trecho possível"),
    ("Âncora indisponível", "Trecho indisponível"),
    ("âncora não resolvida", "trecho não localizado"),
    ("reancoragem explícita necessária", "escolha novamente o trecho"),
    ("reancoragem", "nova localização do trecho"),
    ("reancore", "marque novamente o trecho"),
    ("Âncoras", "Trechos marcados"),
    ("âncoras", "trechos marcados"),
    ("Âncora", "Trecho marcado"),
    ("âncora", "trecho marcado"),
    ("Materialização associativa", "Apresenta a ficha no texto"),
    ("materialização associativa", "apresentação da ficha no texto"),
    ("Perspectiva tipada do Documento", "Ponto de vista do documento"),
    ("perspectiva tipada", "ponto de vista estruturado"),
    ("Perspectiva situada", "Ponto de vista deste trecho"),
    ("Pessoa narrativa", "Pessoa da narração"),
    ("Focalizadores", "Percepção de"),
    ("Falante", "Quem fala"),
    ("Pensador", "Quem pensa"),
    ("Atos de fala", "O que a fala faz"),
    ("Alvos da descrição", "O que está sendo descrito"),
    ("Ocorrência de entidade", "Menção a um elemento da história"),
    ("Comandos inline", "Ações no texto"),
    ("Comandos Inline", "Ações no texto"),
    ("comandos inline", "ações no texto"),
    ("Comando inline", "Ação no texto"),
    ("sugestões inline", "sugestões no texto"),
    ("Modelos inline do Documento", "Textos prontos do documento"),
    ("modelo inline", "texto pronto"),
    ("modelos inline", "textos prontos"),
    ("Retomada (callback)", "Retomada"),
    ("Recorrências", "Palavras repetidas"),
    ("Palavras recorrentes", "Palavras repetidas"),
    ("Identidade e proveniência", "Identificadores, origem e histórico"),
    ("Avisos e proveniência", "Avisos, origens e histórico"),
    ("proveniência", "origem e histórico"),
    ("Proveniência", "Origem e histórico"),
    ("Checkpoint", "Ponto de recuperação"),
    ("checkpoint", "ponto de recuperação"),
    ("Journal", "Registro de recuperação"),
    ("journal", "registro de recuperação"),
    ("Templates", "Modelos"),
    ("templates", "modelos"),
    ("template", "modelo"),
    ("Deletar", "Excluir"),
    ("deletar", "excluir"),
    ("Guardar contexto atual", "Salvar este espaço de trabalho"),
    ("Substituir pelo contexto atual", "Atualizar este espaço com a configuração atual"),
    ("Histórico de operações do Projeto", "Histórico de alterações do projeto"),
    ("Alternar painel contextual", "Mostrar ou ocultar detalhes"),
    ("Lente de visualização", "Modo de visualização"),
    ("Lente:", "Visualização:"),
    ("Linha temporal", "Cronologia"),
    ("Linha do tempo ficcional", "Cronologia"),
    ("Tempo ficcional", "Tempo da história"),
    ("tempo ficcional", "tempo da história"),
    ("Eixo de tempo ficcional", "Linha do tempo"),
    ("eixo de tempo ficcional", "linha do tempo"),
    ("Eixo temporal", "Linha do tempo"),
    ("eixo temporal", "linha do tempo"),
    ("eixos temporais", "linhas do tempo"),
    ("Ponto temporal", "Marco na linha do tempo"),
    ("ponto temporal", "marco na linha do tempo"),
    ("pontos temporais", "marcos na linha do tempo"),
    ("Ordem temporal", "Posição na linha do tempo"),
    ("Rótulo do ponto", "Nome do marco"),
    ("Recorte temporal", "Período mostrado"),
    ("recorte temporal", "período mostrado"),
    ("Precisão temporal", "Quanto sabemos sobre a data"),
    ("Intervalo inclusivo", "Entre as datas, incluindo início e fim"),
    ("Escolher data do corte", "Escolher data da mudança de calendário"),
    ("Modo de sucessão", "Como o calendário passa a ser usado"),
    ("Substituir e aposentar", "Substituir o anterior a partir desta data"),
    ("Adotar e coexistir", "Usar o novo mantendo o anterior disponível"),
    ("Salvar como nova revisão", "Salvar nova versão das regras"),
    ("Visual raster", "Visual em pixels"),
    ("visual suave ou raster", "visual suave ou em pixels"),
    ("Visível a partir do LOD (0–6)", "Mostrar a partir deste nível de detalhe (0–6)"),
    ("LOD ", "Nível de detalhe "),
    ("Inspetor temporal", "Detalhes na cronologia"),
    ("Estrutura editorial", "Organização do manuscrito"),
    ("estrutura editorial", "organização do manuscrito"),
    ("unidade editorial", "elemento do manuscrito"),
    ("Unidade editorial", "Elemento do manuscrito"),
    ("colocações editoriais", "posições no manuscrito"),
    ("colocação editorial", "posição no manuscrito"),
    ("Novo elemento raiz", "Adicionar no nível principal"),
    ("Novo filho", "Adicionar dentro do selecionado"),
    ("elementos filhos", "elementos internos"),
    ("elemento pai", "elemento que contém este item"),
    ("elemento filho", "elemento interno"),
    ("Fila editorial", "Textos para revisar"),
    ("revisões documentais", "versões de revisão dos documentos"),
    ("Fontes, omissões e perdas", "Textos incluídos, itens omitidos e limitações da exportação"),
    ("Escolher destino e produzir", "Escolher destino e exportar"),
    ("Cancelar produção", "Cancelar exportação"),
    ("Perfil de saída", "Tipo de apresentação"),
    ("snapshot preparado", "versão preparada para exportação"),
    ("snapshot confirmado", "versão preparada e conferida"),
    ("sem merge", "sem mesclagem de versões"),
    ("Dicionário semântico local", "Dicionário"),
    ("Dicionário semântico", "Dicionário"),
    ("Explorar construções e frames", "Construções e cenários de significado"),
]
NARRATIVE = [
    ("Estruturas narrativas", "Planos narrativos"),
    ("estruturas narrativas", "planos narrativos"),
    ("Estrutura narrativa", "Plano narrativo"),
    ("estrutura narrativa", "plano narrativo"),
    ("Estruturas", "Planos"),
    ("estruturas", "planos"),
    ("estrutura ativa", "plano atual"),
    ("Nova estrutura", "Novo plano"),
    ("nova estrutura", "novo plano"),
    ("Duplicar estrutura", "Duplicar plano"),
    ("Derivar estrutura", "Criar alternativa a partir deste plano"),
    ("Derivar", "Criar alternativa"),
    ("Modelos reutilizáveis", "Modelos de planejamento"),
    ("Autoridade do vínculo", "A qual ordem esta regra se aplica?"),
    ("Montagem narrativa", "Ordem do plano narrativo"),
    ("montagem narrativa", "ordem do plano narrativo"),
    ("Fila de escrita", "Ordem de escrita"),
    ("fila de escrita", "ordem de escrita"),
    ("Designador", "Código curto"),
    ("Intenção / propósito", "Função desta parte"),
    ("Perspectiva declarada", "Ponto de vista planejado"),
    ("Unidades narrativas", "Partes da narrativa"),
    ("unidades narrativas", "partes da narrativa"),
    ("Unidade narrativa", "Parte da narrativa"),
    ("unidade narrativa", "parte da narrativa"),
    ("Unidades", "Partes"),
    ("unidades", "partes"),
    ("Unidade", "Parte"),
    ("unidade", "parte"),
    ("Papéis narrativos", "Papéis na história"),
    ("Atribuir papel", "Definir papel"),
    ("Remover atribuição", "Retirar este papel"),
    ("Precedência, causalidade e alternativas", "Ordem, causas e alternativas"),
    ("Precede — ordem não classificada", "Vem antes de — ordem ainda não definida"),
    ("Reconciliação textual", "Conferir texto e planejamento"),
    ("reconciliação", "conferência entre texto e plano"),
    ("realização textual", "texto que desenvolve esta parte"),
    ("texto ancorado", "trecho marcado"),
    ("trecho ancorado", "trecho marcado"),
    ("Separar prosa", "Criar texto independente para esta associação"),
    ("autoridade textual", "texto de origem"),
    ("divergência declarada", "diferença intencional entre texto e plano"),
    ("Exceção autoral", "Escolha intencional do autor"),
    ("exceção autoral", "escolha intencional do autor"),
]
COMPOSITION = [
    ("Ocorrências", "Usos dos blocos"),
    ("ocorrência de composição", "uso de bloco na montagem"),
    ("Ocorrência de composição", "Uso de bloco na montagem"),
    ("ocorrências", "usos dos blocos"),
    ("ocorrência", "uso do bloco"),
    ("Ocorrência", "Uso do bloco"),
    ("Composições", "Montagens de texto"),
    ("Composição", "Montagem de texto"),
    ("composição", "montagem de texto"),
    ("Fixar versão apresentada", "Manter esta versão neste uso"),
    ("Acompanhar versão atual do bloco", "Acompanhar a versão em uso do bloco"),
    ("Tornar bloco independente", "Criar cópia independente para este uso"),
    ("Aplicar versão do bloco", "Usar esta versão na montagem"),
    ("Retirar ocorrência da leitura", "Retirar este uso da montagem"),
    ("versão aplicada", "versão em uso na montagem"),
    ("versões aplicadas", "versões em uso na montagem"),
    ("Promover a bloco", "Transformar em bloco reutilizável"),
    ("Mapa de blocos", "Posições e origens dos blocos"),
    ("Leitura derivada", "Prévia da montagem"),
    ("Snapshot", "Cópia preservada da origem"),
    ("snapshot", "cópia preservada da origem"),
]

# A C++ lexer for comments/raw literals/ordinary adjacent literals. Do not
# translate raw JSON, canonical keys, or text occurring only in comments.
TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"|"(?:\\.|[^"\\])*"(?:\s*"(?:\\.|[^"\\])*")*', re.S)
LITERAL = re.compile(r'"(?:\\.|[^"\\])*"', re.S)

def translate(value, path):
    # Lower-case ASCII strings without spaces are normally action names,
    # serialization keys or paths, not prose. Never rename such contracts.
    if not any(c.isspace() or ord(c) > 127 for c in value) and not value[:1].isupper():
        return value
    rules = COMMON
    name = path.name
    narrative = name.startswith("narrative_") or name == "writing_workspace_narrative.cpp"
    composition = "composition_page" in name or name.startswith("blocks_lab_")
    if narrative:
        rules = NARRATIVE + rules
    if composition:
        rules = COMPOSITION + rules
    result = value
    # One-pass replacements avoid translating the translation again.
    mapping = dict(rules)
    pattern = re.compile("|".join((r"(?<!\w)" if k[0].isalnum() else "") + re.escape(k) +
                                 (r"(?!\w)" if k[-1].isalnum() else "")
                                 for k in sorted(mapping, key=len, reverse=True)))
    result = pattern.sub(lambda m: mapping[m.group()], result)
    if name.startswith(("temporal_", "planning_workspace_time", "graphs_workspace")):
        for old, new in [("Sistemas deste Projeto", "Calendários e relógios deste projeto"),
                         ("sistema temporal", "calendário e relógio"),
                         ("Sistema temporal", "Calendário e relógio"),
                         ("Eixo:", "Linha do tempo:"), ("Nome do eixo", "Nome da linha do tempo"),
                         ("Eixo inteiro", "Linha do tempo inteira")]:
            result = result.replace(old, new)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--record", type=Path)
    args = parser.parse_args()
    if args.apply and args.record and args.record.exists():
        raise SystemExit("This one-time migration already has a record; refusing to overwrite it.")
    files = sorted([*ROOT.joinpath("src/ui").glob("*.cpp"),
                    *ROOT.joinpath("src/ui").glob("*.hpp"),
                    *ROOT.joinpath("include/inde/ui").glob("*.hpp"),
                    *ROOT.joinpath("src/experimental").glob("blocks_lab_*.cpp"),
                    ROOT / "include/inde/experimental/blocks_lab_window.hpp"])
    record = {"mode": "applied" if args.apply else "read-only", "files": []}
    for path in files:
        if path.name.startswith("author_vocabulary"):
            continue
        original = path.read_text()
        changes = []
        def replace(match):
            token = match.group()
            if not token.startswith('"'):
                return token
            try:
                value = "".join(json.loads(v) for v in LITERAL.findall(token))
            except (ValueError, json.JSONDecodeError):
                return token
            new = translate(value, path)
            if new == value:
                return token
            changes.append({"line": original.count("\n", 0, match.start()) + 1,
                            "before": value, "after": new})
            return json.dumps(new, ensure_ascii=False)
        result = TOKEN.sub(replace, original)
        if changes:
            record["files"].append({"path": str(path.relative_to(ROOT)),
                                   "before_sha256": hashlib.sha256(original.encode()).hexdigest(),
                                   "after_sha256": hashlib.sha256(result.encode()).hexdigest(),
                                   "literals": changes})
            if args.apply:
                path.write_text(result)
    if args.record:
        args.record.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"files": len(record["files"]), "literals": sum(len(f["literals"]) for f in record["files"]), "mode": record["mode"]}))

if __name__ == "__main__":
    main()
