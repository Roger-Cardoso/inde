# INDE — Integrated Narrative Development Environment

O **INDE** é um aplicativo desktop nativo para Linux voltado ao desenvolvimento, planejamento, organização e escrita de projetos narrativos.

O programa reúne em um mesmo ambiente informações editoriais, elementos narrativos, documentos, relações e referências temporais, mantendo os dados do projeto armazenados localmente.

O INDE está atualmente em desenvolvimento.

O corte E5-S1 acrescenta um laboratório experimental separado de blocos,
colocações, revisões, variantes e perfis, mais comparação física executável de
texto contínuo/chunks/piece table. Não salva blocos no Projeto nem substitui
o editor aprovado. Consulte [contrato e limites do spike](docs/editor-stage5-spike-2026-10-04.md).

Neste candidato local, o seletor de cor tem paleta visual, tonalidade/saturação,
RGB e hexadecimal sincronizados e prévia. Perspectiva tipada global não exige
seleção nem âncora artificial; vínculos manuais ao tempo ficcional declaram eixo
e ponto/período, com busca paginada, prévia e proteções de integridade.
Consulte [o contrato do novo incremento](docs/editor-semantic-3-2026-10-04.md).

O editor mantém três fileiras na ordem solicitada, com
seis níveis de título, Subtítulo/Citação/Corpo, fonte, tamanho, cor e alinhamento
persistentes, comentários ancorados e modelos inline literais por Documento.
Consulte [o contrato da barra e dos comandos](docs/editor-toolbar-2026-10-03.md).
Não há scripts ou inserção automática. Candidato schema 18, sem release;
aceite humano do incremento semântico 3 registrado em 2026-10-04;
somente o novo spike E5-S1 permanece em revisão.
Avaliar em cópias, não no projeto original.

O editor reúne Camadas Semânticas, Lente Semântica e
Marcar Seleção (sem o atalho redundante Busca semântica). Camadas contém a busca
por Projeto/Obra. Perspectiva situada declara pessoa, focalização e focalizadores
em um intervalo, preservando a perspectiva livre do Documento. Relações entre
marcações do Projeto têm seleção paginada, prévia explícita, nota autoral,
navegação pelas duas pontas e undo/redo. Consulte
[o contrato e os limites do incremento](docs/editor-semantic-2-2026-10-03.md).

## Principais recursos

A versão atual inclui:

* criação, abertura, salvamento e cópia de projetos `.inde`;
* catálogo de Propriedades Intelectuais e Obras;
* estrutura editorial hierárquica;
* gerenciamento de entidades narrativas;
* tipos de entidades personalizados;
* relações entre entidades;
* planejamento temporal ficcional;
* acontecimentos e participantes;
* presença e localização de entidades;
* vínculos entre entidades narrativas e estruturas editoriais;
* estruturas narrativas e modelos associados a Obras;
* Biblioteca de Documentos;
* editor de texto formatado;
* consulta explícita ao dicionário semântico local e navegação somente leitura
  por construções/frames autorizados;
* âncoras e referências entre documentos e entidades;
* pesquisa e filtros contextuais;
* timeline ficcional consultiva;
* Cartografia: planetas procedurais locais, prévia/aceite, câmera e camadas;
* edição de relevo com prévia, aplicação, undo/redo e bloqueios autorais;
* posições de Locais compartilhados, medição esférica e exportação SVG da vista;
* revisão ortográfica pt-BR;
* dicionários pessoais;
* persistência local utilizando SQLite.

Alguns recursos permanecem em desenvolvimento e podem sofrer alterações incompatíveis entre versões.

### Editor — consolidação e incremento semântico 2 de 2026-10-03

Em **Camadas semânticas**, o seletor alterna entre o Documento atual, incluindo alterações não
salvas, e os usos persistidos no Projeto. Ambos usam filtros, cartões e paginação
uniformes. A busca no Projeto não força salvamento nem atribui significado.

**Lentes semânticas** usa fundos translúcidos atenuados, preservando as letras e
a formatação autoral. O tooltip identifica os papéis sobrepostos e um campo útil
do tipo, sem transformar a lente em conteúdo ou em operação de undo.

