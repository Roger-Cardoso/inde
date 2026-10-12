# Adaptação temporal — primeiro corte

Registro histórico do primeiro corte, agora aceito pelo usuário no commit
`3f7eae2`. A ampliação vigente está em
[calendários e vínculos — segundo corte](temporal-templates-and-bindings-2026-10-10.md).
Os limites e números abaixo descrevem a entrega anterior, não a nova candidata.

A forma do calendário do oitavo bloco foi rejeitada pelo usuário. Este corte
adapta aquela base, não substitui o parecer humano por um teste técnico. Base
privada: `69cfcaf585678abac49f66cdef381bf377e416e8`; schema 22 → 23 aditivo.
Não é release, conclusão da reforma ou conclusão do sistema temporal total.

## Uso

Em **Planejar → Tempo**, a biblioteca aparece sem exigir um eixo selecionado.
Escolha **Usar modelo real…** ou **Criar sistema próprio…**. O atalho antigo
“Criar definição de calendário…” agora leva ao mesmo editor, sem uma segunda
interface de cadastro. O formulário antigo de meses `Nome | dias` foi removido.

O editor tem duas abas, Calendário e Relógio e unidades. Meses, dias da semana
e subdivisões do dia usam listas com seleção, adicionar, atualizar, remover
e ordenar. Os campos editam um rascunho; alterações de item não são gravações
no Projeto. Salvar valida o conjunto e cria uma versão persistente. “Cancelar
edição” retorna sem salvar. Não há persistência de rascunho entre sessões.

Editar uma versão existente cria uma revisão, com referência à versão de
origem. Nunca atualiza o calendário anterior ou reaplica regras aos fatos.
Aplicar a revisão ao eixo é outra operação. A biblioteca exibe versões
salvas; o limite atual de 64 inclui essas revisões.

**Uso, mudanças e datas…** reúne os eixos existentes:

- **Adoções**: declarar o sistema inicial exige reconhecer explicitamente
  uma unidade do eixo como um dia e conferir a equivalência de um marco.
  Não se deduz essa medida de ordinais relativos. Uma adoção posterior exige
  data do corte, sistema sucessor, equivalência e escolha de modo.
- **Substituir e aposentar** interrompe as apresentações paralelas dos
  antecessores a partir do corte, mas não apaga fatos ou datas anteriores.
- **Adotar e coexistir** mantém apresentações paralelas após o corte.
- **Conferir datas paralelas** consulta um marco já existente e mostra
  as apresentações válidas na sua data, sem duplicar acontecimentos.
- **Datas e horários** salva referências autorais na versão escolhida,
  com ano, mês/dia opcionais e horário opcional, respeitando precisão.

No formulário **Novo ponto**, é possível escolher a origem ou um sucessor.
O calendário precisa estar vigente na data escolhida; a transação confere
essa vigência e o token da adoção. Uma mudança enquanto o formulário está
aberto não cria um marco sob regras diferentes das apresentadas.

A consulta mensal antiga continua sendo uma lente do calendário de origem;
não é ainda um navegador mensal de todos os regimes históricos. Para vigência
e coexistência, usar a consulta de datas paralelas. Sua substituição por uma
consulta mensal com seleção de regime é débito explícito, não funcionalidade
concluída nesta entrega.

## Calendários e relógios

Os dois modelos reais iniciais são **Gregoriano proléptico** e **Juliano
proléptico**. Proléptico significa aplicar as regras também antes da adoção
histórica. Usam numeração astronômica com ano zero; não presumem reformas
regionais nem pretendem representar todos os calendários históricos.

O Gregoriano usa divisibilidade por 4, exceto séculos não divisíveis por 400;
o Juliano usa divisibilidade por 4. A equivalência usa um número inteiro de
dia terrestre, não um instante UTC com hora. O teste de 1582 compara 5 de
outubro juliano e 15 de outubro gregoriano como o mesmo dia. Referências:
[USNO: Julian Date](https://aa.usno.navy.mil/data/JulianDate) e
[USNO: fórmulas de calendário](https://aa.usno.navy.mil/faq/JD_formula).

Nomes são editáveis; alterar as durações de um modelo real exige escolher
motor próprio. Esse motor ainda oferece apenas um dia intercalar por ciclo
simples. Não substituir as exceções gregorianas por “a cada quatro anos”.

O relógio é uma cadeia regular de subdivisões do dia. Exemplos: 24 horas/dia,
60 minutos/hora, 60 segundos/minuto; ou 8 vigílias/dia, 12 pulsos/vigília.
Singular, plural, abreviação e quantidade são autorais. Hora vazia não é
meia-noite. Um horário parcial não vira automaticamente um instante completo.
Mês e ano não são durações fixas em segundos. Não há fuso presumido.

## Persistência e proteção

O payload de calendário v2 acrescenta motor e relógio. Payloads v1 continuam
legíveis sem regravação; recebem o relógio padrão somente na interpretação
em memória. O esquema acrescenta revisões, adoções e referências, sem
converter dados anteriores nem fabricar calendários ou datas na migração.

Operações participam do histórico reversível. Referências usam token para
recusar edições/remoções desatualizadas; adoções comparam o histórico inteiro
da prévia dentro da transação. Depois de adoções, a associação global antiga
não pode reescrever a origem do eixo. Save As transfere a propriedade das
novas tabelas e conserva suas relações. Exclusão de um eixo com dependências
temporais é recusada.

## Verificação e limites

`inde_temporal_system_tests` cobre motores, extremos positivos/negativos,
intercalação, relógios, precisão, payload legado, versões, adoções, conflitos,
vigência de novos pontos, reversão, restart, Save As, integridade e rollback.
O smoke GTK novo cobre a interface de produção em Atlas e projeto vazio,
temas claro/escuro e reabertura, com capturas do topo e fim dos formulários.
O smoke antigo foi atualizado: continua cobrindo aliases e uso mensal legado;
não simula a criação pelo formulário textual removido. O CRUD é verificado
pelo novo driver, e não atribuído ao teste antigo.

Sanitizadores são focais: sete unidades do núcleo e quatro de UI, mais testes;
dependências restantes em Release e LSan desativado. Não certificam toda a
aplicação. Capturas e fluxos automatizados não são aceite humano, validação
de leitor de tela, HiDPI ou sessão longa. Evidências finais e hashes estão no
manifesto separado da adaptação, não nos manifestos congelados anteriores.

Limites atuais: ±1.000.000 anos, 32 meses, 60 dias/mês, 16 nomes de semana,
1–8 subdivisões de relógio, produto até 10^12, 128 adoções/eixo e 500 referências
autorais/eixo. Os limites são recusas explícitas, não truncamento silencioso.

## Trabalho que continua devido

1. Regras intercalares gerais, eras, ciclos e modelos históricos/religiosos
   adicionais, cada qual com regras e proveniência próprias.
2. Integração das referências de data/hora com acontecimentos, viagens,
   Documentos e vistas derivadas; intervalos e incerteza sem precisão inventada.
3. Navegador mensal por regime, duração e conversão com precisão declarada.
4. Contextos culturais/regionais e zonas temporais. A ponte para o mapa não
   autoriza antecipar a reforma cartográfica.

Por decisão do usuário, reforma técnica/gráfica do mapa, mapas com mudanças
temporais e templates de mapas-múndi reais ficam para uma implementação futura.
Não foram alterados neste corte. Não avançar para “conhecimento situado”
deixando a adaptação temporal sem acompanhamento.
