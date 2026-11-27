# Experimentar a preparação nativa — P5-D3

Execute `scripts/demos/Abrir-demo-preparacao-nativa.sh` nesta cópia de trabalho. O lançador compila e abre o próprio aplicativo, cria um projeto fictício novo em `dados/p5d3-demo-*` e usa um perfil privado. Não liga as pausas artificiais do host de teste.

1. Abra **Uma obra transportável** e altere uma frase. Use Ctrl+S imediatamente depois de digitar.
2. Observe a preparação da escrita/recuperação, a confirmação dos objetos e a conferência do arquivo. Operações pequenas podem terminar rapidamente.
3. Quando disponível, Cancelar salvamento conserva o trabalho na área de recuperação e impede iniciar a publicação do arquivo. Escritas privadas já iniciadas podem terminar normalmente.
4. Repita Salvar. O texto mais recente deve aparecer depois de encerrar e reabrir o arquivo fictício.
5. Use Ctrl+Alt+S para continuar em cópia do estado atual. Ela também prepara os escritores, sem exigir outra gravação no arquivo original.
6. Durante uma operação, Sair ao terminar/Ctrl+Q registra o pedido; Continuar no editor o retira. Ao iniciar a confirmação final do arquivo, cancelar fica indisponível.

A edição é pausada durante a preparação e confirmação. Reconstrução da folha, adoção do domínio e parte da preparação de valores ainda usam a interface. Este roteiro não declara uma meta de latência para textos grandes ou conclusão da Fase 5.

[Contrato](../arquitetura/ARQUIVO-UNICO-P5-D3.md) e [validação](../validacao/VALIDACAO-P5-D3.md).
