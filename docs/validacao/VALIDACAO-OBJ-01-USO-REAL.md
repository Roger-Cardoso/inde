# OBJ-01 — recuperação da sessão e regressões de uso

Execução: 23–24/11/2026. Branch `feature/inde-obj-01-foundation`. Base: `b6e122d`.

> Complemento de 24/11/2026: os menus e as ferramentas das especialidades exigiram um resgate adicional. Consulte [Especialidades restauradas](VALIDACAO-OBJ-01-ESPECIALIDADES-RESTAURADAS.md), com cobertura ampliada de ações, painéis e reordenação. Os resultados deste relatório correspondem ao corte anterior.

## Motivo e alcance

O usuário relatou que editar, trocar de especialidade e encerrar o programa estavam bloqueados. A entrega anterior passou por testes estruturais e CI, mas esses resultados foram apresentados com confiança excessiva quanto ao uso cotidiano. O teste gráfico anterior acionava sinais de widgets diretamente: ele não comprovava que um botão estava alcançável pelo mouse, que o foco estava correto ou que o encerramento normal concluía sem erro.

Este corte corrige os bloqueios investigados e acrescenta um percurso por eventos de teclado e ponteiro X11 na janela nativa. Não encerra a Fase 1, não declara aceite humano, não integra à `main`, não migra projetos e não entrega exportação nova.

## Sessão que já estava aberta

Antes de interagir, foram preservados o manifesto e cópias consistentes dos três bancos pelo mecanismo de backup SQLite, incluindo recuperação e composição. A preservação local fica em `dados/obj01-regressoes/preservacao-novo-20261123-201917/`, com acesso restrito ao proprietário. Capturas e dados autorais não são publicados neste repositório.

A janela estava em um formulário de inserção de figura. O processo continuava atendendo à acessibilidade; não foi constatado deadlock. Uma tentativa de inspeção de pilha foi recusada pelo sistema operacional. Não se alterou essa proteção. A ação normal de Cancelar pela acessibilidade devolveu a janela à folha; a ação normal Fechar encerrou o processo, sem matá-lo. A comparação após o encerramento confirmou título e texto preservados, schema 37 e três bancos íntegros, sem violações de chaves estrangeiras.

Não foi demonstrado por que o clique físico de Cancelar não respondeu naquela sessão antiga. O funcionamento pela acessibilidade não prova a causa do relato. O novo teste cobre o clique no formulário normal e após erro de validação em projeto descartável.

A figura original escolhida foi lida, validada e exercitada no seletor nativo de um projeto descartável. Seu hash permaneceu igual. Ela não é incluída nas evidências públicas.

## Correções

- Aplicar/Cancelar e Voltar passam a ficar fora da região rolável dos formulários. O conteúdo pode rolar sem esconder a saída do formulário.
- A troca de especialidade permanece acessível durante um rascunho. O editor oferece continuar editando ou descartar somente o formulário ainda não aplicado antes de prosseguir. O comportamento padrão conserva o rascunho.
- Encerrar com um formulário aberto oferece a mesma decisão, em vez de bloquear o fechamento silenciosamente. Operações em andamento, escolha de arquivo e entrada provisória continuam protegidas, com indicação do motivo.
- Cancelar e concluir retornam à folha imediatamente, mesmo quando sua reconstrução deve aguardar a entrega da entrada de texto. Uma atualização adiada é refeita antes de se considerar a apresentação sincronizada.
- O encerramento normal deixa de consultar foco/display em uma janela GTK já destruída. Conexões de composição de texto são desligadas antes da destruição das vistas e ignoram eventos de um Documento anterior.
- A vista de parágrafo adia sua atualização durante a composição do acento e não reposiciona uma seleção já correta. Antes, uma atualização podia reiniciar o contexto de entrada GTK e perder o acento provisório. A confirmação de entrada provisória também libera sua barreira mesmo se uma etapa posterior recusar a atualização.
- Falhas de leitura e validação PNG têm motivos identificáveis e mensagens específicas, mantendo o formulário para corrigir ou cancelar.
- O iniciador aceita o caminho de um projeto existente e usa o renderizador Cairo como padrão local, sem alterar configuração global.

A propriedade do diálogo segue o adaptador nativo existente, que se descarta após a resposta. As ações de troca/fechamento são retomadas fora do despacho GTK. Não há descarte automático de texto confirmado nem substituição do Documento autoritativo.

## Verificação por entrada real

`tests/run_obj01_real_input.py` abre um display, gerenciador de janelas e sessão D-Bus privados. `tests/test_obj01_real_input.py` envia cliques e teclas por X11. `cpp/object_usage_probe.cpp` observa widgets/estado; não cria projeto, não preenche buffers e não emite sinais para executar as ações.

O percurso parte da tela inicial normal: cria projeto pelo seletor nativo, cria Documento, escreve duas linhas com Enter e acento composto pelo GTK, alterna as nove especialidades conservando o texto, mantém um rascunho ao recusar fechamento, cancela pelo botão, resolve outro rascunho ao trocar de especialidade, escolhe PNG, provoca erro de texto alternativo e cancela pelo botão, reinsere figura decorativa, cria tabela e edita célula, cria cartão de valor e equação, salva e encerra normalmente após resolver um último rascunho. O texto persistido, objetos e integridade são conferidos depois do encerramento.

