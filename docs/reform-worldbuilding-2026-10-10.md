# Oitavo bloco da reforma: nomes alternativos e calendários ficcionais

**Registro histórico:** a forma de calendário descrita abaixo foi rejeitada
pelo usuário. Seu formulário textual não é mais a interface desta cópia.
Consulte [a adaptação temporal](reform-temporal-adaptation-2026-10-10.md)
para o estado novo e os débitos. Os artefatos congelados da oitava entrega
continuam intactos; esse registro não transforma rejeição em aceite.

Base humana aceita: `354e58ac2729d5624f6a422d7b170b483f50b388`, sétima
entrega, Composições persistentes. Esta frente foi autorizada em 10/10/2026.
É implementação privada para avaliação, não release público, promoção do
laboratório ou conclusão da reforma. O aceite deste bloco permanece pendente.

## O que mudou e onde encontrar

### Nomes alternativos

Em **Planejar**, abra uma entidade e use **Ações da entidade → Nomes
alternativos…**. A ficha mostra um resumo compacto; o gerenciamento completo
abre somente quando solicitado. Adicione apelido, título, grafia alternativa
ou outro nome, com nota de uso. Clique no alias para editar ou remover; a
remoção exige confirmação explícita.

A busca do explorador consulta nome canônico, resumo e aliases, sem duplicar
a mesma entidade nos resultados. Quando o resultado vem de um alias, a razão
de correspondência informa “nome alternativo”. Os filtros e a paginação
continuam válidos. Se duas entidades compartilham um nome, ambas aparecem:
o programa não decide que são a mesma pessoa, lugar ou acontecimento.

O UUID e o nome canônico ficam intactos. Nenhuma ocorrência em Documento é
reescrita, nenhum vínculo semântico é criado e nenhum significado lexical
é escolhido. Duplicidade dentro da mesma entidade considera espaços
periféricos, NFC e casefold Unicode, conservando diferenças de acentuação.

### Calendários

No tempo ficcional, selecione um eixo e use **Ações do eixo atual → Calendário
do eixo…**. Crie uma definição nomeada: um mês por linha no formato
`Nome | quantidade de dias`. A semana é opcional. A seção recolhida
**Intercalação opcional** permite um dia adicional em um mês, a cada N anos,
com fase explícita. A definição pertence ao Projeto e é imutável; mudanças
nas regras criam outra definição, sem mudar silenciosamente datas anteriores.

Escolha o calendário e o ordinal correspondente ao ano 0, mês 1, dia 1.
Confira a prévia e marque a confirmação antes de aplicar. Uma unidade de
ordinal equivale a um dia. Associar, trocar ou retirar o calendário muda
apenas a leitura derivada das datas: ordinais e rótulos autorais não mudam.
Qualquer alteração do formulário invalida sua confirmação anterior.

**Novo ponto** oferece criação por data quando o eixo tem calendário.
Ano, mês e dia geram uma prévia de data e ordinal; datas impossíveis são
recusadas, não ajustadas. O rótulo do ponto continua independente. A associação
é conferida novamente dentro da transação de criação. Um formulário antigo
não cria um ponto sob regras que mudaram enquanto ele estava aberto.

**Ver mês do calendário…** mostra os pontos e acontecimentos realmente
registrados naquele mês. Clique em um ponto para abrir sua ficha temporal.
É consulta, não inferência de eventos ou calendário de produtividade.
A consulta seleciona o intervalo do mês diretamente, evitando que pontos
antigos consumam o limite e escondam um mês futuro. A grade tem rolagem
própria, inclusive horizontal para semanas extensas; o rodapé fica separado.

Os rótulos derivados também aparecem nas listas, nos seletores temporais e
na linha temporal. A consulta em lote reutiliza associações e definições,
sem abrir um banco para cada ponto mostrado.

## Autoridade, persistência e recuperação

O schema 22 acrescenta `entity_aliases`, `fictional_calendars` e
`axis_calendars`. Não substitui estruturas, Documentos, Composições, ordinais
ou catálogos linguísticos. Payload de calendário v1: forma estrita, até 16 KiB
e SHA-256 validado na leitura. Os payloads semântico v2, checkpoint v4 e
clipboard v2 permanecem inalterados.

Projetos migrados para o schema 22 devem ser abertos nesta versão, não nos
executáveis anteriores. A avaliação usa novas cópias: não há downgrade
destrutivo para tentar reabrir aliases ou calendários em schema 21. As cópias,
os executáveis e o candidato original anteriores permanecem preservados.

