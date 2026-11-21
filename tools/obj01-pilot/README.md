# INDE OBJ-01 — P2-A: texto e PNG reais

Demonstração local em navegador, controlada por um processo C++20 com o motor documental e os comandos OBJ-01 reais. Python serve os arquivos da interface e encaminha pedidos; não define identidades, ordenação, conteúdo nem gravação. Não usa GTK nem integra o aplicativo principal.

O perfil P2-A abaixo é conservado. Para tabela editável e histórico vivo em dados separados, veja [P2-B](P2B.md).

## Abrir

Na raiz da **branch de trabalho** `feature/inde-obj-01-foundation`:

```sh
bash scripts/demos/Abrir-demo-obj01-p2a.sh
```

Abra no navegador o endereço completo que aparecer após `Demo P2-A:`. A parte depois de `#` permite acesso à sessão local e é retirada da barra depois da abertura. O servidor escuta somente em `127.0.0.1`. Encerre com Ctrl+C depois de salvar.

O iniciador compila o alvo isolado e conserva os dados sintéticos em `dados/obj01-piloto-p2a/`, ignorado pelo Git. Reexecutar o iniciador reabre o estado salvo. Uma segunda sessão sobre a mesma pasta é recusada.

Dependências de compilação: CMake ≥ 3.22, Ninja, compilador C++20, Python 3, GLib, UUID, GdkPixbuf, SQLite3 e zlib. Em Ubuntu/Debian, os pacotes de desenvolvimento usados na CI são `libglib2.0-dev uuid-dev libgdk-pixbuf-2.0-dev libsqlite3-dev zlib1g-dev ninja-build`.

Para uma pasta experimental diferente, nova ou já reconhecida pelo piloto:

```sh
bash scripts/demos/Abrir-demo-obj01-p2a.sh --workspace /tmp/meu-experimento-obj01
```

Sem o iniciador, `python3 tools/obj01-pilot/serve.py` usa uma pasta temporária nova e informa seu caminho. Pastas temporárias podem desaparecer na limpeza/reinício do sistema. Não use pastas de obras: o núcleo recusa caminhos `.inde`, pastas novas não vazias, links no arquivo de banco e bancos sem o identificador do piloto.

## Percurso de cinco minutos

1. Confira os três parágrafos de origem. O texto fica em leitura neste corte.
2. Clique **Usar figura de exemplo**, confira legenda/descrição e escolha **O horizonte abre uma passagem.** em **Inserir antes de**. Clique **Inserir figura**. Também pode escolher um PNG estático de até 8 MiB pelo seletor de arquivo.
3. Com a figura selecionada, escolha o parágrafo vizinho **O horizonte abre uma passagem.** e clique **Mostrar lado a lado**. A proporção pode ser escolhida antes de aplicar o arranjo.
4. Clique **Salvar documento**, depois **Reabrir salvo**. A imagem vem do banco experimental, independentemente do arquivo de importação.
5. Clique **Criar outro uso da mesma figura** com destino no fim. Mova só esse uso para antes de **Água, luz e memória.**: o outro uso e o recurso continuam iguais.
6. Em **O que muda**, escolha **Somente a apresentação** e mova um uso. Alterne entre **Apresentação** e **Ordem de leitura** para ver a diferença. Antes de mudar novamente a sequência autoral, use **Restaurar apresentação pela leitura**.
7. Salve, encerre o servidor e execute novamente o iniciador com a mesma pasta. Confira as figuras, suas posições e os arranjos.

Cada item pode ser selecionado por teclado, com Tab e Enter/Espaço. Os controles de movimentação substituem arraste neste corte. Mover uma figura para a coluna principal desfaz apenas o Row do qual esse uso participava. Restaurar a apresentação pela leitura remove os Rows e reconstrói a coluna principal explicitamente.

## O que é real e o que fica pendente

São reais: captura integral de um `project::Document` sintético compatível com os codecs schema 37; IDs/bindings; PNG original, CRC/hash/dimensões/decodificação; recursos compartilhados; sequências autoral/visual; Row derivado em grafo validado; seleção; comandos de reordenação; transação SQLite com comparação de revisão/hash e readback; reabertura por processo novo.

A fixture é um Documento proprietário com IDs, formatação, âncora, resma de dois parágrafos, folha coberta e perfil de visibilidade. **Não é um projeto SQLite schema 37 completo.** A folha coberta é conservada no snapshot privado e não enviada à interface. O corpo ativo aparece integralmente; o perfil de visibilidade não filtra essa vista autoral. A apresentação do texto é simples e não pretende renderizar todas as camadas de formatação.

Ainda não há edição do texto de origem, edição posterior de legenda/conteúdo compartilhado, remoção de ocorrência, undo/redo do agregado, tabela, equação, arraste/resize/snap, integração Comic, exportação ou publicação. **Trabalho não salvo vive apenas na memória do processo C++.** Fechar/reiniciar esse processo perde esse trabalho; P13 e a recuperação multimídia ficam para P2-C. Reabrir o estado salvo com trabalho pendente pede confirmação de descarte.

O banco `obj01-piloto.sqlite3` é um formato experimental próprio, identificado por `application_id=0x4f424a31`, `user_version=1` e snapshot `INDE-OBJ01-COMPOSITE-P2A-1`. Não é o futuro arquivo único `.inde`; não existe migrador de projetos reais. Os blobs e a referência salva entram na mesma transação; nenhum checkpoint textual é apresentado como recuperação desse agregado.

O piloto usa um adaptador SQLite pequeno e local. Os wrappers canônicos atuais ligam `SqliteTransaction` a `OperationCapture`, histórico de operações e dependências de remoção/migração. Essas dependências não são ligadas ou simuladas no alvo isolado. O adaptador não é um novo serviço universal.

## Perfil de admissão e falhas

- PNG estático: até 8 MiB, cada lado até 8.192 pixels e até 16.777.216 pixels decodificados. PNG animado, CRC inválido, truncamento ou dados após IEND são recusados.
- Até 32 MiB de blobs distintos, 64 blobs, 256 usos de figura e 128 Rows. Textos de legenda/descrição até 4.096 **bytes UTF-8**, com orçamento agregado de strings de 8 MiB no piloto.
- Row com um parágrafo e uma figura adjacentes na apresentação; frações positivas somando 1, tolerância 1e-9; gap lógico entre 0 e 256. A interface usa gap 20. Frações são preservadas com precisão de roundtrip.
- Não se insere figura dentro de um grupo textual protegido da sequência autoral. Movimento autoral exige alinhamento prévio; não reordena parágrafos neste corte.
- Versão desconhecida, origem alterada, blob ausente/corrompido e conflito de revisão recusam a operação. Recusas conservam o trabalho em memória; o save usa transação e rollback. Falha de energia e todos os erros possíveis de sistema não foram demonstrados por esses testes.
- Não há autenticação contra outro processo do mesmo usuário nem publicação do servidor para a rede. Não atribuir essa garantia ao token de sessão.

## Testar

```sh
cmake -S tests/object_composite_pilot -B build-obj01-p2a -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-obj01-p2a --parallel 2
ctest --test-dir build-obj01-p2a --output-on-failure
```

O alvo configura a ponte e os codecs proprietários isolados, além dos testes do processo C++ e do controlador HTTP real. Os testes HTTP precisam de permissão do ambiente para abrir loopback. A CI repete a suíte em GCC/Clang, com e sem ASan/UBSan, incluindo LeakSanitizer no ambiente remoto. Não confundir esses testes com aceite humano ou conclusão da Fase 2 inteira.