O método de entrada simples do GTK e o layout internacional são fixados apenas no teste. A entrada provisória é observada antes de confirmar o acento. Isso evita confundir temporização/mapeamento de teclas do driver com regressão do editor. O teste não representa certificação de IBus/Fcitx ou todos os métodos de entrada.

Os testes estruturais anteriores continuam cobrindo IME, seleção entre parágrafos, projeção, histórico, recuperação e publicação. Eles agora incluem cancelar durante entrada provisória, retorno à folha e resolução do bloqueio após seu fim. Essas verificações por sinal são identificadas como estruturais, separadas do percurso X11.

## Reprodução e limites

```sh
cmake --build build-nativo --target inde inde_object_text_projection_tests --parallel 2
python3 tests/run_obj01_real_input.py
INDE_REAL_INPUT_FULL_SUITE=1 python3 tests/run_obj01_real_input.py
bash scripts/demos/Abrir-demo-folha-comum.sh /caminho/do/projeto.inde
```

O teste exige `xvfb-run`, `dbus-run-session`, `xfwm4`, `xdotool`, `xclip` e `setxkbmap`. Não usa a sessão gráfica do autor. A suíte normal ignora o percurso X11 quando ele não foi iniciado pelo runner privado; a CI o executa obrigatoriamente em etapa própria. O driver observa o foco do campo do seletor antes de enviar a confirmação do caminho.

## Resultados registrados

Correção do aplicativo: `e015bfca94074db29a0be2c61d74f5b6fd128b09`. Código final do teste e observador: `53375ee4ca360983b3cbaa1f0f89f2c1a9a58fc1`.

| Verificação | Resultado | Evidência |
|---|---|---|
| CI do código final | 26/26 trabalhos aprovados | `ci.json`, `ci-nativo-testes.txt` |
| Suíte local completa, com percurso X11 obrigatório | 98/98, 95,080 s | `python-98.txt`, `resultados-locais.json` |
| Mesmo percurso com a figura original, em projeto descartável | 1/1, 40,759 s; origem inalterada | `figura-original.json` |
| Fontes integradas fixadas | 758 arquivos | `base-schema37.json` |
| Compactados originais e inventários internos | 3/3 conferidos | `auditor-fontes.json` |
| Arquivos anteriores protegidos | 83/83 inalterados | `preservacao-anteriores.json` |
| Fontes da versão local | 40 hashes fixados e hash do executável | `codigo-validado.json` |
| Recuperação/encerramento da sessão antiga | Texto preservado; 3 bancos íntegros; schema 37 | `recuperacao-sessao.json` |
| Conferência na retomada | Texto igual à cópia fechada; 3 bancos íntegros | `integridade-projeto-retomada.json` |

A CI inicial do commit de correção passou em 25/26 trabalhos e falhou na nova verificação X11 do seletor de arquivos. O aplicativo compilou e passou nos quatro testes nativos e nos testes de publicação. A falha não foi escondida nem transformada em passe: foi acrescentada observação do foco do campo de caminho e o driver passou a devolvê-lo ao campo antes de confirmar. O teste completo passou localmente após esse ajuste. Trechos da falha estão em `ci-primeira-falha.txt`.

A versão corrigida foi reaberta no projeto do usuário em 24/11. A janela mostrou a folha, seus parágrafos e as nove especialidades; a abertura não produziu mensagens críticas GTK. Ela foi deixada aberta. Capturas e conteúdo do projeto permanecem na pasta local de diagnóstico, sem publicação.

Os resultados do relatório anterior permanecem históricos e não constituem aceite de usabilidade. As evidências deste corte ficam em [evidencias-obj01-uso-real](evidencias-obj01-uso-real/). A [CI final 45394393989](https://github.com/Roger-Cardoso/inde/actions/runs/45394393989) concluiu com sucesso em 24/11/2026, sobre exatamente `53375ee4ca360983b3cbaa1f0f89f2c1a9a58fc1`: 26/26 trabalhos, incluindo GCC/Clang com e sem sanitização no domínio e o aplicativo nativo em Release. Nesse trabalho nativo, os quatro testes de janela/projeto, os quatro testes de publicação e o novo percurso X11 passaram; este último levou 25,194 s. A aprovação não é um passe humano nem certificação de todos os fluxos.

Para verificar os registros sem abrir ou gravar projetos:

```sh
python3 docs/validacao/evidencias-obj01-uso-real/verificar.py
```

O verificador confere o manifesto das evidências, os 40 arquivos de código, os compactados e a correspondência entre commit e CI, inclusive a etapa X11 obrigatória. O hash do executável é registrado como proveniência local; ele não é distribuído e sua recompilação pode produzir outro hash. Os dados privados e os projetos de teste são locais, ignorados pelo Git.

Limites: o relato amplo de outros erros não está esgotado por este percurso. Não há passe humano concluído, teste prolongado de manuscrito grande, cobertura de todos os diálogos/falhas de disco, todos os ambientes gráficos, métodos de entrada externos ou exportação dos novos objetos. Não foi reproduzida a causa exata do clique sem resposta na sessão antiga. A proteção de publicação dos formatos ainda não suportados permanece vigente.
