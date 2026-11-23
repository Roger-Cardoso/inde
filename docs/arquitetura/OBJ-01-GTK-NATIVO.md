# OBJ-01 — correção da interface para C++/GTK4

Registro histórico de 22/11. A integração ativa no editor de Escrita está descrita em [Escrita integrada](OBJ-01-ESCRITA-INTEGRADA.md).

Data: 2026-11-22. Base da correção: `47328b9fc90a03beff3a640d9ec7b90ccaf62016`, branch `feature/inde-obj-01-foundation`.

## Correção do requisito

O usuário esclareceu que a implementação deveria usar a linguagem e a interface nativas do INDE. A entrega anterior usava um conteúdo HTML/JavaScript dentro de uma janela GTK por WebKit; essa classificação como integração nativa foi inadequada ao requisito. Este corte substitui a interface por widgets C++/GTK4 e desenho Cairo/Pango. Os resultados anteriores continuam históricos e não validam a nova superfície.

O motor de documento, cálculo, cartões, composição, identidade, histórico e persistência permanece o mesmo C++ validado. A correção não redefine regras, codecs ou formatos de projeto. A entrada continua sendo a ação `win.document-objects` do INDE, com o Documento real selecionado na Escrita e a captura de origem preservada.

## Interface

`ObjectDocumentWindow` contém `Gtk::Paned`, `Gtk::Stack`, caixas/rolagem, botões, listas, seletores, campos de texto e `Gtk::TextView`. As fórmulas são editadas por cartões GTK ou escrita livre; as frases misturam trechos literais com resultados ligados. Células, aparência de tabela, eixos recuperáveis, composição e retirada/restauração usam operações do mesmo motor.

As solicitações são chamadas C++ em processo; a representação JSON é o codec de domínio existente, sem transporte web ou avaliação de JavaScript. Um `GTask` executa a operação de domínio/persistência sob mutex. Só a resposta atualiza os widgets na thread principal; um marcador de vida impede callbacks de acessar uma janela destruída. Há no máximo uma solicitação em voo pela interface.

Os rascunhos são campos GTK em memória, com aplicação/cancelamento explícitos. Fechamento e Salvar projeto consultam diretamente o estado nativo de rascunho, tarefa e seletor de arquivo. Não existe a antiga corrida de notificação JavaScript. Campos não aplicados não recebem promessa de recuperação após queda do processo.

`FractionRow` é um widget GTK registrado que mede e aloca filhos proporcionais com altura por largura. É usado nos arranjos e colunas de tabelas. Desparentaliza seus filhos durante `dispose` e mantém destruição idempotente. Os alinhamentos de domínio `start/center/end` são traduzidos para GTK; não são confundidos com `left/right` do alinhamento textual nem `top/middle/bottom` das células.

## Matemática em C++

O núcleo [MicroTeX](https://github.com/NanoMichael/MicroTeX), com revisão fixa e proveniência local, compõe as expressões. A ponte original Cairo foi portada de cairomm/GTK3 para cairomm 1.16/pangomm 2.48/GTK4. `MathPreview` é um `Gtk::DrawingArea` que desenha o resultado com Cairo/Pango e acompanha a cor do tema. Os modos de bloco/texto usam estilos matemáticos distintos.

O interpretador é precedido por uma fronteira léxica fechada: UTF-8/tamanho, número de passos, profundidade de chaves, número de células, ambientes e comandos permitidos. Definição de macros, leitura de arquivos, imagens e links externos não são aceitos na prévia. Os comandos suportados não executam TeX ou shell. Uma fonte recusada pela prévia continua sendo dado autoral preservado pelo motor e pode ser salva.

O material original, fontes e licenças estão em `cpp/vendor/`. Um ajuste documentado em `latex.cpp` troca a montagem do caminho HOME por `std::string`, evitando `asprintf` sem conferência e liberação incompatível. A auditoria verifica os bytes importados contra seus hashes e o hash explícito dessa alteração. A licença do núcleo é MIT; as licenças próprias das fontes também foram conservadas.

A biblioteca importada é compilada com seu perfil de avisos. O código próprio do INDE — interface, ponte de domínio e wrapper matemático — conserva os avisos estritos tratados como erro. Isso é uma separação explícita entre terceiros e código próprio, sem afirmar que o código importado passou pela política estrita do INDE.

## Compatibilidade e fronteiras

Não há dependência de WebKit/JavaScriptCore no executável para esta integração, instalação de HTML/JS como editor ou alteração de política AppArmor para executá-lo. O piloto web permanece separado como laboratório. O build do aplicativo instala apenas os recursos matemáticos e licenças nativos necessários.

Snapshot privado V5, banco auxiliar versão 5 e proprietário schema 37 permanecem. O corpo canônico, seus grupos/camadas e a origem são preservados; a reconciliação quando ele muda na Escrita continua pendente. Publicação dos objetos, entidades/pseudônimos, edição inline unificada, numeração e serviço comum com Comic não foram promovidos a concluídos por essa troca de tecnologia.

Veja o [roteiro ativo](../../tools/obj01-pilot/NATIVO.md) e a [validação específica](../validacao/VALIDACAO-OBJ-01-GTK.md). A amostra fica em `dados/obj01-gtk-nativo-final/Projeto.inde`, separada das anteriores. Main e projetos autorais permanecem fora do corte.