Em **Recuperação → Retenção e exportação…**, consulte a prévia, exporte um
checkpoint sem limpar ou selecione registros fechados elegíveis. O padrão conserva
os últimos 30 dias; pendentes, ativos e ilegíveis ficam protegidos. Limpar exige
confirmação separada, exportação reaberta e verificada e transação integral. Não
há limpeza automática. Os arquivos ficam em `recovery/editor-v1/exports` dentro
do Projeto. São checkpoints completos da sessão, não backup integral do Projeto
ou exportação editorial DOCX/EPUB/PDF; não há importação gráfica desses arquivos.

A composição IME continua nativa do GTK. Texto provisório não vira conteúdo
salvo e a confirmação é isolada da digitação anterior no histórico reversível.
O caminho automatizado usa IMContextSimple; a validação do método de entrada
efetivamente utilizado pelo autor e da sessão humana de 45–60 minutos permanece
pendente. Confira o contrato, as limitações e o gate em
[Consolidação do editor](docs/editor-consolidation-2026-10-03.md).

### Editor — escala e recuperação da Etapa 1 (candidato local)

No Documento aberto, **Localizar** ou **Ctrl+F** abre a busca literal Unicode.
**Substituir…** ou **Ctrl+H** mostra a substituição; **F3** e **Shift+F3** navegam,
e **Esc** fecha a busca. Maiúsculas/minúsculas e palavra inteira são opções
explícitas. Substituição por vazio apaga o trecho. **Substituir tudo** é uma única
operação de desfazer, preserva identidade de âncoras/referências e separa a
digitação anterior. Não há expressões regulares ou normalização Unicode implícita.

A sessão de edição possui histórico limitado em memória, com deltas compactos
e fallback para snapshots. O histórico de undo não é persistido; a recuperação
usa checkpoints independentes, descritos abaixo.
O autosave usa um worker isolado, com um snapshot ativo e um pendente substituível.
Confirmações atrasadas não marcam edições mais novas como salvas. Conflitos com
alterações externas são recusados sem sobrescrever o banco ou descartar o texto
local. Salvar, trocar de Documento e fechar aguardam uma barreira de persistência;
essas transições podem bloquear a interface. Undo/redo e operações compostas
restauram o delta textual em GTK; abertura ainda usa restauração completa.

**Copiar/colar** oferece texto puro por padrão e estilos somente por escolha.
**Ctrl+C**, **Ctrl+X** e **Ctrl+V** usam texto puro; **Ctrl+Shift+C** copia estilos.
Para colar estilos, use o menu explícito. Âncoras, entidades e bindings nunca
são importados do clipboard; vínculos existentes no destino preservam identidade.
O histórico INDE é a única autoridade de undo/redo, inclusive nas entradas
**Desfazer INDE** e **Refazer INDE** do menu contextual.

**Recuperação** mostra checkpoints de sessões inativas com prévia e escolha
explícita. Restaurar o original exige comparação atômica contra a base; se o
original mudou ou foi removido, existe uma cópia de resgate de texto e estilos,
sem âncoras ou vínculos. Descarte tem segunda confirmação e conserva a evidência.
O journal SQLite auxiliar é versionado, tem checksum e usa DELETE/EXTRA; não
altera o schema 15 do Projeto. Apenas o último checkpoint confirmado é
recuperável: texto ainda aguardando confirmação pode se perder. Falha do journal
é explicada e não impede a tentativa de salvamento normal. Não é garantia de
recuperação de cada tecla, nem de durabilidade do hardware em queda elétrica.

O grande corte anterior foi aprovado em 2026-10-03. Escala foi o incremento 1/2;
recuperação é o incremento 2/2, com revisão técnica consolidada e gate humano
pendente antes de outro incremento. A Etapa 1 não está encerrada; IME e sessão
longa continuam pendentes. Não há release, nova semântica automática ou Etapa 2.
Contratos: [escala](docs/editor-stage1-scale-2026-10-03.md) e
[recuperação](docs/editor-stage1-recovery-2026-10-03.md).

### Cartografia (C-01 + C-02)

Na aba **Cartografia**, crie um planeta, ajuste seed/água/relevo e gere uma
prévia. Aceite para salvar ou descarte sem alterar o Projeto. Arraste o mapa,
use a roda ou os botões de zoom e escolha uma camada. **Visual suave** usa o
melhor LOD que cabe no orçamento e interpolação bilinear; **Visual raster**
preserva a aparência das células quando ela for desejada. **Contorno de costa**
realça somente a borda terra–água e pode ser desligado. Selecione um Local do
Planejamento para definir coordenadas ou usar o modo de posicionamento por
clique. O modo de medição calcula distância sobre a esfera, não uma rota.
**Exportar SVG** salva a vista atual. A geração funciona offline e sem IA.

