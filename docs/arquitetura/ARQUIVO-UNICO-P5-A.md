# P5-A — arquivo de projeto com estados imutáveis

## Direção e alcance

Em 2026-11-26, o usuário autorizou antecipar o trabalho da Fase 5 e aprovou a ideia de reconhecer o que pode mudar, guardar estados anteriores e impor regras rígidas para aplicar e reverter alterações. A série E continua sendo trabalho de maturidade do editor; antecipar esta fundação não declara E3–E7, as Fases 3/4 ou a Fase 5 inteira concluídas.

Este corte implementa uma biblioteca **C++20 nativa**, um arquivo SQLite `INDE-P5-1`, uma ferramenta de conversão explícita e uma demonstração com o INDE real. Não introduz WebKit. A base congelada schema 37 e os projetos autorais não são migrados. O trabalho ocorre em `feature/inde-p5-single-file`, derivada de `1b77951` da branch OBJ-01; main permanece preservada.

A referência Projeção Narrativa separa o estado produzido pelo autor das ferramentas que o interpretam. Seus arquivos fornecidos foram examinados como dados: o exemplo `.sopa` codifica um JSON de estado; ele não fornece, por si, o protocolo de histórico e recuperação desta implementação. O INDE conserva a mesma separação conceitual sem copiar a implementação ou tratar a codificação de texto do exemplo como proteção contra corrupção.

## O que é armazenado

| Camada | Conteúdo | Regra |
| --- | --- | --- |
| Identidade | UUID do projeto e UUID deste armazenamento | Imutáveis; pedidos de outra identidade são recusados |
| Elemento | Chave, tipo, descrição JSON canônica, recurso opcional e referências | Uma edição cria outra versão; versões publicadas não são atualizadas |
| Recurso | Bytes originais e SHA-256 | Conteúdo idêntico é reutilizado; não se perde a imagem por depender do caminho externo |
| Estado | Conjunto de versões de elementos e hash do conteúdo | Publicado de uma só vez, em transação |
| Histórico | Sequência, anterior, origem da restauração, data e descrição | Restaurar acrescenta um estado; não elimina o futuro |

As descrições de elementos são texto estruturado. Recursos binários são guardados como bytes, sem conversão obrigatória para Base64. Identidade, conteúdo, referências e apresentação precisam ter donos explícitos; não é necessário reproduzir cliques antigos para abrir o último estado. O histórico neste corte guarda estados completos quanto à composição e reutiliza as versões de conteúdo; não é um diário de eventos que precisa ser reexecutado.

O parser JSON herdado interpreta reais com `std::stod`, dependente da configuração numérica. A fundação P5 envolve toda sua leitura JSON em um locale numérico C **local à thread**, restaurado inclusive em erro, e codifica reais com `to_chars` e precisão de ida e volta. Isso conserva decimais e zero negativo sob `pt_BR` sem alterar a configuração regional do processo ou do sistema. O parser congelado permanece intacto; outros consumidores legados fora desta fundação não foram corrigidos por esse invólucro.

## Contrato de alteração

1. Observar o projeto e guardar UUID do projeto/armazenamento, sequência e hash do estado observado.
2. Descrever um lote de alterações, com chave e versão esperada de cada elemento. Versão zero significa ausência esperada. Retirar um elemento retira seu uso atual, mantendo suas versões antigas.
3. Conferir campos, limites, texto UTF-8, JSON objeto sem chaves duplicadas, tipo estável da identidade, recursos e referências do conjunto resultante.
4. Reservar a escrita SQLite e conferir novamente o estado esperado. Um pedido antigo é recusado; não se tenta adivinhar como fundi-lo.
5. Inserir somente conteúdos alterados e os recursos ainda ausentes, criar a composição do novo estado e mudar o ponteiro atual na **mesma transação**.
6. Confirmar. Um pedido sem mudança de conteúdo não cria versão. Uma falha antes da confirmação desfaz o lote inteiro. Se a confirmação for incerta, bloquear o uso daquele handle até fechar e reabrir o arquivo para conferir o que foi confirmado.

Não é permitido retirar o destino de uma referência deixando um dependente no estado atual. Retirar ambos no mesmo lote é válido. O motor verifica existência dos vínculos; regras semânticas próprias de Documento, entidades, fórmulas, Comic e planejamento continuam nos serviços respectivos. Esta biblioteca não passa a ser um interpretador de fórmulas nem resolve o significado de vínculos autorais.

## Restauração

Exemplo: estado 1 contém a abertura; estado 2 contém a continuação. Restaurar 1 cria estado 3 cujo conteúdo corresponde a 1, com `parent=2` e `restored_from=1`. O estado 2 continua consultável e pode ser retomado em outro estado. Restaurar conteúdo que já é atual não cria uma cópia redundante.

O hash confere o conteúdo lógico, incluindo os UUIDs, descrições, recursos e referências ordenadas. Sequência, data, descrição e IDs internos das versões não fazem parte desse hash. Por isso uma restauração pode ter o mesmo hash de conteúdo e outra sequência. Os vínculos do histórico são verificados separadamente, inclusive a igualdade entre o conteúdo restaurado e o estado indicado como sua origem.

Na abertura são conferidos o formato, o desenho exato das tabelas/índices/triggers, `integrity_check`, chaves estrangeiras, **todos os estados históricos**, hashes e bytes dos recursos. Não há retorno silencioso a uma versão antiga quando alguma parte do histórico está corrompida. Não há descarte automático de histórico para caber em um limite.

## Arquivo e falhas

