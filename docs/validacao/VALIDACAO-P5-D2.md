# Validação P5-D2 — operações do arquivo

Data: 27/11/2026. Branch `feature/inde-p5-single-file`, base `4da5e09e8ac53848752bbe6f7d084ea2128f0593`. D1 foi aprovado pelo usuário. O aceite humano deste novo lote permanece separado dos testes.

O [contrato D2](../arquitetura/ARQUIVO-UNICO-P5-D2.md) reúne abertura/troca, criação, duas cópias, consulta paginada e restauração. Todos os projetos de teste são fictícios e novos. A cópia congelada e os três arquivos compactados foram novamente conferidos; main e projetos autorais não receberam alterações deste corte.

## Resultados locais já conferidos

- Núcleo independente: 5/5, com os dois perfis, publicação sem substituição, cancelamento e recibo incerto depois da restauração.
- Clang ASan/UBSan/LeakSanitizer: 5/5; ThreadSanitizer: 1/1.
- Percurso gráfico D2: 18 verificações, com teclado e mouse reais, histórico de mais de cem estados, cancelamento, preservação do vínculo anterior, adoção e saída pendente.
- Percursos anteriores: 14 verificações no formato integral, 14 por blocos e as 14 verificações gráficas D1.
- Lançador entregue: abriu o aplicativo normal com um arquivo fictício, recebeu Salvar/Encerrar por teclado e teve sua integridade conferida.
- Suíte nativa: 71 testes passaram na rodada completa. O novo teste de conflito foi ajustado para respeitar a trava de sessão e passou em reexecução específica (1/1). Nenhum teste ficou com falha pendente.

O novo teste de conflito inicialmente tentava abrir duas sessões protegidas no mesmo workspace, operação corretamente impedida. Ele agora injeta uma revisão externa coerente diretamente no banco fictício e comprova a recusa, mesmo com snapshot local limpo.

A primeira rodada gráfica encontrou a regravação redundante dos recibos da composição depois de confirmar a origem. A correção remove a drenagem repetida e verifica o salvamento idêntico sem regravar o banco. O teste de interrupção também passou a aguardar o estado nativo persistido, em vez de supor que um segundo fixo bastava sob carga. A cópia de recuperação pendente do ensaio inicial continuou preservada.

## Fechamento em andamento

A suíte geral do editor e a CI da revisão final serão registradas aqui com o manifesto verificável. Os resultados locais acima não são apresentados como resultado remoto.

## Limites preservados

A edição fica pausada durante as operações. Drenagem dos escritores, adoção do projeto, abertura do domínio nativo e reconstrução das especialidades ainda têm trabalho na interface. Não há garantia global de 100 ms, interrupção instantânea de qualquer chamada ao sistema, retenção automática, mudança de quotas, migração de obras reais ou conclusão da Fase 5/E3–E7.

[Demonstração nativa](../roteiros/ARQUIVO-UNICO-P5-D2.md).
