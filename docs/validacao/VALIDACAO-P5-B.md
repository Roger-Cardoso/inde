# Validação P5-B — arquivo único no editor nativo

Data: 2026-11-26. Branch `feature/inde-p5-single-file`, continuidade de `cefde34789d4216804208d61b8030adc0d5225a9`. O [contrato](../arquitetura/ARQUIVO-UNICO-P5-B.md) delimita a adoção opt-in no editor existente. Não é conclusão da Fase 5 inteira nem certificação de obras autorais.

## Escopo e casos

| Camada | Exercício |
| --- | --- |
| Biblioteca de sessão | Confirmação explícita, recusa de segunda abertura, interrupção real de processo, retomar área, abrir salvo, preservar diretórios, cópia com histórico, restauração e perda do recibo depois de COMMIT |
| Recuperação auxiliar | Descritor inválido, descritor ausente com áreas existentes, recusa de caminho inseguro e conservação dos dados reservados |
| Núcleo proprietário | Projeto real schema 37 com Documento completo, alternativas, visibilidade, semântica, Composição textual/rascunho, planejamento, PNG, tabela, equação e arranjo divergente |
| Interface real | Abrir `.inde`, escrever, Ctrl+S, consultar/retomar histórico, copiar com histórico e somente estado atual, Recentes com caminho verdadeiro, interrupção e recuperação, criação nativa e seletores |
| Encerramento com erro | Diretório do arquivo fica temporariamente sem escrita: Sair oferece preservar a área, encerra normalmente e a abertura seguinte recupera o conteúdo |
| Fluidez | A escrita/autosave nativos na área não alteram o arquivo único a cada tecla; a confirmação ocorre na barreira explícita |

O driver GTK observa widgets e usa eventos reais de teclado/mouse. Não chama sinais de botões nem troca diretamente os valores do editor para simular resultados. Os bancos consultados pelo driver são cópias fictícias, em leitura, para verificar o conteúdo realmente confirmado no arquivo.

## Resultados

Código validado: `f5001baab10424d62ec6da809eed2808009e1b5a`. A [execução de CI 45892089650](https://github.com/Roger-Cardoso/inde/actions/runs/45892089650) concluiu em 26/11/2026, às 20:45:20 UTC, com **5/5 jobs aprovados**: GCC e Clang com e sem ASan/UBSan/LeakSanitizer (3 testes em cada combinação), mais integração nativa (4 testes P5, demonstração anterior e percurso gráfico P5-B completo). O demonstrador novo também foi verificado localmente no aplicativo normal.

O passe local e a execução de CI identificada são registrados nas [evidências](evidencias-p5-b/manifesto.json). Os logs separam testes de biblioteca, testes nativos, regressão da interface existente e o percurso do novo formato. A existência de botões ou de um workflow não é aprovação de funcionalidade; o aceite humano continua separado.

Passe local em 26/11/2026:

| Verificação | Resultado |
| --- | --- |
| Biblioteca GCC 15 Release | 3/3, 3,23 s |
| Biblioteca Clang 18 + ASan/UBSan/LeakSanitizer | 3/3, 8,13 s |
| Regressões nativas após correção decimal | 69/69, 163,88 s |
| Interface existente, suíte completa | 103/103, 267,55 s |
| Percurso novo GTK | 14 condições verificadas; resultado e log próprios |
| Demonstrador P5-B entregue | Aplicativo normal, arquivo direto, Ctrl+S, Ctrl+Q e integridade aprovados |
| Fontes integradas e compactados | 758 arquivos preservados e três arquivos compactados conferidos |

A suíte de 103 casos antecede apenas a correção decimal e a invalidação final das visualizações na troca de materialização. O passe nativo de 69 casos inclui a correção decimal; o percurso GTK final inclui as duas correções. Os testes têm dados fictícios e não constituem aceite humano de obras reais.

## Correções encontradas durante o trabalho

- O repositório nativo normaliza diretórios com o sufixo `.inde`. As gerações materializadas foram alinhadas a esse contrato; o arquivo continua sendo a origem exibida em Recentes.
- A primeira versão do formulário de histórico tentava destruir o diálogo além da destruição já gerenciada pelo editor. O teste real reproduziu o encerramento anormal e o depurador identificou a liberação duplicada. A integração passou a usar a propriedade existente do diálogo.
- O seletor automatizado precisava confirmar o foco no campo de caminho antes de Enter; colagem e conferência integral evitam interferência da conclusão automática de nomes. O seletor real é exercitado sem emitir respostas artificialmente.
- Uma confirmação do arquivo seguida de falha no descritor não permite repetir cegamente a escrita. Reabertura compara os bytes e reconhece o estado confirmado.
- O cache de Recentes aceita arquivos INDE reconhecidos pelo cabeçalho, sem incluir pastas internas ou aceitar qualquer arquivo avulso.
- O leitor JSON nativo usava conversão dependente de locale. A transformação revisada usa `from_chars`, e o teste proprietário lê um decimal sob `pt_BR.UTF-8`. O arquivo congelado continua byte a byte preservado.

## Reproduzir

Biblioteca, sem GTK:

```bash
cmake -S tests/project_file -B build-p5 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-p5 --parallel 2
ctest --test-dir build-p5 --output-on-failure
```

Integração na aplicação já compilada, em X11/D-Bus privados:

```bash
xvfb-run -a dbus-run-session -- python3 tests/project_file/test_native_session.py /CAMINHO/NOVO/PARA/EVIDENCIAS
```

São necessários `xvfb`, `xauth`, `xdotool`, `xclip`, `xfwm4` e D-Bus, além das dependências do INDE. O teste exige destino novo e cria seus próprios projetos e perfis. O demonstrador para experiência humana fica no [roteiro P5-B](../roteiros/ARQUIVO-UNICO-P5-B.md).

## Limites e preservação

- Main, fontes compactadas/congeladas e obras autorais não são alteradas. As mudanças proprietárias são transformações declaradas de uma cópia de build.
- O formato novo é opt-in. Os comandos antigos continuam atendendo projetos em pasta.
- O adaptador ainda versiona componentes inteiros. Volume, custo de Salvar e crescimento do histórico precisam de um corte próprio de granularidade; não há promessa de meta de latência para acervos grandes.
- Cópias mantêm o mesmo projeto/identificadores; não há remapeamento semântico independente ou sincronização entre cópias.
- Áreas de trabalho ficam preservadas e podem ocupar espaço; não há limpeza automática nem poda silenciosa de histórico.
- Cotas, exclusão de WAL, fronteira de dependências externas, Linux e necessidade de backup externo continuam conforme P5-A.
- O teste de falha de gravação é por permissão do diretório, acompanhado dos testes anteriores de `SQLITE_FULL` e interrupção. Não é simulação de perda elétrica física.
