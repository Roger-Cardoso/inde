# Obra importada com identidades públicas — PUB-03B / laboratório 0.6

A demo usa a interface GTK e os serviços reais do INDE schema 28. Cria um Projeto novo com uma Obra, vários capítulos/Documentos, personagens canônicos, Site e Edição. A extensão de publicação evolui para schema 3; o schema autoral permanece 28.

## Experimentar

```sh
bash scripts/demos/Abrir-demo-obra-importada.sh
```

1. Selecione o capítulo no topo. A demo contém **A chegada**, com URL número **7**, e **O retorno**, com número **11**. A ordem de leitura vem da lista declarada, independentemente dos números.
2. Compare HTML aplicado e autoria salva. O texto e os recursos aparecem nos dois lados; nada é escolhido pela data do arquivo. Selecione uma fonte para cada capítulo. A criação só é habilitada quando todas as escolhas são válidas.
3. A demo permite, por exemplo, **autoria salva** no primeiro e **HTML aplicado** no segundo. Ambas as fontes são conferidas novamente, inclusive a fonte não escolhida.
4. Informe uma pasta que ainda não exista e clique **Criar Projeto e Edição**. A criação é feita em área reservada; a pasta final só aparece depois de concluir e reconferir as fontes. Destino existente não é substituído.
5. Clique **Abrir no INDE**. Use **Publicação → Catálogo do Site → Preparar Site → Abrir prévia**. Os cadastros de Site/Edição já fazem parte do novo Projeto, com destino Local. A geração e a abertura são ações separadas da importação.
6. Confira a biblioteca, os dois capítulos e os Auxílios. A Personagem Mara está associada aos dois Documentos da mesma Obra por um único ID de origem. Negrito, fala, perspectiva e âncoras das cenas acompanham a publicação.
7. Para repetir, escolha outra pasta de resultado. A abertura não apaga conferências nem Projetos existentes.

A demo não lê nem converte o acervo autoral. Os nomes e textos são fictícios. Os arquivos de `demo-obra-importada/` são preservados nas aberturas seguintes.

## O que conserva a identidade

Os IDs internos novos pertencem ao INDE. A Edição guarda, em transação, um mapa separado para os IDs públicos do Literature. O mapa cobre Obra, capítulos, personagens, cenas e marcações; âncoras e marcações possuem contexto de Documento, permitindo IDs locais iguais em capítulos diferentes.

A demo preserva:

- Obra pública `obra-literature-origem` e capítulos `capitulo-origem-1` / `capitulo-origem-2`;
- pasta anterior `obra-1`, endereço principal `mares-literature` e alias `mares-antigas`;
- URLs `capitulo-7` e `capitulo-11`, com o próximo número reservado em **18**;
- personagem público `character-origem`, suas referências de fala/perspectiva e as âncoras públicas das cenas.

Um capítulo novo escrito no INDE recebe 18; retirar/restaurar a Edição conserva mapa e reservas. Alterar o título não troca IDs. O formulário não pode apagar o mapa nem mudar a Obra ou a rota vinculada inadvertidamente. Rotas e aliases pertencentes a outras Edições são recusados. A Edição mantém os cadastros de origem mesmo quando uma parte é removida da seleção, conservando as reservas para recuperação.

O leitor usa os IDs de Obra/capítulo em progresso, posição e retomada. O cálculo do identificador de discussão também conserva esses IDs. **Dados já guardados no navegador dependem da mesma origem — protocolo, domínio e porta.** Mudar domínio ou usar outra porta de prévia não transfere automaticamente esse armazenamento. Os testes executam os scripts reais com os IDs anteriores e conferem também o leitor no navegador; não migram cookies, contas ou comentários remotos. Comentários e reações seguem desativados.

## Conferir fontes escolhidas

```sh
bash scripts/importacao/Revisar-obra.sh "/caminho/obra-literature.json"
```

O manifesto de entrada é um contrato do importador 0.6, preenchido a partir das fontes escolhidas. Ele não é apresentado como um formato de exportação que já exista no Gerenciador. A versão atual não varre automaticamente diretórios, backups ou lixeira e não deduz uma biblioteca inteira. Os caminhos abaixo são relativos ao manifesto:

```json
{
  "protocol": "inde.literature.work",
  "version": 1,
  "work": {
    "id": "id-publico-existente-da-obra",
    "title": "Título da Obra",
    "synopsis": "Sinopse da Obra.",
    "slug": "endereco-publico-atual",
    "folder": "obra-1",
    "aliases": ["endereco-publico-anterior"],
    "next_number": 18
  },
  "site": {
    "name": "Minha biblioteca",
    "author": "Minha autoria",
    "description": "Apresentação da biblioteca.",
    "theme": "violeta",
    "origin": "",
    "base_path": "/biblioteca"
  },
  "chapters": [
    {
      "id": "id-publico-existente-do-capitulo",
      "title": "Título do capítulo",
      "number": 7,
      "html": "aplicado/capitulo.html",
      "autoria": "autoria/capitulo.narrproj"
    }
  ]
}
```

A lista determina a ordem. Cada ID de capítulo precisa coincidir com a seção raiz das fontes escolhidas. Os números devem ser positivos, únicos e menores que `next_number`. Informe o contador real, incluindo reservas anteriores; o importador não o reduz aos capítulos presentes. IDs usados em caminhos aceitam letras ASCII, números, hífen e sublinhado. Pastas, slugs e aliases usam letras minúsculas, números e hífens, sem nomes reservados. Os temas são `violeta`, `verde` e `ambar`. Origem HTTPS e caminho base devem vir sem barra final; a origem pode ficar vazia para uso local. A raiz usa `base_path` vazio. O destino inicial é Local; GitHub é configurado no painel, sem envio neste corte.

É responsabilidade da seleção autoral representar os IDs públicos existentes, os números e as reservas corretas no manifesto. O programa verifica consistência, procedência e correspondência às fontes; não inventa esses dados nem atesta um manifesto contra um site remoto que não foi fornecido.

## Limites e recuperação

Aceita até 100 capítulos planos, com um Documento por capítulo. A autoria continua em `.narrproj` v5, no subconjunto documentado em `ROTEIRO-IMPORTACAO.md`. O leitor passou a representar os três estilos básicos e as âncoras das cenas. Sobreposição de fala/pensamento, anotação atravessando parágrafo, campos sem tradutor, hierarquia de cenas, listas, glossário e os demais recursos ainda sem correspondência impedem a conversão daquela fonte. A preparação nativa tem limite de 16 MiB; não há truncamento.

Personagens com o mesmo ID e definições incompatíveis entre capítulos exigem reconciliação anterior. Nomes iguais com IDs distintos continuam distintos. O recibo `data/importacao-literature.json` versão 2 registra escolhas/remapeamentos, e as fontes brutas ficam em `data/importacao-fontes/`. Abrir e editar o Projeto criado não depende dos originais continuarem disponíveis.

A importação de um capítulo do corte 0.5 continua disponível e continua bloqueada para publicação enquanto não houver um mapa público validado. O corte 0.6 não adota automaticamente esses Projetos, não modifica Projetos existentes e não combina várias Obras de origem em um Projeto autoral existente.

A extensão schema 1/2 é consultada sem migração. Na primeira gravação, uma cópia SQLite íntegra é guardada em `data/publicacao-recuperacao/schemaN-UUID.sqlite3`, e as novas formas são gravadas em transação. O banco autoral não é alterado pela migração da publicação. Versões anteriores do aplicativo recusam a extensão schema 3. Os retratos e as produções que contêm identidades vinculadas usam protocolo versão 2, para que o gerador anterior também os recuse em vez de ignorar o mapa. Produções nativas sem vínculo continuam na versão 1.

Continuam pendentes: preparar a seleção do acervo por uma interface própria, tradutores para os recursos autorais adicionais, hierarquias completas, integração entre Projetos, serviços remotos, envio GitHub e instalador com runtime. O próximo corte de importação é PUB-03C, voltado aos recursos que ainda impedem usar as fontes autorais reais. O pacote 0.6 permanece laboratório sujeito à avaliação humana.
