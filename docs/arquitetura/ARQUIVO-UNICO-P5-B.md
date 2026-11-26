# P5-B — arquivo único no editor nativo

## Continuidade

Corte autorizado pelo pedido de continuidade de 26/11/2026, sobre `cefde34`, na branch `feature/inde-p5-single-file`. Usa a fundação [P5-A](ARQUIVO-UNICO-P5-A.md), o mesmo editor C++/GTK e os serviços proprietários. Não altera as fontes congeladas nem migra projetos autorais. Não declara a Fase 5 inteira, a série E ou os recursos comuns com Comic concluídos.

## Percurso entregue

| Ação | Efeito |
| --- | --- |
| Projeto → Novo arquivo único `.inde` | Cria um projeto nativo novo e publica seu arquivo verificado antes da abertura |
| Projeto → Abrir arquivo único `.inde` | Abre o arquivo com lease exclusiva e uma área de trabalho persistente própria |
| Abrir por caminho ou Recentes | Reconhece arquivo ou pasta; Recentes aponta para o arquivo escolhido, não para a área interna |
| Salvar / Ctrl+S | Espera os escritores nativos, salva texto e objetos e confirma o estado no `.inde` |
| Estados do arquivo / Ctrl+Alt+H | Confirma o trabalho atual, lista revisões e permite retomar uma revisão como novo estado |
| Cópia com histórico | Copia o banco completo pelo backup SQLite, confere e publica sem substituir destino; original continua ativo |
| Continuar em cópia do estado atual | Publica outro arquivo a partir do trabalho atual e o abre; mantém o original, seu histórico e sua área de recuperação |
| Fechar, trocar projeto ou Sair | Confirma o arquivo após os escritores nativos; falhas são apresentadas sem descartar o trabalho |
| Falha da confirmação ao Sair | Permite voltar ou encerrar conservando a área de trabalho para a próxima abertura |

Novo/Abrir pelos controles antigos continuam atendendo projetos em pasta. O novo formato é opt-in pelo menu, pelos atalhos Ctrl+Alt+N / Ctrl+Alt+O, por caminho ou por Recentes. Não há conversão silenciosa de obras existentes.

As cópias transportáveis mantêm o UUID do **mesmo projeto**. A cópia com histórico conserva também o UUID do armazenamento e todas as revisões; o caminho separa suas áreas de trabalho. A cópia do estado atual ganha outro armazenamento e começa um histórico novo. Não são uma nova obra nem um remapeamento universal de entidades. O comportamento de cópia independente dos projetos antigos em pasta permanece separado.

## Autoridade e momentos de gravação

O arquivo contém os estados confirmados. Durante a edição, os serviços existentes trabalham numa pasta privada materializada. A recuperação/autosave da Escrita continua gravando nessa área; uma alteração de texto não dispara captura do projeto inteiro. Salvar, fechar, trocar projeto, consultar o histórico e certas cópias passam pela barreira de confirmação.

A barreira drena os coordenadores de texto e recuperação, espera a composição textual e grava os objetos; o serviço de Comic também é consultado. O transporte mantém reservas de leitura nos bancos SQLite enquanto monta e publica o estado. A confirmação do arquivo precede a indicação de sucesso. Na barra principal, mensagens de autosave da área são identificadas como trabalho nessa área, sem prometer confirmação do `.inde`.

A confirmação ainda é síncrona e por componente, com as cotas P5-A. Não é chamada a cada tecla. Projetos grandes, latência de Salvar e crescimento do histórico por cópias de bancos inteiros continuam exigindo o corte de granularidade/performance; esta integração não torna o formato automaticamente adequado a todo acervo.

## Sessão e recuperação

A área fica em `XDG_STATE_HOME/inde/project-files`, ou em `~/.local/state/inde/project-files`. O demonstrador e os testes usam perfis próprios. Cada armazenamento/caminho tem uma pasta privada `0700`; arquivos novos e descritores são `0600`. Cada geração materializada é uma pasta `work-UUID.inde`, atendendo ao contrato de caminhos do repositório nativo.

