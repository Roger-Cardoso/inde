# Experimentar operações do arquivo — P5-D2

Execute `scripts/demos/Abrir-demo-operacoes-arquivo.sh` na cópia `obj01-audit`. O lançador compila o aplicativo nativo e cria uma pasta nova em `dados/p5d2-demo-*`, com perfil privado, projeto fictício, objetos e um recurso de 16 MiB. Abre o próprio INDE; não usa a interface de observação dos testes.

1. Altere uma frase e use Ctrl+S. Observe as fases e o cancelamento antes da confirmação.
2. Use Ctrl+Alt+S. Escolha Copiar com o histórico e um destino novo. O projeto original continua ativo.
3. Repita escolhendo Continuar em cópia do estado atual. A cópia fica ativa, preservando o arquivo anterior.
4. Use Ctrl+Alt+H. Escolha um estado anterior e Retomar este estado. As versões posteriores permanecem no histórico.
5. Use Ctrl+Alt+N e um destino novo para criar outro arquivo. Use Ctrl+Alt+O para voltar ao anterior.
6. Durante uma operação, experimente Cancelar operação ou Sair ao terminar. Continuar no editor retira a saída pendente. Operações pequenas podem terminar antes de você clicar; os testes usam pausas apenas no host de observação para conferir cada fronteira.

A edição é pausada enquanto o trabalhador confere o arquivo. Depois da confirmação final, o cancelamento fica indisponível até o resultado. Se uma operação falhar depois de publicar uma cópia, confira o destino informado antes de tentar novamente. Use apenas os dados fictícios desta demonstração para o aceite deste corte.

A aprovação humana desta demonstração é distinta dos testes automatizados em [Validação P5-D2](../validacao/VALIDACAO-P5-D2.md).
