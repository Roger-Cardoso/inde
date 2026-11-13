# INDE — Integrated Narrative Development Environment

O INDE é uma oficina narrativa local para Linux, desenvolvida em C++20, GTK4
e SQLite. Reúne escrita, planejamento, organização do manuscrito, revisão,
tempo ficcional, cartografia e produção de saídas.

Este repositório contém o snapshot da reforma aceito pelo usuário em
**12 de outubro de 2026**, com **schema 28**. O programa continua em
desenvolvimento; a publicação não significa que toda a reforma esteja concluída.

## Começar sem burocracia

Crie um Projeto e um Documento livre para começar a escrever. Não é necessário
cadastrar previamente uma Obra ou um Universo. Recursos que dependem desses
contextos oferecem criação e orientação contextual, sem associar o texto
silenciosamente.

A interface utiliza ferramentas progressivas, paleta de comandos e temas
claro, escuro e sistema. O laboratório de blocos é um executável separado.

## Áreas de trabalho

- **Escrever:** documentos formatados, busca/substituição, comentários,
  marcações, tempo e perspectiva por trecho, consulta explícita ao dicionário,
  vínculos e sugestões confirmados pelo autor.
- **Planejar:** planos e modelos narrativos, linhas, unidades, personagens,
  elementos, relações e realização textual conectados, com alternativas e
  avisos consultivos de continuidade.
- **Revisar:** sugestões explícitas, decisões e comparação de versões.
- **Manuscrito e Montagens:** organização editorial e composição persistente,
  com rascunhos, versões, usos vivos/fixados e revisão de impacto.
- **Cronologia:** sistemas de tempo, calendários históricos/atuais/fictícios,
  relógios, adoções/sucessões e seleção gráfica adequada ao calendário escolhido.
- **Mapas:** geração e edição de terreno com prévia, locais, câmera, medição
  esférica e exportação de vista.
- **Catálogo e Projeto:** organização, espaços de trabalho salvos, histórico
  de operações e auxiliar de exclusão que indica onde um item é usado.
- **Produção:** saídas DOCX, EPUB, PDF, HTML e TXT, com escolha explícita das
  fontes e relatório de omissões, perdas e hashes.

Ordem narrativa, cronológica, de escrita e de apresentação editorial são
representações distintas. O programa não transforma uma delas na outra nem
escolhe automaticamente interpretações, vínculos semânticos ou conteúdo final.

## Teclado

| Combinação | Ação |
|---|---|
| Ctrl + Shift + P | Paleta de comandos |
| F1 | Ajuda contextual |
| Ctrl + F1 | Consulta e personalização de atalhos |
| Ctrl + 1 até Ctrl + 7 | Áreas principais |
| Ctrl + Shift + 8 | Montagens |
| Ctrl + Shift + 9 | Espaços de trabalho salvos |
| Ctrl + Shift + N | Documento livre |
| F6 / Shift + F6 | Navegação por regiões |
| Ctrl + Shift + D | Dicionário para o termo selecionado |
| Ctrl + Shift + K | Comandos inline |
| Ctrl + W / Ctrl + Shift + W | Sair da vista / fechar o projeto |

Há 208 comandos explícitos no principal e 37 no laboratório; não são todos
atalhos globais. Campos de texto, IME, menus e modais têm prioridade contextual.
As preferências de teclado pertencem ao perfil, não ao banco do Projeto.
Consulte [contrato de teclado](docs/keyboard-workflow-2026-10-12.md).

## Compilar

Dependências: CMake 3.22+, compilador C++20, GTKmm 4.8+, GLib, UUID, SQLite com
ENABLE_SESSION e ENABLE_PREUPDATE_HOOK, Pango/Cairo e ICU 74+. A revisão
ortográfica utiliza Enchant/Hunspell e dicionário pt-BR. Um ambiente compatível
é Ubuntu 24.04; as bibliotecas de outros sistemas devem satisfazer os mínimos.

```bash
sudo apt-get install build-essential cmake ninja-build pkg-config \
  libgtkmm-4.0-dev libsqlite3-dev uuid-dev libicu-dev libhunspell-dev \
  libenchant-2-2 hunspell-pt-br xauth xvfb
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/inde
```

Para produzir um projeto demonstrativo sintético em uma pasta nova:

```bash
./build/inde_demo_project_generator "/caminho/novo/Atlas.inde"
./build/inde "/caminho/novo/Atlas.inde"
```

Não use um projeto autoral como alvo de geradores ou testes. Não foram incluídos
bancos de projetos na publicação. Catálogos linguísticos externos precisam ser
fornecidos explicitamente e ser compatíveis; sua ausência não autoriza o
programa a inventar resultados. Eles não são distribuídos neste snapshot.

## Verificação e limites

O candidato aprovado passou por 35/35 testes CTest, 3.334 casos de política de
teclado e 16 cenários GTK em Projeto vazio/Atlas e temas claro/escuro, além de
regressões selecionadas, startups limitados e inspeção visual.

Debug e ASan/UBSan são **focais**, não instrumentação integral da aplicação;
LSan estava desativado. Esses ensaios não certificam teclado ABNT2 físico,
IME real, Wayland, leitor de tela, HiDPI ou sessões longas. O aceite humano é
um registro separado, não uma ampliação artificial da cobertura dos testes.
Veja [identidade, publicação e evidências](docs/publication-2026-10-12.md).

A receita AppImage permanece em `packaging/`, mas este snapshot de código
**não declara um novo AppImage testado nem uma release binária certificada**.
O workflow de empacotamento é acionado manualmente ou por tags de versão;
compilação/testes têm workflow próprio. A versão CMake histórica 0.1.0 não
substitui a identidade do commit e do corte datado.

## Documentação e histórico

[Contexto atual](docs/PROJECT-CONTEXT.md) identifica o estado vigente.
Os contratos datados em `docs/` preservam as etapas anteriores, seus schemas,
evidências e limites no momento de emissão. Uma pendência histórica pode ter
sido sucedida por um aceite posterior; não reescrevemos esse passado.
O [README anterior](docs/README-historico-2026-10-04.md) é uma fonte histórica,
não uma descrição atual da interface.

Conversas integrais, material autoral e o acervo cronológico privado permanecem
locais por decisão do usuário e não são parte deste repositório público.

## Licença

Consulte [LICENSE](LICENSE) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
A publicação não altera a licença existente nem relicencia materiais de terceiros.
