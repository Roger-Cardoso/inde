# P5-D3 — preparação nativa e passagem para a folha

O usuário aprovou D2 e pediu o próximo lote combinado. Base `2fad9a3`, branch `feature/inde-p5-single-file`. Este corte continua o fechamento das operações nativas antes de alterar a representação semântica do projeto. O aceite D2 fica registrado; o aceite humano D3 é separado da validação técnica.

## Percurso combinado

A pausa e o progresso são apresentados antes de esperar os escritores. A recuperação e o salvamento do Documento continuam pertencendo aos coordenadores nativos existentes. A interface acompanha seus recibos sem chamar `drain()` para esperá-los durante a preparação do arquivo único.

Depois dos recibos, a interface captura o Documento atual e prepara a confirmação dos objetos. Um trabalhador recebe apenas esse valor, o runtime puro da sessão e um recibo; nenhum widget ou serviço de projeto pertencente a GTK entra nessa função. Ele sincroniza a sessão dos objetos com o Documento confirmado e executa o protocolo existente de salvamento. Só então começa a captura/confirmação do arquivo.

O caminho atende Salvar, fechar projeto, encerrar e a confirmação anterior a abrir/criar/copiar com histórico/consultar estados. A cópia do estado atual e a saída de uma pasta para um arquivo também usam a preparação, sem tentar salvar novamente o arquivo original quando se escolhe continuar numa cópia.

O fechamento normal de um arquivo não executa uma barreira síncrona anterior ao progresso, nem repete a drenagem depois de a confirmação ter terminado. O encerramento explicitamente escolhido para preservar trabalho, após uma falha, também não exige reabrir uma composição incerta antes de permitir fechar a janela.

## Propriedade e cancelamento

`Preparation` transmite uma única função de trabalho ou uma exceção por mutex/condition variable. O trabalhador pode aguardar/cancelar sem executar callbacks GTK. Uma segunda entrega é recusada. Destruir a tarefa cancela e recolhe esse trabalhador, mesmo sem entrega. Os coordenadores do texto continuam pertencendo à Escrita.

Cancelar a preparação impede iniciar a confirmação do `.inde`. Escritas de recuperação/autosave já iniciadas continuam sendo escritas válidas na área privada; não são desfeitas nem declaradas descartadas. Se a confirmação privada dos objetos já começou, ela termina antes de observar o cancelamento do arquivo. Não há interrupção forçada de uma transação SQLite. O limite irreversível de publicação do arquivo permanece o mesmo de D1/D2.

Durante o empréstimo da sessão dos objetos, a adoção automática da folha fica suspensa e `can_leave()` recusa novas operações. O recibo é consumido na interface somente depois do resultado release/acquire e da coleta do trabalhador. Sucesso, falha e cancelamento liberam a preparação; o estado de persistência incerta dos objetos também é devolvido à interface.

Os testes também interrompem o processo depois da confirmação do texto e antes da confirmação dos objetos, reabrem a área preservada e confirmam o texto recuperado no arquivo.

Uma confirmação de objetos idêntica conserva o contrato D2 de não regravar recibos, sem deixar de conferir conflitos externos.

## Recuperação antes do transporte

A regressão também encontrou um journal privado interrompido entre a gravação do Documento e seu recibo de recuperação. O texto principal podia estar durável, sem nova escrita a solicitar, mas o conversor corretamente recusava o arquivo auxiliar restante.

A preparação agora passa esse banco ao proprietário nativo para recuperação normal do SQLite. Journals quentes são revertidos pelo próprio SQLite. Quando sobra um journal ainda não quente, uma transação reservada reconfirma o mesmo `user_version=1` do banco privado e deixa SQLite encerrar seu journal. A versão é conferida sob a reserva; nenhum registro, estado de recuperação ou schema é convertido. Não há remoção manual do auxiliar, resolução de registros pendentes ou descarte de evidência.

O caminho recusa links, hardlinks, componentes de outro proprietário, journal órfão, modo WAL e formato privado desconhecido. Isso não certifica bancos arbitrários hostis. Testes interrompem processos nas duas fronteiras, conferem todos os campos do registro antes/depois e recusam formatos/auxiliares inseguros. A interface também confirma novamente uma área com journal frio interrompido e confere a igualdade dos registros.

## Passagem para o projeto

Antes de consumir o resultado e reconstruir a folha, um turno do loop principal apresenta a etapa de atualização. A tarefa continua sendo a dona da pausa nesse intervalo: a edição permanece bloqueada e pedidos de saída são conservados. Não há bombeamento manual de eventos ou loop GTK aninhado.

A adoção do projeto, os caches e a renderização continuam no thread da interface. O corte torna sua posição no percurso explícita; não afirma que o carregamento do domínio inteiro foi transferido a um trabalhador.

## Limites e compatibilidade

Não mudam envelope, schemas, perfis, limites, identidades ou política de retenção. Main, fontes congeladas e projetos autorais são preservados. As demonstrações e testes usam projetos fictícios novos.

A reabertura excepcional da mesma sessão incerta ainda drena seus escritores antes de liberar a trava antiga. Capturar os valores GTK, concluir alterações de visibilidade/composição, salvar Comic quando estiver alterado, guardar preferências/manifesto e o raro recibo de desfazer até a base ainda têm trabalho na interface. Renderização e adoção também permanecem nela. O corte remove a espera síncrona dos trabalhadores textuais e a persistência dos objetos dessa preparação, sem prometer cancelamento instantâneo ou latência global de 100 ms. A Fase 5, a granularidade semântica e E3–E7 continuam abertas.

[Validação](../validacao/VALIDACAO-P5-D3.md) e [demonstração](../roteiros/ARQUIVO-UNICO-P5-D3.md).