Para editar o relevo, escolha **Elevar terreno**, **Rebaixar terreno** ou
**Suavizar terreno**, ajuste raio/intensidade e clique no mapa. O clique cria
uma prévia: use **Aplicar relevo** para persistir ou **Descartar prévia** para
preservar o Projeto. Undo/redo de terreno cobre as últimas dez operações da
sessão. **Bloquear área do pincel** protege uma região nomeada e persistente;
não representa território nem permissão de acesso.

O terreno tem quatro níveis, até 1024×512 amostras; zoom só revela dados
existentes e não refina o planeta. Rios, biomas, importação de heightmap e
territórios/rotas temporais ainda não estão presentes. O esquema atual é
**v15**: faça cópia de seus Projetos antes de testar, e não abra cópias migradas
com executáveis antigos. Um AppImage anteriormente gerado precisa ser
reconstruído para incluir estas alterações. A validação humana de C-02
permanece pendente.

### Dicionário semântico no Editor (L-06 v0.9 + Onto.PT v2.2/v2.4 + promoção v2.5)

No painel **Ferramentas** de um Documento, **Consultar palavra selecionada**
abre a consulta lexical L-06 e apresenta todas as acepções encontradas sem
escolher uma interpretação. **Explorar construções e frames** navega o catálogo
L-08 promovido sem executar match sobre o texto. As duas operações são
explícitas, locais, somente leitura e não criam binding ou alteram o Documento.

No mesmo diálogo, a opção **Consultar somente a quarentena Onto.PT (isolada)**
troca deliberadamente a fonte da consulta. Os resultados recebem o rótulo
`QUARENTENA`, mostram registro/synset de origem e indicam se há sobreposição de
lema + classe com OpenWordNet-PT. Passar pelos critérios automáticos não é
aceite linguístico individual e a opção nunca mistura os resultados com o
catálogo principal.

A v1.2 mantém a rubrica de utilidade definicional e registra a revisão humana
da calibração: os 32 rascunhos aprovados aparecem como **DEFINIÇÃO EDITORIAL
APROVADA**, sempre com o original da fonte logo abaixo. As 8 abstenções não
foram aprovadas; o INDE conserva a definição original como fallback, rotulada
**DEFINIÇÃO ORIGINAL SINALIZADA**, com `motivo` visível e validação de relações
marcada como pendente. Pontuação automática e fallback não equivalem a aceite.

A v2.2 preserva integralmente essa camada histórica e acrescenta a
decomposição mensurável das definições multissentença. Ela expõe 3.607
acepções decompostas vinculadas, conserva 13.180 fragmentos sem vínculo fora da
consulta padrão e mantém 2.625 registros integrais com hífen incerto na fila de
revisão. Esses novos sentidos aparecem como **ACEPÇÃO DECOMPOSTA — REVISÃO
HUMANA PENDENTE**: a política de decomposição foi aprovada, mas isso não simula
aceite humano individual.

A camada derivada v2.3 separa definitivamente a unidade “palavra” da unidade
“acepção em revisão”. Cada resultado pode apresentar seu cluster provisoriamente
congelado, somente as evidências autorizadas daquele cluster, o mapa de cada
segmento da definição e os gates `LEX-DEF-001` a `LEX-DEF-004`. O redator não
recebe as acepções irmãs no mesmo contexto; conflito ou possível split devolve
o item ao alinhamento em vez de o corrigir durante a redação.

A v2.4 mantém essa camada como predecessor imutável e acrescenta atomicidade
da evidência. Registros com mais de uma unidade definicional recebem
`LEX-DEF-005` e não podem alimentar redação até classificação humana. Relações
de duplicação ou absorção formam famílias `LEX-DEF-006`, bloqueando todos os
membros simetricamente. A mesma grafia em classes diferentes compartilha apenas
um agrupador visual: comparações semânticas continuam restritas a forma + UPOS.

A v2.5 aplica a autorização humana de promoção sem modificar nenhum predecessor.
O lote aceito contém 86 definições: 75 colapsam candidatas literalmente
duplicadas e corroboradas em pelo menos dois synsets; as demais pertencem aos
casos humanos localizados `aba`, `ab-rogação` e `abade`. Colisão aproximada,
mesmo synset com redações distintas, sobreposição com L-06, fonte única não
revisada ou qualidade abaixo do limiar continuam bloqueados. Cada resultado
promovido aparece como **DEFINIÇÃO PROMOVIDA v2.5**, com pontuação,
proveniências e resolução de colisão. Promoção não cria binding semântico.

