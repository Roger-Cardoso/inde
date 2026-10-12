# Alternativas, linhas e impacto — contrato e revisão / 2026-10-11

Continuação autorizada pelo usuário com “prossiga”. Isso autoriza implementação,
não acrescenta retroativamente aceite humano ao manifesto do primeiro corte.
Base Git 995fbb27, acrescida do primeiro corte privado congelado do construtor;
schema 27 aditivo. Fonte permanece não commitada. Sem push, tag ou publicação.

## Contrato de derivação

Duplicar e derivar usam a mesma transferência transacional completa do contexto
de planejamento existente: linhas/unidades, posições da montagem e associações
nas linhas, entidades/funções, vínculos, papéis de estrutura e unidade, datas de
linhas e unidades, Local, fila opcional, relações em foco e realizações textuais.
As duas ações preservam proveniência e diferem no tipo de criação, sem ativação.

Novas identidades para os itens da alternativa e para suas associações. Entidades,
relações, Locais, Documentos e referências temporais continuam compartilhados.
Não duplica texto, calendário ou fatos do mundo; não cria colocação editorial.
Essa política exige reconhecimento explícito no diálogo. Editar o Documento ou
a referência compartilhada pode afetar ambas as alternativas. Papéis globais da
Obra continuam globais, não são duplicados como papéis da alternativa.

O comando relê a origem na transação, compara a revisão observada e recusa
confirmação obsoleta; vetores fornecidos pelo chamador não autorizam cópia parcial.
Falha em qualquer etapa reverte estrutura, associações e registro de proveniência.
Proveniência guarda correspondência por identidade e observação normalizada do
ponto de derivação, sem corpo de texto. Limite próprio de 4 MiB; recusa explícita.

## Comparação sem promoção

Comparar estado atual de duas alternativas da mesma Obra ou o destino com seu
ponto de derivação registrado. Correspondência por linhagem, nunca por nomes
parecidos. Novos itens sem correspondência aparecem como adição/ausência. A
linhagem acompanha derivações sucessivas. A origem registrada não é atualizada
quando o autor modifica a alternativa, seus Documentos ou cadastros compartilhados.

Dimensões: título/designador/resumo/intenção/perspectiva, montagem, fila opcional,
Local, linhas e posições próprias, entidades/funções, relações em foco, associações
textuais, referências temporais e papéis. Encadeamento e sua autoridade também
entram. Compara declarações/associações, não o corpo da prosa nem os valores
internos atuais dos cadastros compartilhados. Não julga qualidade literária ou
equivalência entre narrativas, não faz merge nem reconcilia silenciosamente.

Uma página alternativa na área atual, com 40 diferenças por página e detalhes
recolhidos; até 4.000 diferenças, depois recusa em vez de truncar como completo.
Permite abrir a unidade atual na origem/destino. IDs da observação histórica não
são usados como alvos vivos. Preferências não persistem resultados de comparação
como aprovações: após reinício, a consulta deve ser refeita.

## Linhas e movimentos

Duplo clique numa linha, ou lente “Ordem própria da linha selecionada”, filtra e
ordena suas unidades pelas posições de associação. Mover nessa lente altera
somente essa associação; não a montagem, as outras linhas, a fila ou o editorial.
Lente, linha e unidade integram o contexto salvo. Descrição editável da linha
permite registrar pergunta/objetivo, sem impor estrutura de conflito.

Movimentos de montagem/fila mostram prévia: vizinhança declarada, contagem de
Documentos e datas preservados, avisos de vínculos e autoridades. Vizinhança
é conectividade direta, não propagação comprovada de consequências.

O autor pode declarar a autoridade de um vínculo: não classificada, montagem,
cronologia, fila ou editorial. A migração não classifica vínculos antigos.
Somente precede/depende na autoridade de montagem ou fila escolhida têm posição
avaliada nessa prévia. “A depende de B” testa B antes de A; causalidade não impõe
ordem de leitura. Falta de posição é insuficiência, não erro. Avisos não bloqueiam
movimento deliberado. Cronologia/editorial não são reordenados nem validados por
essa consulta; ciclos, vigências e dependências transitivas exigem outro contrato.

## Persistência e revisão

Schema 27: `narrative_branch_provenance` e `narrative_dependency_scopes`, índice
composto de dono do vínculo. FKs, histórico/reversão e Salvar cópia. A proveniência
histórica é evidência da decisão, não relacionamento com cadastros vivos que
deva impedir excluir um item. Contextos ativos continuam no auxiliar de exclusão.
Modelos reutilizáveis ainda são esqueletos, não alternativas com contexto.

Gates técnicos separados: Release/CTest; dois drivers de núcleo em Debug e
ASan/UBSan focais; GTK alternativas e regressões do ciclo planejamento/escrita e
fluxo temporal em Atlas/vazio Claro/Escuro e reinício; inicialização principal e
laboratório; inspeção visual selecionada; integridade/FKs e preservação dos
originais/entregas. Sanitizadores não instrumentam toda a aplicação; LSan desligado.
Resultados finais pertencem ao manifesto deste corte, não ao anterior.

Revisão humana deste corte pendente. Roteiro:

1. Derivar uma estrutura com datas de linha/unidade, papéis, Local e texto.
2. Conferir que só o planejamento ganhou identidades, sem publicação/ativação.
3. Comparar cópia intacta e depois uma montagem modificada; mudar título e fila.
4. Ordenar uma linha sem mudar a outra ou a montagem; fechar e reabrir o Projeto.
5. Declarar precedência de montagem, inverter a ordem, ler o aviso e manter a
   escolha. Classificar cronologia e conferir ausência de ordenação automática.
6. Escrever num Documento compartilhado e compreender o alcance dessa mudança.
7. Cancelar prévia, provocar erro de nome e preservar campos; examinar rolagem,
   contraste, teclado, retorno, exclusão guiada e reversão de uma derivação.

## Ainda não entregue

Realizações por trechos/ocorrências de composição; prosa independente por
alternativa com reconciliação autoral; comparação interna de calendários/cadastros;
dependências transitivas/ciclos e continuidade completa de tempo/espaço; arraste;
informação/conhecimento/revelação, modelos interrogativos e lentes avançadas.
Não acrescentar esses itens como “concluídos” por causa desta fundação.
Reconhecimento assistido de entidades por último; fusos e cartografia futura
continuam fora deste corte. Verificação técnica não é aceite humano nem release.
