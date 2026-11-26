# P5-C — confirmação e histórico por blocos

Continuidade autorizada pelo pedido “prossiga”, sobre `e605b37`, na branch `feature/inde-p5-single-file`. A integração permanece no editor nativo, com o ciclo de sessão e recuperação de [P5-B](ARQUIVO-UNICO-P5-B.md). Nenhum projeto autoral ou schema interno é migrado. O corte não encerra a Fase 5 ou as pendências da série E.

## Resultado e alcance

Uma alteração pequena num banco grande não deve acrescentar outra cópia inteira desse banco ao histórico. Novos arquivos criados pelo menu de arquivo único e novas cópias do estado atual usam blocos de 64 KiB, reutilizados por hash. A restauração reconstrói os bytes exatos antes de publicar a pasta usada pelos serviços existentes.

Não é conversão semântica por Documento, entidade ou célula. Os serviços continuam operando seus bancos schema 37 numa área persistente. Não é necessário reproduzir eventos antigos para abrir um estado. Descrições e relações são texto estruturado; imagens e outros recursos continuam sendo bytes incorporados, sem Base64 obrigatório.

## Compatibilidade explícita

| Origem ou ação | Perfil e comportamento |
| --- | --- |
| Arquivo P5-A/P5-B existente | Salvar mantém `INDE-DIRECTORY-37`, com componentes inteiros |
| Novo arquivo único pelo editor | `INDE-DIRECTORY-37-CHUNKS-1` |
| Nova cópia somente do estado atual | Perfil por blocos; outro armazenamento do mesmo projeto; original preservado |
| Copiar com todo o histórico | Conserva o perfil, identificadores e estados do arquivo de origem |
| Comando `converter` / API sem perfil explícito | Mantém o perfil anterior, para compatibilidade de integrações |
| Comando `converter-blocos` | Exige destino novo e cria o perfil por blocos |
| Leitor de P5-B diante de um arquivo por blocos | Recusa a materialização, sem substituir destino ou modificar a origem |
| Leitor de P5-B diante de novo estado no perfil antigo | Continua materializando normalmente |

O envelope SQLite permanece `INDE-P5-1`, schema 1. A nova capacidade é um perfil de transporte distinto, com raiz `chunked-project`; a raiz antiga continua `legacy-project`. A versão anterior do aplicativo não materializa o perfil novo. Não há promessa de leitura retroativa das novas capacidades por executáveis anteriores.

Abrir ou salvar um arquivo anterior não o converte para blocos. O editor não oferece conversão do histórico existente neste corte. A cópia explícita do estado atual permite experimentar o novo perfil conservando o original.

## Modelo e invariantes

- A raiz do perfil declara formato e UUID do projeto e referencia os dois componentes canônicos obrigatórios.
- Cada diretório conserva caminho relativo canônico e vínculo com seu pai.
- Cada arquivo `chunked-file` contém caminho, tamanho, SHA-256 completo e lista **ordenada** dos hashes dos blocos. Suas referências são o pai e o conjunto dos blocos utilizados.
- Cada registro `chunk/HASH`, tipo `transport-chunk`, carrega o recurso de mesmo hash e declara o tamanho. Blocos completos têm 65.536 bytes; somente o último pode ser menor. Arquivo vazio tem lista vazia e hash do conteúdo vazio.
- Repetição de um bloco na receita é permitida; referência repetida não é. Arquivos diferentes podem compartilhar blocos idênticos dentro do armazenamento.
- Todo bloco do estado pertence a pelo menos um arquivo. Blocos ausentes, sem dono, com tipo, tamanho, conteúdo ou referências divergentes impedem a materialização.
- A receita deve reconstruir exatamente o tamanho e o hash completo declarados. Depois disso, a pasta é recapturada e comparada ao estado antes da publicação sem substituição.
- Retirar um arquivo ou reduzir seu tamanho remove os usos no estado atual, preservando versões e recursos históricos. Restaurar continua acrescentando uma revisão.
- Erros continuam passando pelas barreiras transacionais, verificação de estado esperado, preservação da área de trabalho e tratamento de confirmação incerta de P5-A/P5-B.

Continuam em vigor as cotas de 128 MiB por projeto transportado, 64 MiB por componente, 4.096 componentes, 512 MiB no armazenamento e os limites de registros, referências e histórico. Blocos também consomem registros e entradas de histórico; não há aumento silencioso de cotas.

## Trabalho evitado em cada confirmação

A identificação de bancos lê somente o cabeçalho, em vez de copiar todo arquivo nessa primeira passagem. A captura ainda lê e confere o conteúdo completo; o teste de origem inalterada continua baseado em hashes, sem confiar somente em `mtime`.

A comparação produz operações somente para registros alterados ou retirados. A confirmação recebe bytes somente dos recursos necessários às operações alteradas, evitando enviar e validar novamente recursos de elementos inalterados. As reservas de leitura SQLite, `integrity_check`, conferência de vínculos e comparação final da origem permanecem.

O perfil antigo também recebe essas otimizações de leitura e montagem de operações, conservando sua representação e compatibilidade.

## Prova e limites

A [validação](../validacao/VALIDACAO-P5-C.md) compara a revisão anterior com a implementação atual, usando o mesmo gerador determinístico e seis pequenas atualizações por sessão persistente em bancos de 1, 4 e 16 MiB. Mede tempo de confirmação, crescimento dos recursos, crescimento físico do arquivo e memória máxima do processo. Confere a restauração e a preservação do histórico. O tempo medido é do serviço de confirmação; não equivale a latência completa da interface.

A biblioteca continua conferindo integralmente os arquivos na captura. Salvar e restaurar ainda são síncronos na interface. Escrita que desloca muitos bytes, reorganização física do SQLite, VACUUM e conteúdo totalmente substituído podem alterar muitos blocos. Não há garantia universal de ganho nem meta de até 100 ms para todo projeto.

O histórico ainda guarda entradas do estado inteiro e permanece sujeito às cotas. Não há compactação ou limpeza automática de recursos, áreas reservadas ou gerações antigas. A economia obtida por blocos não substitui a futura granularidade semântica, ensaios maiores, trabalho assíncrono e gestão de recuperação. Dados locais continuam exigindo backup externo.

[Demonstração](../roteiros/ARQUIVO-UNICO-P5-C.md), [implementação](../../cpp/project_file_transport.cpp), [comparação reproduzível](../../tests/project_file/benchmark_comparison.py).
