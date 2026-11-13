# Validação · COMIC-02 / INDE 0.8

02/11/2026. Linux, C++20/GTKmm 4.10, Python 3.12.3, sessão gráfica GTK com Cairo. Dados sintéticos. Resultado técnico; aceite humano e equivalência integral do estúdio 0.4 não são inferidos.

## Resultado

**86 testes Python aprovados, sem skips.** Mantêm a cobertura de publicação, catálogo, importação, revisões, fontes/recursos, HTML, exportação e prévia. Os dez casos de comics continuam executando as **39 verificações C++** de persistência, conflitos, recuperação, cópia e preparação. Essas verificações integram o percurso da suíte e não são outros 39 testes Python.

**Dois percursos do aplicativo GTK completo aprovados**, em Projetos sintéticos separados:

### Espaço Comic

- A barra foi lida da árvore de widgets e comparada, na ordem, com Criar, Planejar, Revisar, Manuscrito, Cronologia, Mapas e Obras. Os seis botões diretos também foram acionados e seus destinos conferidos.
- O menu Criar contém Escrita e Comic; suas escolhas foram acionadas pela interface e conferidas contra o espaço ativo.
- Comic foi confirmado como conteúdo principal do sistema de espaços, mapeado e com largura própria, sem diálogo modal.
- Criação e salvamento de comic, edição, histórico de texto, Desfazer/Refazer e preservação de bytes de imagem ao desfazer/refazer recursos.
- Rascunho e zoom preservados ao alternar Escrita/Comic e ao retomar contexto salvo.
- Proteção durante composição provisória de entrada e encerramento assíncrono pelo despacho do GTK. O teste emite o sinal público de preedição; não equivale a uma matriz real de métodos de entrada.
- Fechamento recusado com rascunho pendente; salvamento, remoção e desfazer, cópia explícita de Documento e preparação assíncrona de publicação.
- Painel geral de Publicação conferido com três abas, sem o editor de comics.
- Retorno a Comic e a Escrita, com a mesma identidade de Documento, buffer, texto, seleção/estado e hash do banco autoral.
- Captura visual inspecionada: sequência à esquerda, área de composição ampla e inspetor lateral com largura inicial reduzida.

### Regressão de Escrita e publicação de novel

O percurso anterior PUB-02 passou com a barra nova e o espaço Comic presente: bloqueio com escrita pendente sem salvamento implícito pelo painel, CRUD de Site/Edição, geração, retorno ao mesmo editor, seleção e Desfazer.

## Base e dados

O verificador confirmou os **641 arquivos congelados da base**, sem alterações, no commit de referência `89b47f27e83d76e1ef61a46cb6c29b8c75c305c4`. A integração usa fontes C++ da interface atual e a camada de integração já existente neste projeto; o resultado executado é o próprio aplicativo GTK.

O banco autoral permanece schema 28, publicação schema 3 e comics schema 1. Os comics da entrega anterior continuam no mesmo formato. Nenhuma obra autoral foi usada para teste. A demonstração nova usa pasta e perfil próprios, sem reescrever a amostra anterior.

## Reproduzir

```sh
bash Preparar-INDE-especial.sh
python3 -m unittest discover -s tests -v
python3 verificar_base.py
python3 tests/verificar_workspace_comic.py /tmp/inde-comic-verificacao-nova
bash scripts/demos/Abrir-demo-comic.sh
```

A pasta de verificação GTK deve ser nova. Ela recebe apenas fixtures sintéticas, logs e capturas. O teste gráfico exige uma sessão gráfica; o teste de prévia precisa abrir uma porta local.

## Evidências

Neste workspace, em `evidencias/comic02/`:

- `workspace-comic.png`: espaço Comic com arte, elementos e a nova barra.
- `escrita-retorno-comic02.png`: retorno ao editor existente.
- `workspace-novel.png`: regressão do painel de publicação.
- `resultado.json`, `comic.log` e `novel.log`: resultados dos percursos GTK.
- `regressao-python.log`: saída da suíte de 86 testes.
- `base-verificada.json`: verificação da fonte congelada.

As capturas são do aplicativo executado. O pacote de fontes leva somente as capturas selecionadas, código, recursos, testes e documentação; os bancos, perfis e dados de trabalho ficam fora.

## Limites

Esta entrega conclui a reorganização da navegação e a integração do espaço de trabalho. O inventário dos recursos restantes para equivalência integral do estúdio está em CONTRATO-COMIC-02.md: caudas/conexões, montagem livre, texto rico internacional, diagnóstico de glifos, cálculo por resoluções, importação 0.4 e catálogo público misto.

A área GTK usa fontes instaladas; fontes incorporadas e o resultado final de leitura devem ser conferidos na prévia web. A tradução nativa do navegador não foi exercitada aqui. Não houve validação em outros sistemas, aparelhos físicos, leitores de tela, todos os métodos de entrada ou uso prolongado. Alterações da composição são salvas explicitamente; a recuperação automática de rascunho após falha ainda integra a migração restante.

Não houve envio remoto, implantação, alteração de credenciais ou aceite humano presumido.