Os catálogos não são incorporados ao repositório público. O aplicativo procura
por padrão em `~/.local/share/inde/linguistic/` ou aceita caminhos explícitos:

```bash
export INDE_L06_CATALOG=/caminho/dictionary-l06-query.sqlite
export INDE_L08_CATALOG=/caminho/dictionary-l08-promoted.sqlite
export INDE_QUARANTINE_CATALOG=/caminho/dictionary-ontopt-v0.6-decomposition-quarantine-v2.2.sqlite
export INDE_DEFINITION_EVIDENCE_REVIEW=/caminho/definition-evidence-review-v2.4.sqlite
export INDE_DEFINITION_PROMOTION=/caminho/definition-candidate-promotion-v2.5.sqlite
./build/inde
```

Sem `INDE_QUARANTINE_CATALOG`, a quarentena é procurada separadamente em
`~/.local/share/inde/linguistic/quarantine/` (ou no equivalente sob
`$XDG_DATA_HOME`). O INDE procura primeiro a v2.2 e mantém a v1.2 como fallback
legado compatível.

O overlay v2.4 também permanece em `quarantine/`, separado do banco-fonte. Ele
é somente leitura, não promove dados e não substitui a v2.2 nem reescreve a
v2.3. Se ausente, o INDE procura a v2.3 legada; se ambas estiverem ausentes, a
consulta continua sem a camada de evidência. Overlay incompatível é rejeitado.

O catálogo v2.5 fica separado em
`~/.local/share/inde/linguistic/definition-candidate-promotion-v2.5.sqlite`.
Se estiver ausente ou incompatível, o L-06 continua disponível sem as novas
definições; o INDE nunca recria, corrige ou degrada esse arquivo em tempo de
consulta.

Metadados incompatíveis, catálogo ausente, rede exigida ou indicação de
binding automático fazem a integração falhar de forma segura. O contrato e os
limites estão em
[`docs/linguistic-editor-integration-v0.7.md`](docs/linguistic-editor-integration-v0.7.md).

O catálogo L-06 local v0.9 contém 9.855 formas e 13.776 acepções; todas as
acepções catalogadas possuem definição de origem não vazia. Os textos são
afirmações da fonte ainda marcadas `needs_review`, não revisão humana
individual. A v0.9 corrige o reconhecimento de identificadores RDF Unicode e
recupera 2.422 formas e 2.784 acepções definidas que a v0.8 não ligava ao Word
correspondente. O INDE preserva o conteúdo de origem sem inventar correções e
normaliza a chave de consulta em NFC + Unicode casefold, incluindo maiúsculas
acentuadas e formas Unicode decompostas.

A leva Onto.PT v0.6 em quarentena contém 960 sentidos definidos, relativos a
948 palavras distintas. A decisão humana aceita a aquisição seletiva e seu
teste isolado; os registros individuais permanecem `needs_review`. O catálogo
ativo v0.9 não é alterado, a aquisição integral continua não autorizada e toda
forma, definição e decisão de seleção conserva a procedência de origem.

Na primeira calibração editorial, as 960 acepções foram avaliadas: 667 são
candidatas a manter a definição da fonte e 293 exigem intervenção. O revisor
humano aprovou os 32 rascunhos preparados. As 8 abstenções foram deferidas, não
aceitas: primeiro se estabelece a acepção e uma definição útil; depois se
validam as relações. Na ausência de definição segura, permanece o original
raso com motivo visível. Nada foi promovido e todos os originais permanecem
imutáveis.

## Plataforma

O INDE é desenvolvido atualmente para ambientes Linux.

Dependências principais:

* compilador com suporte a C++20;
* CMake 3.22 ou superior;
* GTK 4.8 ou superior;
* gtkmm 4.8 ou superior;
* SQLite 3 com módulo R-tree (`ENABLE_RTREE`);
* libuuid;
* Enchant 2;
* dicionário Hunspell pt-BR.

### Ubuntu / Linux Lite

```bash
sudo apt update

sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  libgtkmm-4.0-dev \
  uuid-dev \
  libsqlite3-dev \
  libenchant-2-2 \
  hunspell-pt-br
```

## Compilação

Clone o repositório:

```bash
git clone https://github.com/Roger-Cardoso/inde.git
cd inde
```

