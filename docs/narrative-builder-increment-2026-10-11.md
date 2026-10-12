# Construtor narrativo — primeiro corte / 2026-10-11

Base: `995fbb27f8ee9359643a90f0220bc71269ef5e9a`, aceita pelo usuário.
Implementação privada, schema 26 aditivo. A autorização da proposta não é aceite
deste novo candidato. Este corte cobre o reparo operacional e a primeira passagem
de unidade para Documento; não encerra toda a proposta de quatro blocos.

## Autoridades e fluxo

| Visão | Autoridade preservada | O que não faz |
|---|---|---|
| Montagem narrativa | Posição declarada da unidade na estrutura | Não muda datas, associações nas linhas, fila ou editorial |
| Cronologia ficcional | Referências, eixo, calendário, precisão e incerteza existentes | Não se deduz de posição, causalidade ou timestamp |
| Fila de escrita | Inclusão e posição de trabalho explicitamente escolhidas | Não é histórico de edição nem prioridade obrigatória |
| Apresentação ao público | Estruturas editoriais/colocações/produção existentes | Não é reorganizada pelo construtor |

Sem regra universal para gênero, técnica ou estilo. Precedência legada permanece
não classificada; ainda não é uma dependência temporal tipada. Relação em foco
não altera relação canônica; entidade vinculada não declara presença física.
Local é contexto planejado, sem criar viagem, coordenada ou fato no mundo.

Em Planejar → Estrutura narrativa → abrir estrutura → Abrir unidade:

1. Editar intenção/resumo/perspectiva; situar datas pelo componente temporal
   existente, com calendário gráfico quando aplicável.
2. Expandir somente o necessário: ordens/trabalho, Local, linhas/entidades/relações
   em foco, realizações textuais, encadeamento/remoção.
3. Incluir explicitamente na fila se desejar. Comandos de movimento nomeiam a
   ordem que afetam; não movem as outras.
4. Associar Documento existente ou criar Documento vazio e independente.
   Muitos Documentos podem desenvolver uma unidade e o mesmo Documento pode
   desenvolver várias. Não se afirma correspondência de todo o texto.
5. Escrever abre uma realização única; havendo várias, o autor escolhe o destino.
   O cabeçalho oferece Voltar ao planejamento, salvando antes do retorno.
6. Retirar realização, relação em foco, vínculo de linha/entidade ou contexto de
   Local sem apagar texto ou fatos. Novos usos integram o auxiliar de exclusão.

Seleção, lente e seções abertas integram o contexto de Planejar e seus espaços
salvos. O botão de retorno é um atalho da sessão; não promete sobreviver ao
encerramento. Retomada do planejamento por preferências é distinta desse botão.
Validação de formulário conserva campos. Consultas recusam acima de 2.000
unidades/estrutura, 10.000 linhas de associações por consulta, 4 MiB de campos por
consulta ou 8 MiB de snapshot; não exibem resultado truncado como completo.

## Persistência e segurança

- `narrative_unit_contexts`: Local opcional, posição de escrita opcional e token
  de contexto; não duplica os campos/posição da unidade.
- `narrative_realizations`: associação unidade–Documento; texto não é copiado.
- `narrative_unit_relations`: associação de foco; não duplica a relação.
- FKs compostas, validação de Local, transação composta, comparação do snapshot
  observado na confirmação, journal/reversão existentes e integração do uso
  nomeado no auxiliar de exclusão. Snapshot não é autorização persistida.
- Salvar cópia troca a identidade de projeto dos três conjuntos. Migração
  transacional recua se falhar; documentos/editorial/tempos anteriores não mudam.
- A duplicação antiga omitia datas e papéis. Derivação/duplicação com contexto
  adicional agora é bloqueada dentro da transação, preservando a origem. Modelo
  reutilizável é explicitamente descrito como esqueleto, não cópia completa.

## Verificação e aceite

Resultados finais deste corte, com o binário principal
`3859e0c9017ae00d1a7e5d329f58103f2dac3413c048d2a939ef541e62875607`:

| Verificação | Estado / escopo |
|---|---|
| Release CTest | Passou, 29/29; log `ctest-narrative-builder-sealed-2026-10-11.log` |
| Núcleo Debug / ASan+UBSan | Passou; driver e seis unidades, dependências Release, LSan desligado |
| Interface construtor Release | Passou, 8/8, relatório `temporal-20261011-122351-757427/report.json` |
| Interface construtor ASan+UBSan | Passou, 8/8; driver e sete unidades, relatório `temporal-20261011-122744-036619/report.json` |
| Regressão fluxo temporal | Passou, 8/8, relatório `temporal-20261011-122356-498220/report.json` |
| Inicialização principal/laboratório | Passou, seis segundos cada em cópia preparada; só startup |
| Inspeção visual selecionada | Quatro capturas finais inspecionadas; não certificação de acessibilidade |
| Aceite humano deste corte | Pendente; gate de avanço não aprovado |

Logs em `../logs`, manifesto na pasta `outputs` do chat. Não reutilizar números
dos candidatos anteriores como prova deste. Ensaios GTK automatizados são
diferentes de smoke de inicialização e de aceite humano. Rodadas intermediárias
incompletas/fracassadas permanecem registradas e não compõem a prova final.

Passe humano pendente, em Projeto recém-criado e Atlas, Claro/Escuro:

- Criar uma unidade numa linha, editar título/resumo e conferir seleção.
- Contar primeiro o resultado e depois a causa, com datas contrárias à montagem;
  mover a montagem e verificar que calendário/editorial não mudaram.
- Incluir só parte das unidades na fila e mudar essa fila independentemente.
- Associar/retirar Local, entidade, relação e Documento; conferir preservação.
- Escrever, retornar à unidade/lente/seções, trocar workspace e reabrir projeto.
- Examinar contraste, teclado/Tab, rolagem, títulos longos e formular erro sem
  perder campos. Medir fluidez subjetiva; testes não provam facilidade humana.
- Tentar exclusão de um Documento/Local/relação usado: abrir a unidade indicada,
  retirar apenas o vínculo pretendido e voltar à exclusão.

## Próximo gate e débitos explícitos

Derivação completa/reconciliação, comparação e impacto; semântica tipada de
dependências; desenvolvimento por linhas; realizações por trechos/ocorrências;
intenções e transformações opcionais; conhecimento/revelação; lentes avançadas e
cruzamentos de continuidade ainda não entregues. Ordenação dentro de cada linha
permanece independente e não ganha editor de movimento neste corte.
Sem geração de prosa, pontuação literária, presença inferida, fusos, reforma
cartográfica, NLP automático ou novo formato público. Reconhecimento assistido
de entidades continua por último. Revisão deste corte precede o próximo bloco.
