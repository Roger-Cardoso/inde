# Validação · COMIC-01 / laboratório 0.7

02/11/2026. Linux, C++20/GTKmm 4.10, Python 3.12.3, Chromium. Dados exclusivamente sintéticos. Este registro comprova a primeira integração técnica e não equivale a aceite humano, migração integral do estúdio 0.4, teste prolongado ou release pública.

## Base e isolamento

`verificar_base.py` confirmou os **641 arquivos** da fonte congelada do INDE, sem alterações, no commit `89b47f27e83d76e1ef61a46cb6c29b8c75c305c4`. O arquivo de origem conserva SHA-256 `3d6bee4c60dbecc602a56881b1957f90b55798dbc711c9c290701e2ad8a6e97e`.

A integração foi feita no overlay C++ do laboratório. O banco autoral permanece schema 28 e a extensão de publicação, schema 3. A extensão comic usa schema 1. O site autoral do Literature/UniLit e projetos autorais existentes não foram usados como material de teste nem convertidos.

## Testes automatizados

A suíte completa de publicação passou: **86 testes Python, sem skips**, incluindo os 76 testes anteriores e 10 novos casos de integração de comics. A suíte nova chama o executável C++ real, que passou em **39 verificações nativas** de modelo, persistência, revisões, recursos, preparação e cópia de Projeto. Essas verificações são internas ao percurso chamado pela suíte; não foram somadas como se fossem outros 39 testes Python.

Foram conferidos:

- round trip do modelo tipado; rejeição de geometria inválida/NaN, idioma inseguro, IDs repetidos, recurso ausente e vínculo com Obra externa;
- consulta sem criar banco; salvamento, conflito de revisão, retirada/restauração e reabertura;
- cópia explícita de Documento com ID/hash de origem e conservação do banco autoral;
- imagem PNG real e importação de arquivo TTF real disponível no sistema; recursos incorporados e exportados;
- preparação vinculada às revisões do Site e do comic; invalidação após alterações; rejeição de retrato adulterado e materialização repetida;
- adoção da extensão em Salvar cópia, invalidação das preparações e conservação da origem;
- integridade SQLite e histórico de estados;
- HTML escapado, idioma/direção/tradução e legenda da onomatopeia; texto contínuo sem scripts;
- despacho do novo protocolo pela mesma entrada de publicação; rejeição de versão/mídia incompatível, caminhos inseguros e base64 inválido;
- links relativos sob `/biblioteca`, CNAME para domínio próprio, ausência de CNAME para github.io, manifesto de hashes e ZIP íntegro;
- recusa de sobrescrita de geração anterior e exclusão de SQLite/proveniência privada do pacote público;
- regressão do catálogo de novels, importação, identidades públicas, ambiente do navegador e reutilização da prévia.

A execução inicial restrita falhou no teste anterior que abre uma porta de prévia; a execução com permissão para o serviço local passou integralmente. Isso foi tratado como condição do ambiente, sem remover o teste.

## Percurso GTK real

O teste `--comic01-smoke` abriu o aplicativo INDE completo, revelou um Documento e guardou seu buffer, texto, seleção/estado de visualização e hash do banco autoral. Pela nova aba Comics, exercitou:

1. criação de uma amostra nativa e bloqueio de saída com alterações pendentes;
2. salvamento, edição de fala e geometria, e novo salvamento;
3. remoção de elemento e descarte, recuperando o estado salvo;
4. cópia explícita de Documento, com origem registrada, e salvamento;
5. preparação assíncrona pelo novo protocolo e materialização do site/ZIP;
6. retorno ao INDE, conferindo a mesma identidade de Documento, o mesmo buffer, texto, seleção/estado e hash do banco autoral.

Resultado: **aprovado**. A abertura inicial da amostra também carregou as imagens raster incorporadas pelo gerador nativo de demo. A conferência corrigiu o uso do MIME no carregador de imagem e o caminho dos recursos de leitura antes da conclusão.

Também passou o percurso GTK anterior `--pub02-smoke`: bloqueio com escrita pendente sem salvamento implícito, CRUD de Site/Edição, geração de novel, retorno à mesma sessão, seleção e Desfazer. O aviso de escolha do renderer Cairo nesta máquina é o comportamento de compatibilidade já documentado.

O seletor nativo de arquivos foi implementado, mas não se afirma ter exercitado todas as interações manuais de importação de imagem/fonte. A importação real de recursos foi conferida no núcleo e o carregamento de arte, no GTK.

## Leitor no navegador

A demonstração ilustrada foi criada por `inde_comic_demo`, salva no Projeto, preparada pelo serviço C++ e materializada pela publicação existente. Foi servida pelo mecanismo de prévia desse laboratório, sem o estúdio Python 0.4.

No Chromium foram observados:

- biblioteca com nome, descrição e autoria do Site e link para a comic;
- três quadros com arte raster e texto HTML;
- navegação quadro a quadro até 3/3;
- fala árabe com `lang=ar`, `dir=auto` e direção calculada RTL;
- VUUUM e a legenda “Som grave do portal.” como texto visível;
- seleção do modo Celular, conservando os três textos e a legenda;
- abertura de Texto contínuo, contendo os três quadros e **zero scripts**.

Não foi executado motor nativo de tradução. Marcação de idioma e permissão de tradução não certificam o resultado de tradução de uma grafia sonora. A demonstração ilustrada não leva uma coleção de fontes universal; o texto árabe no navegador usou os recursos disponíveis no computador de teste.

## Reproduzir

```sh
bash Preparar-INDE-especial.sh
python3 -m unittest discover -s tests -v
python3 verificar_base.py
bash scripts/demos/Abrir-demo-comic.sh
```

Para o teste C++ isolado, forneça uma pasta que ainda não exista:

```sh
build-nativo/inde_comic_tests /tmp/inde-comic-fixture-nova recursos
```

O roteiro visual está em ROTEIRO-COMIC.md. Os testes GTK exigem sessão gráfica e uma fixture sintética nova, criada com `inde_web_bridge --demo`, acrescida do Site de demonstração com `inde_comic_demo`. Não execute probes sobre um Projeto autoral.

## Evidências e limites

Neste workspace, `evidencias/comic01/` contém `comic-nativo.png`, `editor-retorno-comic01.png`, `regressao-novel.png` e `leitor-comic.png`. São capturas do software executado, não mockups. Os caminhos das gerações e o controle temporário de prévia são registros locais e ficam fora do pacote de fontes.

O pacote 0.7 contém código, fonte congelada, recursos, testes, instruções e as capturas selecionadas; o binário já compilado fica neste workspace. Outro computador compatível precisa compilar. Instalador autossuficiente, Windows, dispositivos físicos, acessibilidade assistiva, todos os formatos de fonte e uso prolongado não foram validados.

A lista do que ainda não foi portado e a sequência para catálogo misto estão em CONTRATO-COMIC-01.md. A interface de autoria usa texto comum e a referência tipográfica é a prévia web. Não há importação do arquivo 0.4 nem equivalência declarada de caudas, conexões, contornos, montagem livre e diagnóstico de glifos.

Não houve envio a GitHub, implantação remota, ativação de comentários ou alteração de credenciais.