O descritor `INDE-WORKSPACE-1` guarda origem, UUID do projeto/armazenamento, sequência/hash de base, geração e estado de encerramento. É gravado por arquivo temporário, sincronização, renomeação e sincronização do diretório. Não fica dentro do projeto capturado.

Na reabertura, o conteúdo da área é comparado com o arquivo; o rótulo `clean` sozinho não autoriza descartar nada:

1. Se a área corresponde ao estado confirmado, ela pode ser reutilizada. Isso também resolve confirmação do arquivo seguida de falha no recibo, sem duplicar a alteração.
2. Se difere, o autor pode cancelar, retomar o trabalho ou abrir o salvo. Abrir o salvo reserva o descritor antigo e conserva a pasta anterior.
3. Se o arquivo avançou desde a base, o aviso explicita isso. Retomar a área é uma escolha de novo estado sobre a versão observada agora; versões confirmadas posteriores permanecem no histórico. Não há fusão automática.
4. Descritor danificado/ausente com áreas existentes ou caminhos inseguros não são tratados como autorização para apagar. A abertura do salvo preserva essas evidências; retomada automática insegura é recusada.
5. Handle com confirmação incerta não continua escrevendo. Reabrir o mesmo arquivo libera a lease antiga após drenar os escritores e observa novamente arquivo e área.

O destrutor da sessão **não** marca o trabalho como limpo. A finalização explícita confere a pasta outra vez. Nenhuma geração reservada é limpa automaticamente neste corte.

## Retomar histórico

A materialização do alvo é conferida antes de alterar o estado atual. A restauração só ocorre se a área corresponde à base confirmada. Depois de publicar a revisão, o editor reabre os serviços sobre outra geração materializada; a pasta anterior fica preservada. Leituras em andamento, caches das especialidades e retornos de navegação são invalidados também quando o UUID do projeto permanece igual. Estado zero, anterior à captura de um projeto completo, não aparece como opção de projeto no seletor.

Isso é restauração de projeto. Não equivale a adicionar um comando de texto ao desfazer do Documento. Texto, formatação, alternativas, visibilidade, objetos e planejamento retornam juntos ao estado armazenado.

## Falhas e fronteiras

- Falha antes da publicação deixa o arquivo anterior e a área persistente. Falha do recibo depois de confirmar exige reabertura e conferência.
- Falha ao confirmar o arquivo durante Sair oferece encerramento preservando o trabalho já drenado para a área. Não é autorização para descartar texto que o escritor nativo ainda não conseguiu gravar nessa área.
- A cópia do estado atual permite continuar em outro arquivo sem exigir mais uma revisão num arquivo que atingiu sua cota. O original não é apagado nem compactado.
- Cópias mantêm a identidade do projeto, não sincronizam alterações e não são duplicação semântica independente.
- Não há coleta de gerações antigas, compactação, migração de schemas reais, aumento das cotas, incorporação de dependências externas nem certificação fora de Linux.
- Não mover/substituir um arquivo aberto. Alterações externas são recusadas; as áreas continuam preservadas, mas localizar automaticamente a recuperação depois de mudar seu caminho está fora do corte.
- Histórico e áreas no mesmo computador não substituem backup externo.

A leitura JSON do núcleo nativo foi corrigida na cópia de build para usar `from_chars` em reais, evitando dependência de vírgula decimal. A fonte congelada fica intacta e o teste nativo exercita `pt_BR`. Isso não constitui revisão universal dos demais formatos e linguagens de expressão do programa.

[Demonstração](../roteiros/ARQUIVO-UNICO-P5-B.md), [validação](../validacao/VALIDACAO-P5-B.md), [sessão](../../cpp/project_file_session.hpp), [integração GTK](../../cpp/main_window_project_file.cpp).
