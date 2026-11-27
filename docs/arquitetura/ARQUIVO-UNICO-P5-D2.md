# P5-D2 — lote combinado de operações do arquivo

O usuário aprovou D1 e autorizou seguir para o próximo da fila como um grande lote combinado. Base `4da5e09e8ac53848752bbe6f7d084ea2128f0593`, branch `feature/inde-p5-single-file`. O aceite D1 é humano; a validação D2 tem evidências próprias e não modifica os manifestos históricos A–D1.

## Resultado e percurso

O mecanismo exclusivo de confirmação passa a atender abertura/troca, criação, cópia com histórico, cópia do estado atual, consulta e restauração do histórico. A confirmação antes de deixar um arquivo também antecede a troca para uma pasta antiga. As especialidades continuam operando o mesmo editor nativo.

Antes de um trabalho, a interface drena seus escritores e guarda o contexto. O trabalhador recebe valores, caminhos e uma sessão emprestada exclusivamente, sem widgets nem o serviço de projeto ativo. Na criação, um serviço nativo independente nasce, cria uma semente fictícia e é destruído inteiramente no trabalhador. A adoção do projeto e a reconstrução das especialidades continuam na interface.

A sessão anterior permanece ativa enquanto uma abertura ou cópia é preparada. A nova sessão só é adotada depois de a operação terminar. Falha/cancelamento conserva o vínculo anterior; uma cópia já publicada permanece no destino mesmo se sua ativação falhar. A reabertura do mesmo arquivo com confirmação incerta precisa liberar sua trava; nesse caso a vista antiga é fechada primeiro, mantendo sua área de recuperação.

Confirmar o projeto de origem e executar a operação seguinte são dois trabalhos separados. Cancelar o segundo não desfaz a confirmação anterior. A cópia do estado atual mantém a exceção necessária para escapar de um arquivo cheio: captura a área drenada sem tentar acrescentar outra versão ao arquivo original. Ela continua no arquivo novo somente depois da publicação e materialização verificadas.

## Regras de publicação e cancelamento

| Operação | Preparação cancelável | Fronteira final |
| --- | --- | --- |
| Abrir | Verificação do arquivo, comparação da recuperação e materialização privada | Antes de publicar o registro da sessão escolhida |
| Criar/copiar estado atual | Captura, transação no arquivo temporário e verificação | Antes de publicar o nome do arquivo novo, sem substituição |
| Copiar histórico | Verificação e backup SQLite incremental de 64 páginas por passo | Antes de publicar o destino, sem substituição |
| Consultar histórico | Leitura dos estados no trabalhador | Nenhuma alteração persistente nesta consulta |
| Retomar estado | Comparação do trabalho atual e materialização privada do alvo | Antes do COMMIT da nova versão de restauração |

Um COMMIT no arquivo temporário não fecha o cancelamento da publicação do destino. O controle de preparação encaminha checkpoints, mantendo separado o controle que sela a publicação. As materializações privadas de abertura e restauração também não selam a operação prematuramente.

Depois da fronteira, cancelar é recusado. Uma falha de recibo depois do COMMIT continua sendo incerteza, com reabertura obrigatória. Se uma restauração altera a sessão e falha depois, o editor antigo é fechado para impedir gravações através de repositórios ligados à geração anterior. As gerações, versões e destinos pendentes são preservados.

Não há interrupção forçada de SQLite ou de uma chamada ao sistema já iniciada. A verificação observa o cancelamento entre estados, registros e recursos; transporte e backup têm checkpoints próprios. Um pedido aceito pode aguardar a chamada em curso retornar.

## Confirmação repetida da composição

Os ensaios gráficos encontraram uma recusa real no encadeamento: a segunda drenagem dos escritores regravava os recibos da composição depois de confirmar a origem. Isso fazia a cópia corretamente recusar uma área que já diferia da base. O lote elimina essa segunda drenagem e torna idempotente a confirmação de um snapshot protegido idêntico.

A confirmação sem mudanças ainda abre uma transação e confere as revisões e bytes salvos/em trabalho. Não contorna conflitos externos. Quando tudo corresponde, não reescreve recibos, gerações ou o banco da composição. Há testes de igualdade exata dos bytes e de recusa de uma revisão externa mesmo quando a memória local está limpa.

## Interface e histórico

A camada modal informa o nome da operação e as fases reais. Cancelar operação fica indisponível na confirmação final. Sair ao terminar e Ctrl+Q registram a intenção; Continuar no editor a retira. Uma consulta com saída pendente não abre a seleção de histórico. Operações que mudam o vínculo terminam a adoção antes de passar pelo encerramento normal.

O histórico é lido fora da interface; a apresentação monta no máximo cem opções por página, com Estados anteriores/seguintes e contagem. Isso limita o trabalho de montagem de controles, sem mudar o formato ou apagar estados. Retomar um estado idêntico continua sem criar revisão duplicada.

O temporizador automático de contexto também fica suspenso enquanto há uma operação. A interface recolhe o trabalhador antes de reutilizar a sessão ou destruir suas capturas; não há thread destacada. Erros com o código de cancelamento só são classificados assim quando o controle recebeu um cancelamento real.

## Compatibilidade e fronteiras

Continuam o envelope `INDE-P5-1`, os perfis integral e de blocos, schema nativo 37, limites, identidade e regras de recuperação. Não há migração de projetos reais, coleta automática de pendências, reescrita de histórico ou alteração em main. As fontes compactadas e a cópia congelada permanecem verificáveis.

A drenagem dos escritores, a abertura do domínio nativo pela interface, a adoção e a reconstrução das especialidades ainda podem consumir tempo na interface. O trabalho total de I/O permanece; a edição fica pausada durante uma operação. Carregamento de pastas antigas, fluidez editorial e E3–E7 não são declarados concluídos por este lote. Fase 5 segue aberta, inclusive armazenamento semântico definitivo, evolução de formato e política de retenção.

[Validação](../validacao/VALIDACAO-P5-D2.md), [demonstração](../roteiros/ARQUIVO-UNICO-P5-D2.md).
