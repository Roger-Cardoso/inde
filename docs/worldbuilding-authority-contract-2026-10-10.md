# Aliases e calendários ficcionais — contrato v1

Base aceita: 354e58ac2729d5624f6a422d7b170b483f50b388. Oitavo bloco,
autorizado em 10/10/2026. Contrato registrado antes da migração.

- Alias é um nome alternativo de uma entidade existente, não outra entidade,
  relação, significado linguístico ou instrução de substituir texto. Nome
  canônico, UUID e menções existentes permanecem intactos. Busca inclui aliases,
  conserva filtros e paginação e mostra todos os candidatos; nunca cria binding.
- Aliases admitem apelido, título, grafia alternativa e outro, com nota de uso.
  NFC + casefold Unicode + remoção de espaço periférico definem duplicidade
  dentro da mesma entidade. Homônimos entre entidades são permitidos. A nota é
  descritiva: não promete escopo temporal, conhecimento situado ou privacidade.
- Calendário é uma definição imutável, nomeada, opcional, do Projeto. Uma regra
  nova cria outro calendário. Meses e semana configuráveis; intercalação v1:
  um dia adicional em um mês, a cada N anos, com fase explícita. Não é motor
  astronômico, gregoriano, de múltiplas eras ou de festas móveis.
- O eixo continua autoridade dos ordinais. Associação opcional fixa um
  calendário e o ordinal correspondente a ano 0, mês 1, dia 1. Uma unidade é
  um dia. Associar/trocar/desassociar não altera pontos, fatos ou rótulos.
  Mudança da associação exige confirmação e comparação de seu estado atual.
- Datas têm anos assinados com ano zero. Conversão é reversível, com divisão
  euclidiana para anos negativos e limites explícitos; data impossível ou
  overflow são recusados, nunca arredondados. Limite: anos -1.000.000 a 1.000.000,
  32 meses, 60 dias/mês, 16 nomes de dias da semana, ciclo até 10.000 anos.
- Criar ponto por data é uma escolha explícita e usa a associação conferida
  dentro da mesma transação. Prévia antiga não autoriza criar no calendário
  errado. Rótulo autoral fica separado da data derivada.
- Definições têm payload v1 bounded de 16 KiB e SHA-256; leituras validam forma,
  IDs, regras e checksum. Schema aditivo 22, transação, FK e Save As preservam
  aliases e calendários e levam as associações para o novo dono.
- A ficha mantém resumo compacto; gerenciar aliases é sob demanda. Calendário
  e vista mensal abrem na área modal existente, não em um painel permanente.
  Eventos aparecem somente a partir dos registros temporais, sem inferência.
- Operações novas entram no histórico do Projeto. Remover alias exige decisão
  explícita; definição de calendário não tem exclusão definitiva nesta v1.
  Reversão de entidade/eixo deve incluir seus aliases/associação pelas mesmas
  transações e checks do histórico existente, não por undo textual.
- Gates: normalização/colisão/paginação; calendário/bissexto/anos negativos/
  extremos; checksum/corrupção; conflito/ABA; restart/Save As; rollback da
  migração; GTK vazio/Atlas/temas/rodapé; conservação de origens e entregas.
  IME físico, HiDPI, acessibilidade assistiva e sessões longas continuam gates
  separados. A nova entrega exige novo aceite humano; não publica a reforma.