Aliases e associações usam tokens de alteração para recusar conflitos,
inclusive mudança seguida de retorno ao valor original (ABA). Erros ficam
no formulário, preservando sua entrada. Operações entram no histórico do
Projeto; prévia, conflitos, reversão e cascatas usam o mecanismo transacional
existente. **Salvar como** conserva aliases, definições e associações com a
nova identidade do Projeto. Banco ou definição inválida falha fechado.

## Limites deliberados desta v1

- Até 64 aliases por entidade; nome de até 512 bytes UTF-8 e nota de 2.048.
  A nota descreve uso; ainda não restringe época, idioma ou conhecimento.
- Até 64 definições por Projeto; 1–32 meses, 1–60 dias por mês, até 16 dias
  da semana, um dia intercalado, ciclo até 10.000 anos. O mês intercalado
  não pode ultrapassar 60 dias. Anos de −1.000.000 a 1.000.000, com ano zero.
- Não é calendário gregoriano/astronômico, multiera, de festas móveis ou
  intercalações múltiplas. Sem exclusão definitiva de definições pela UI.
- Exibição mensal: até 500 pontos e 500 acontecimentos, com contagem real
  e aviso explícito de exibição parcial. Não há truncamento do banco.
- A reforma segue minimalista: ações contextuais e profundidade progressiva,
  sem novos painéis fixos ou outra área principal de trabalho.
- Testes automatizados e imagens não são aceite humano. IME físico, leitores
  de tela, HiDPI, base autoral extensa e sessões longas precisam de avaliação
  própria. Sanitizadores focais não certificam toda a aplicação/GTK.

## Avaliação humana sugerida

1. Na cópia vazia, crie uma entidade com um título e um apelido. Encontre-a
   pelos dois nomes; experimente um homônimo em outra entidade. Confira que
   os textos e nomes canônicos continuam intactos.
2. No Atlas, crie um calendário pequeno, confira a prévia de pontos antigos
   e associe-o explicitamente. Compare a lista e a linha temporal antes/depois.
3. Crie um ponto em ano negativo e outro em mês intercalado. Experimente um
   dia inexistente. Abra o mês e siga o ponto de volta à ficha.
4. Troque ou retire a associação. Confira que nenhum ordinal ou rótulo mudou.
   Feche e reabra a cópia; experimente Salvar como em outro destino privado.
5. Compare tema claro/escuro, janela reduzida, leitura, navegação por teclado
   e rodapés. Use somente as novas cópias isoladas, não a obra original.

## Verificação técnica desta entrega

- Release: 22/22 CTest, incluindo `inde_worldbuilding_tests` e as regressões
  das frentes anteriores. Asserções permanecem ativas nos testes dedicados.
- GTK real: 46 cenários, união exata de quatro blocos concluídos e disjuntos
  em Atlas/vazio; 20 cenários adicionais em tema escuro, em dois blocos.
  Binários e drivers idênticos entre os blocos; `G_DEBUG=fatal-warnings`.
- ASan/UBSan focal: sete unidades do núcleo e quatro da UI, mais os testes;
  quatro cenários GTK com reabertura. Dependências restantes em Release,
  LSan desativado. Não é certificação de toda a aplicação.
- Verificação de posições após rolagem: comando final da definição,
  ponto no fim do mês e rodapé efetivamente dentro da área visível.
- Duplicidade de nome ou de data tem orientação em português, preserva
  a entrada e não funde identidades ou pontos automaticamente.
- Preservação: comparação byte a byte dos 402 arquivos do candidato
  original e hashes dos 14 executáveis das sete entregas congeladas.
- Relatórios interrompidos/exploratórios foram conservados, mas não integram
  o gate final. Cópias e configurações de teste são isoladas. As novas cópias
  de avaliação não recebem os aliases, calendários ou registros sintéticos
  dos testes.

Os registros finais estão em `reforma-mundo-verificacao-2026-10-10.json`,
`reforma-mundo-tema-escuro-final-2026-10-10.json`,
`reforma-mundo-ui-sanitizadores-final-2026-10-10.json` e no manifesto da
entrega. Integridade/FK, identidade de origem e hashes são conferidos ao selar.

## Próxima frente

Conhecimento situado e visões adicionais salvas, conforme a ordem já definida.
Só começa após novo aceite humano/autorização. Aliases descritivos não entregam
essa frente antecipadamente. O laboratório continua separado e sem adoção
automática de seu estado volátil.
