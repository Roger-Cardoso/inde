# OBJ-01 — validação dos cartões preenchíveis

Data: 2026-11-22. Base `8ad48d13b32d87fdeb493877e4fdf77c8ae3fb22`; implementação `972cc6a8e7f4a12be3db77e3ec97203ad526a2e4`; branch `feature/inde-obj-01-foundation`. Ver [contrato](../arquitetura/CARTOES-FORMULAS-OBJ-01.md), [roteiro](../../tools/obj01-pilot/CARTOES.md), [resultados estruturados](evidencias-obj01-cartoes/resultados.json) e [SHA256SUMS](evidencias-obj01-cartoes/SHA256SUMS).

## Comportamento entregue

Cartões de condições no piloto real: entrada numérica/textual ou referência escolhida a variável/resultado; nome da entrada e frases preenchíveis; adicionar, remover, subir, descer e alterar valores; primeira condição atendida; alternativa; prévia no motor C++; aplicação de fonte + frase atômica; edição avançada conservada. Documento e célula usam o mesmo modelo.

A prévia não grava nem executa comandos. Cartões são reconstruídos da fonte; não há modelo de regras persistido em paralelo. Formato auxiliar 5 e linguagem 2 mantidos, sem schema novo ou migração. Dados antigos não são atualizados no lugar.

## Matriz local final

| Configuração | Resultado | Tempo | Log |
| --- | --- | --- | --- |
| GCC Debug | 13/13 programas | 111,19 s | [GCC](evidencias-obj01-cartoes/gcc-final.txt) |
| Clang Debug | 13/13 programas | 92,11 s | [Clang](evidencias-obj01-cartoes/clang-final.txt) |
| GCC ASan + UBSan | 13/13 programas | 238,24 s | [GCC sanitizers](evidencias-obj01-cartoes/asan-final.txt) |
| Clang ASan + UBSan | 13/13 programas | 190,73 s | [Clang sanitizers](evidencias-obj01-cartoes/clang-asan-final.txt) |

São os 11 programas anteriores mais a bateria de cartões (15 casos de núcleo) e HTTP (sete casos: quatro comuns herdados e três específicos). As baterias anteriores A/B/C/D/D2 e adaptador da origem também passam. Localmente LeakSanitizer fica desativado; a matriz GitHub habilita detecção de vazamentos. Compilação usa avisos tratados como erro. Sintaxe JavaScript/Python/iniciador e diff conferidos.

Casos novos: limites e campos desconhecidos, números finitos/valores vazios, escrita decimal conservada, aspas/barras/Unicode/nova linha, tentativas de código mantidas como strings, comparação exata/substring/prefixo, strings numéricas sem coerção, alternativa e lista vazia, prioridade sobreposta, entrada por referência real, alteração externa atual e reset reconhecidos, prévia sem escrita, frases independentes por uso, célula, fonte avançada e frase rica conservadas, revisão/instrumento/alvo/tipos, undo/redo/save/open e SIGKILL.

Quatro janelas de crash do comando visual de edição (`after_blobs`, `after_snapshot`, `before_commit`, `after_commit`) verificam que regra e frase reaparecem juntas no lado anterior ou confirmado. O protocolo de checkpoint anterior e seus testes de confirmação incerta continuam incluídos na matriz completa. Não é prova de toda falha possível de disco, energia ou sistema operacional.

## Interface e recuperação observadas

Motor real, SQLite auxiliar e navegador real, em `dados/obj01-piloto-cartoes/`, separados das cinco demonstrações anteriores. Foram observados:

1. Criação do exemplo numérico preço 15 → “vou negociar”; adição de terceira condição, edição do valor e mudança de prioridade.
2. Reabertura dos cartões, edição de preço para 16 e frase para “Na feira, Armand comentou: preço caro.”; desfazer restaura fonte/frase anteriores; refazer restaura o conjunto novo.
3. Criação da entrada textual Atitude/hostil → “O visitante parece ameaçador.”.
4. Criação em célula com entrada referenciada ao resultado desse trecho, comparando “ameaçador” → “Reação: atenção.”.
5. Sem Save, SIGKILL aplicado somente ao motor sintético e reinício com o binário final. Working geração 16, hash e snapshot integral iguais; Saved permanece na revisão 0. As três apresentações e suas fontes/referências reaparecem. Integridade SQLite `ok`, nenhuma violação de chave estrangeira. Histórico reinicia vazio.
6. Reabertura dos cartões recuperados e tentativa de valor numérico vazio: aviso aparece dentro do diálogo, documento não muda, rascunho continua disponível; Cancelar conserva o estado anterior.

