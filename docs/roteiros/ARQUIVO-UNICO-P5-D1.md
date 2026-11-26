# Experimentar a confirmação em segundo plano — P5-D1

Execute com Bash [Abrir-demo-confirmacao-assincrona.sh](../../scripts/demos/Abrir-demo-confirmacao-assincrona.sh), sem argumentos. Ele compila a versão desta branch, cria um projeto fictício com texto e objetos, acrescenta a uma cópia um recurso de observação de 16 MiB e abre outro arquivo único. As fontes fictícias iniciais ficam preservadas. O perfil e os dados são separados dos projetos autorais.

1. Abra “Uma obra transportável”, altere uma frase e use Ctrl+S.
2. Observe as fases da conferência. A edição fica pausada; a janela continua respondendo.
3. Durante a conferência, Cancelar salvamento mantém o trabalho na área de recuperação. Tente novamente e confira que pode continuar escrevendo no mesmo ponto.
4. Ao iniciar a confirmação final, o cancelamento fica indisponível. O programa espera o resultado.
5. Durante a operação, use Sair após salvar, Ctrl+Q ou feche a janela. Para retirar o pedido, use Continuar no editor.
6. Feche o projeto ou encerre normalmente e reabra seu arquivo para conferir o texto confirmado.

O tempo disponível para interação depende do computador e do conteúdo. A demonstração usa dados e operações reais, sem o mecanismo de pausas controladas dos testes.

Abertura, criação, cópias e histórico ainda têm trechos síncronos. Salvar em segundo plano não permite alterar a área simultaneamente à sua captura. Consulte os [limites e evidências](../validacao/VALIDACAO-P5-D1.md).