- SQLite `application_id=1229866033`, `user_version=1`, tabelas STRICT, chaves estrangeiras, modo defensivo, `trusted_schema=OFF`, `journal_mode=DELETE`, `synchronous=FULL`.
- Exclusão entre sessões cooperantes por `flock`, verificação de inode/caminho e detecção de alterações externas por `data_version`. Handle herdado por `fork` é recusado.
- Criação exclusiva, sem seguir link no arquivo final, arquivo regular sem hardlinks e permissão inicial `0600`.
- Arquivo desconhecido ou em modo WAL é recusado antes da abertura SQLite para escrita. Não se remove journal de um arquivo com gravação interrompida: a recuperação normal do SQLite é seguida pela conferência integral.
- Durante uma transação pode existir um journal transitório. “Arquivo único” descreve o projeto fechado e transportável, não a inexistência de arquivos temporários durante qualquer operação.
- SHA-256 e triggers detectam danos e alterações fora do contrato; não constituem assinatura, criptografia, autenticação ou proteção contra um agente com controle total dos arquivos.
- Testes interrompem processos antes/depois de COMMIT e forçam `SQLITE_FULL`. Não simulam perda elétrica de hardware, firmware defeituoso ou todos os sistemas de arquivos. Recuperação depende também das garantias de SQLite, sistema de arquivos e armazenamento.
- Estados anteriores no mesmo arquivo ajudam a desfazer; **não substituem uma cópia de segurança externa** caso o arquivo inteiro seja perdido ou fisicamente danificado.

## Compatibilidade com o INDE existente

O primeiro transporte conserva os componentes atuais da pasta como elementos identificados por caminho, com descritores JSON e bytes incorporados. O banco canônico schema 37 e os bancos auxiliares são preservados integralmente. Isso evita converter milhares de relações em outro modelo antes de provar a reversibilidade.

**Consequência deliberada:** uma alteração no banco canônico guarda uma nova versão daquele arquivo de banco, não somente das linhas modificadas. A biblioteca já admite versões pequenas de elementos, mas o adaptador do projeto real ainda trabalha por componente. Não se deve anunciar que cada entidade real do INDE já foi convertida em um registro textual independente.

Conversão e reabertura:

1. Exigir uma cópia de pasta de projeto schema 37, fechada e coerente. Recusar links, arquivos especiais e arquivos auxiliares SQLite ativos. Bancos WAL não são normalizados automaticamente.
2. Manter transações de leitura nos bancos em journal DELETE, conferir integridade/vínculos, capturar todos os arquivos e pastas vazias, verificar inventário e hashes novamente.
3. Criar arquivo pendente novo, conferir, fechar e publicar com renomeação Linux que recusa substituir um destino existente.
4. Para reabrir, conferir o estado inteiro, criar pasta pendente privada, gravar bytes e pastas, recapturar e comparar todos os componentes; somente então publicar uma pasta nova.
5. Conservar UUID do projeto. É uma cópia de transporte/recuperação, não uma duplicação autoral “Salvar como” com remapeamento de identidades.

Falhas deixam uma cópia pendente identificada, sem apagar a origem e sem substituir destinos. Os caminhos de componentes são relativos, canônicos e confinados à pasta. A conferência do conversor não equivale a uma análise de segurança completa de bancos maliciosos para execução posterior no editor.

## Limites deste perfil

| Limite padrão | Valor | Ao atingir |
| --- | --- | --- |
| Arquivo versionado | 512 MiB | Recusar a gravação; preservar versões |
| Recurso individual | 64 MiB | Recusar |
| Descrição de elemento | 1 MiB e profundidade menor que 128 | Recusar |
| Elementos de um estado | 20.000 | Recusar |
| Estados, incluindo o inicial | 10.000 | Recusar |
| Versões de elementos | 100.000 | Recusar |
| Participações no histórico / referências de versões | 1.000.000 / 400.000 | Recusar |
| Transporte de pasta | 128 MiB e 4.096 componentes, incluindo pastas | Recusar |
| Plataforma deste corte | Linux, SQLite ≥ 3.37 | Outros ambientes ainda não certificados |

Não foram implementados compactação, coleta de versões antigas, reparo automático, união de alterações de duas cópias ou sincronização. O bloqueio é por arquivo; cópias independentes não se coordenam. Não há promessa de desempenho de grandes acervos baseada apenas nos projetos de teste.

## Demonstração e próximo contrato

[Roteiro utilizável](../roteiros/ARQUIVO-UNICO-P5-A.md), [validação](../validacao/VALIDACAO-P5-A.md), [API](../../cpp/project_file.hpp), [transporte](../../cpp/project_file_transport.hpp).

A demonstração abre uma pasta materializada no editor GTK existente e registra a pasta no arquivo único **ao encerrar normalmente o demonstrador**. O botão Salvar do editor ainda grava a pasta; não foi alterado para gravar o arquivo único. Este corte não entrega adoção transparente do novo arquivo pelo editor.

O próximo contrato deve integrar abertura, criação, Salvar, Salvar como, troca de projeto e encerramento ao mesmo ciclo de confirmação; preservar a área de trabalho após interrupção; conciliar recuperação da Escrita e dos objetos; mostrar o histórico com restauração verificável e manter os caminhos/Recentes apontando para o arquivo escolhido pelo usuário. Só depois dessa integração e de testes reais de teclado/mouse deve-se oferecer conversão de obras autorais. A conversão semântica por entidade e a evolução de formato exigem contratos próprios; não podem ser confundidas com empacotamento reversível.