Configure o projeto:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

Compile:

```bash
cmake --build build
```

Execute os testes:

```bash
ctest --test-dir build --output-on-failure
```

Execute o programa:

```bash
./build/inde
```

Também é possível abrir diretamente um projeto:

```bash
./build/inde "/caminho/Projeto.inde"
```

## AppImage para Linux x86_64

As versões distribuídas em AppImage reúnem o executável, o runtime GTK 4 e a
revisão ortográfica pt-BR num único arquivo. Depois de baixar o AppImage e o
arquivo `.sha256` correspondente em
[Releases](https://github.com/Roger-Cardoso/inde/releases):

```bash
sha256sum --check INDE-0.1.0-x86_64.AppImage.sha256
chmod +x INDE-0.1.0-x86_64.AppImage
./INDE-0.1.0-x86_64.AppImage
```

Um projeto também pode ser aberto diretamente:

```bash
./INDE-0.1.0-x86_64.AppImage "/caminho/Projeto.inde"
```

Se a distribuição não oferecer FUSE, use o modo de extração temporária:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./INDE-0.1.0-x86_64.AppImage
```

A receita local, as dependências, o fluxo de publicação e a matriz de validação
estão documentados em [`packaging/README.md`](packaging/README.md).

## Formato de projeto `.inde`

Um projeto INDE é armazenado como um diretório-contêiner com extensão `.inde`.

Estrutura básica:

```text
Projeto.inde/
├── manifest.json
├── assets/
├── data/
│   └── project.sqlite3
└── documents/
```

O `manifest.json` contém informações fundamentais do projeto.

O banco `project.sqlite3` é a principal fonte operacional dos dados estruturados nas versões atuais do formato.

A estrutura em diretório permite que o projeto permaneça autocontido e possa ser copiado ou armazenado como uma unidade.

## Arquitetura

O código é organizado principalmente em quatro áreas:

```text
include/inde/ + src/
├── project/       domínio e estruturas fundamentais
├── application/   serviços e casos de uso
├── persistence/   armazenamento e SQLite
└── ui/            interface gráfica GTK
```

A lógica central do programa é mantida separada da interface gráfica sempre que possível, permitindo que operações de domínio e persistência sejam testadas sem inicializar GTK.

## Persistência

A persistência principal utiliza SQLite.

O projeto implementa, entre outros mecanismos:

* chaves estrangeiras;
* migração versionada de esquema;
* transações;
* rollback de operações;
* statements preparados;
* validações de integridade;
* identificadores persistentes;
* operações de cópia de projeto com nova identidade.

O formato do banco pode evoluir enquanto o programa estiver em desenvolvimento.

## Testes e benchmarks

O projeto possui uma suíte de testes para o núcleo e programas auxiliares de benchmark.

Após a compilação:

```bash
ctest --test-dir build --output-on-failure
```

Alguns executáveis adicionais podem ser gerados quando `BUILD_TESTING` está habilitado, incluindo benchmarks relacionados ao planejamento narrativo, timeline, escrita, workspace e revisão textual.

## Projetos de demonstração

O diretório `examples/` contém projetos sintéticos criados especificamente para demonstração e testes do INDE.

Eles não contêm obras literárias de terceiros.

Um exemplo pode ser aberto diretamente:

```bash
./build/inde "examples/Atlas de Véspera — Projeto Integral.inde"
```

## Dados do usuário

O INDE funciona localmente e os projetos permanecem armazenados no computador do usuário.

O programa não depende de uma conta online para utilizar o formato de projeto e seus recursos locais atuais.

Recomenda-se manter cópias de segurança regulares de projetos importantes.

## Licença

O código original do INDE é distribuído sob a **INDE Non-Commercial Educational License 1.0**.

Consulte:

* `LICENSE`
* `THIRD_PARTY_NOTICES.md`

A licença permite determinados usos pessoais, estudantis, educacionais e não comerciais do código, conforme seus termos.

Obras, textos, documentos e demais conteúdos produzidos pelos usuários utilizando uma versão autorizada do INDE permanecem propriedade de seus respectivos autores. A licença do programa não reivindica propriedade ou royalties sobre esses conteúdos.

Componentes de terceiros utilizados pelo projeto permanecem sujeitos às suas próprias licenças.

## Autor

**Roger Cardoso**

INDE — Integrated Narrative Development Environment

https://github.com/Roger-Cardoso/inde