Evidências: [antes do reinício](evidencias-obj01-cartoes/antes-reinicio.txt), [depois](evidencias-obj01-cartoes/depois-reinicio.txt), [resumo do reinício](evidencias-obj01-cartoes/reinicio.json), [texto antes](evidencias-obj01-cartoes/texto-antes.json), [texto depois](evidencias-obj01-cartoes/texto-depois.json), [editor textual](evidencias-obj01-cartoes/editor-texto.txt), [recusa visual](evidencias-obj01-cartoes/recusa-visual.txt), [captura do editor](evidencias-obj01-cartoes/editor-texto.jpg), [célula em prévia](evidencias-obj01-cartoes/celula-previa.jpg), [editor recuperado](evidencias-obj01-cartoes/editor-recuperado.jpg), [documento final](evidencias-obj01-cartoes/documento-final.jpg). JSONs do texto antes/depois são idênticos em bytes; dumps legíveis de acessibilidade têm espaços finais de linha normalizados. Reabrir os cartões depois da prova pode registrar contexto/seleção novos, sem mudar regra/frase.

## Falhas encontradas e corrigidas

- Servidor não admitia os novos nomes de operação: lista HTTP atualizada e testes reais incluídos. Capacidade anunciada pelo motor impede oferecer cartões novos a servidores antigos ainda em execução.
- Abertura de cartões de objeto existente selecionava o uso sem entrar em Texto: contexto corrigido antes de selecionar; edição/undo/redo conferidos no navegador.
- Mensagem de erro ficava atrás do diálogo: aviso inserido na própria caixa; tentativa de entrada vazia conferida.
- Clang recusou conversão implícita de signedness no teste de caracteres de controle: conversão explícita aplicada. Execuções que usaram binário antigo após essa falha de build não contam como validação; os logs finais foram produzidos após recompilação bem-sucedida.
- Execuções HTTP sem acesso à conexão local falharam ao criar socket. Foram refeitas com acesso local; logs finais das quatro configurações passaram. Não se apresenta esse impedimento do ambiente como falha do produto.

## CI, fontes e preservação

A execução [45050792508](https://github.com/Roger-Cardoso/inde/actions/runs/45050792508) corresponde ao commit de implementação e passou nos **25/25 trabalhos**; seu resultado completo está em [ci.json](evidencias-obj01-cartoes/ci.json). A matriz inclui os cartões, todos os perfis, adaptadores, planos, sessões e fontes fixadas. A conclusão e o total de trabalhos são conferidos no JSON estruturado antes de fechar a evidência.

Os três arquivos compactados foram novamente verificados por hashes/manifestos e extraídos em pasta temporária nova, com bytes conferidos: [fontes](evidencias-obj01-cartoes/fontes.json). Auditor de recusas: [cinco testes](evidencias-obj01-cartoes/auditor-fontes.txt). As dez origens/bancos das cinco demonstrações anteriores conservaram hashes: [início](evidencias-obj01-cartoes/preservados-inicio.json), [fim](evidencias-obj01-cartoes/preservados-final.json). Manifestos históricos são verificados nas respectivas revisões, sem reescrevê-los para acompanhar alterações posteriores.

Main permanece em `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, conservando a alteração de modo preexistente em `Preparar-ponte.sh`. Fontes, metadados, modelo/comandos da fundação anterior, adaptadores, protocolo de escrita e checkpoint não foram alterados neste corte. Nenhuma obra, schema canônico ou publicação autoral foi escrita.

## Lacunas reais

- Vínculo de variáveis a entidades, nomes alternativos e pseudônimos segue pendente. Armand é texto literal neste exemplo.
- Até 12 condições simples sobre uma entrada; grupos Todas/Alguma e outras composições lógicas visuais ficam pendentes. A linguagem avançada existente continua disponível.
- Só fontes integralmente reconhecidas abrem em cartões; expressões livres/comentários adicionais permanecem avançados. Funções textuais novas não são garantidas em binários antigos.
- Rascunho de diálogo não é recuperável até aplicar; histórico de desfazer não persiste entre processos. Prévia não altera isso.
- Frases com outros resultados usam Editar frase para sua montagem; cartões não as achatam.
- Não houve integração GTK, edição de offsets dos parágrafos originais, publicador canônico, `.inde` único ou início de Fase 3. Os 61 testes nativos registrados no corte anterior não foram repetidos como nova prova deste corte; foram repetidos os 13 programas do piloto/adaptador em quatro configurações e a matriz CI.
- Não comprovados todos os dispositivos/métodos de entrada, ampla concorrência, benchmark de latência/RSS ou todos os faults de armazenamento. Aceite humano do produto continua separado da validação técnica.

Conferência do pacote, na raiz deste worktree: `sha256sum -c docs/validacao/evidencias-obj01-cartoes/SHA256SUMS`.
