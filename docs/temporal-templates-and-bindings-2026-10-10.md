# Calendários reais e vínculos de datas — segundo corte temporal

Base humana aceita: `3f7eae22ff6e230d4ef4030d99d05e75e15d5f23`.
Escopo autorizado: ampliar seis modelos, vincular datas a quatro classes
canônicas e apresentar oportunidades. Este corte precisa de novo aceite;
não é publicação nem encerramento do sistema temporal ou da reforma.

## Variantes implementadas

| Modelo | Regra e coordenada de ano | Limite deliberado |
|---|---|---|
| Chinês | ICU lunissolar; ano gregoriano associado ao começo do ano lunar; meses em ordem, intercalar nomeado | 1901–2100; cálculo pode divergir de observação/tabela local em um dia |
| Judaico | Hebraico calculado, AM, Tishri primeiro; Adar I/II e meses variáveis | 1–9999; não calcula pôr do sol/observância |
| Islâmico | Civil tabular, Hégira; intercalação de 30 anos | 1–9999; não representa observação lunar/Umm al-Qura |
| Egípcio antigo | Civil de 365 dias; 12×30 e cinco dias epagomenais como período separado | Ano autoral, alinhamento manual; sem reinado/época absoluta inventada |
| Maia | Haab: 18×20 e Wayeb×5, dia zero; correlação GMT 584283 | Ciclos Haab sintéticos ancorados: ciclo 0 começa 348 dias antes da origem da Contagem Longa; não são anos de uma era histórica |
| Etíope | Amete Mihret; 12×30 e Pagumen×5/6 | 1–9999; não é Amete Alem |

Data maia completa apresenta Haab, Tzolkin e Contagem Longa; esses dois últimos
são leituras derivadas, não campos autorais independentes nem um conversor de
entrada de Contagem Longa. Datas anteriores à origem não recebem Contagem
Longa negativa inventada. Correlações alternativas continuam devidas.

Os quatro motores ICU guardam `icu-74.2/inde-day-v1` neste build. Versão diferente
falha explicitamente, sem reinterpretar calendário salvo. Uma futura migração
de motor deve mostrar diferenças e criar revisão; não foi presumida neste corte.
ICU 74+ é dependência de compilação/runtime, não uma dependência remota: tudo
funciona localmente. Payload v3 contém proveniência; v1/v2 continuam lidos sem
reescrita. Os ordinais dos novos motores ICU usam dias julianos inteiros (JDN);
o alinhamento do eixo continua explícito, sem mudar ordinais já existentes.

No chinês e hebraico, o índice de mês é a posição naquele ano, não um número
fixo de mês cultural: a escolha mostra o nome. Mudar ano/sistema limpa a escolha
de mês no formulário de referência, evitando transportar silenciosamente Adar
II ou um mês intercalar para outro ano. A janela mensal e o novo ponto por data
também recebem os meses do ano, e o Haab inclui o dia zero.

Os relógios continuam regulares e configuráveis. O padrão 24/60/60 é uma
convenção de trabalho, não alegação de horas históricas egípcias, início
religioso do dia ou fusos. A extração interna ICU em UTC não torna a data autoral
um instante UTC. Astronomia completa, horários sazonais e mapas não fazem parte
deste corte.

## Uso sem novos painéis permanentes

1. **Planejar → Tempo → Usar modelo real…**. Escolher variante, conferir ano e
   meses calculados, nomear e salvar. Salvar não aplica calendário ao eixo.
2. **Tempo → Uso, mudanças e datas… → Datas e horários — [eixo]**.
   Adicionar referência, escolher sistema e mês pelo nome. Ano/mês parciais e
   hora desconhecida permanecem válidos, sem meia-noite inventada.
3. Abrir uma data e **Vínculos desta data…**. Escolher tipo de elemento,
   buscar, selecionar resultado e escrever **Significado da data**. Exemplos
   ilustrativos: nascimento, início, fim, ocorrência, menção. Nenhum é escolhido
   pelo programa. Confirmar vínculo.
4. **Tempo → Consultar datas de um elemento…** faz a consulta inversa e permite
   abrir a referência. Os papéis são mostrados junto à data.

Uma página contextual rolável, com ação principal fora da rolagem. Detalhes
das regras recolhidos no formulário de referência. Não adiciona mapa, painel
de grafo ou propriedades permanentes à área de escrita.

Entidade = `entities`; relação = `relations` (entre entidades);
unidade/linha = `narrative_units` / `narrative_lines`, não seções editoriais ou
arestas entre unidades. Relações preservam seus qualificadores anteriores;
vínculos novos não substituem `relation_contexts`, não criam pontos cronológicos,
não ordenam unidades nem inferem causalidade. Derivações futuras precisam de
contrato separado.

## Integridade e reversibilidade

Schema 24 aditivo. Cada vínculo possui identidade/token, referência, exatamente
um destino, papel e nota. Chaves estrangeiras compostas protegem o proprietário;
remoção de referência/destino é restrita enquanto houver vínculo. Inclusive
excluir uma estrutura com unidades/linhas vinculadas não apaga datas por cascata.
Duplicar o mesmo vínculo/papel é rejeitado. Remover vínculo não remove a data.

Novo vínculo confere a referência completa e o snapshot do destino em transação.
Editar uma referência confere também seus vínculos; a UI mostra impacto e exige
confirmação quando compartilhada. Novos vínculos adicionados enquanto a edição
estava aberta invalidam a confirmação antiga. Captura de operações e Save As
incluem a nova tabela. Não há materialização no grafo.

Limites: 64 versões/project, 500 referências/eixo, 500 vínculos por referência
ou destino. Busca admite até 100 resultados; exceder exige refinar, não escolhe
nem trunca silenciosamente. Busca SQLite usa casefold ASCII; nomes não ASCII
mantêm distinções de caixa/acentos. Ainda não é pesquisa linguística universal.

## Fontes e vetores independentes

- [ICU: serviços de calendário](https://unicode-org.github.io/icu/userguide/datetime/calendar/).
- [Unicode CLDR: identificação de variantes](https://github.com/unicode-org/cldr/blob/main/common/bcp47/calendar.xml).
- [Hong Kong Observatory: tabelas gregoriano-lunares](https://www.hko.gov.hk/en/gts/time/conversion.htm)
  e [tabela de 2023](https://www.hko.gov.hk/en/gts/time/calendar/text/files/T2023e.txt):
  ano lunar começa em 22/01, mês 2 intercalar começa em 22/03.
- [Metropolitan Museum: tempo no Egito antigo](https://www.metmuseum.org/de/essays/telling-time-in-ancient-egypt):
  calendário civil de 365 dias, cinco dias fora dos meses.
- [Smithsonian NMAI: sistema maia](https://maya.nmai.si.edu/calendar/calendar-system):
  estrutura dos ciclos; vetor de 01/01/2000 = 12.19.6.15.2, 11 Ik’, 10 Kankin.

Testes de round trip não são prova histórica. Vetores independentes complementam
a consistência algorítmica; não certificam todos os anos/culturas/observâncias.
Verificação técnica, capturas e relatório final constam na entrega própria.

Gate técnico final: CTest 24/24; 10.495 round trips no teste novo; núcleo focal
ASan/UBSan (oito unidades), interface nova 8/8 Release e 8/8 focal ASan/UBSan
(quatro unidades UI), regressão anterior 8/8. Revisão visual do agente em
14 capturas selecionadas, claro/escuro e Atlas/vazio. Dependências Release e
LSan desligado nos testes focais; não certifica toda a aplicação, HiDPI,
tecnologia assistiva ou sessões longas. Novo aceite humano continua pendente.

O teste gráfico instrumentado detectou uso de widget já destruído no evento
de seleção da lista durante reconstrução da página de vínculos. Os handlers
agora acompanham a vida de todos os widgets que usam, incluindo o rótulo de
seleção e o status. O fluxo de criar os quatro vínculos, consultar, editar,
remover com confirmação, reverter e reabrir é exercitado após essa correção.

## Oportunidades propostas, não implementadas

1. **Intervalos e incerteza autoral** — antes/depois, entre, aproximadamente,
   conhecido só por estação. Resolver o incentivo a inventar dia exato para
   conseguir consultar. Primeiro desenhar a precisão e a exibição de limites;
   não reduzir tudo a um timestamp. Critério: incerteza visível também na exportação.
2. **Comparação de história e ordem de apresentação** — consultar as datas das
   unidades sem modificar sua posição editorial. Útil para flashback, múltiplas
   linhas e revelações. Deve haver uma única vista alternável, não novos painéis.
3. **Consultas de continuidade com explicação** — nascimento posterior à cena,
   relação terminada antes de uma ocorrência, viagem com intervalo insuficiente.
   Requer papéis formalizados pelo autor, intervalos e duração mínima declarada;
   alerta é hipótese revisável, nunca correção automática.
4. **Atalho contextual Datas nos quatro elementos** — abrir o mesmo editor
   a partir da ficha/relação/unidade/linha. A consulta inversa já funciona na
   biblioteca; falta reduzir a distância de navegação sem duplicar interfaces.
5. **Visão mensal por regime de adoção** — a lente mensal ainda usa calendário
   base do eixo. Acrescentar escolha de regime vigente/paralelo, com cortes
   visíveis, preservando a opção de calendário de origem.
6. **Fontes, eras e correlações alternativas** — guardar evidência da data,
   opinião de cronista e numeração por reinado; permitir comparar correlações
   maias sem substituir silenciosamente uma data. Complementa incerteza, não
   cria consenso histórico artificial.
7. **Ciclos recorrentes, festivais e sazonalidade** — ocorrências por regras do
   calendário, incluindo intercalação. Criar agenda cultural local opt-in;
   evitar que um template religioso venha com feriados presumidamente universais.
8. **Início cultural do dia e futuro mapa temporal** — pôr do sol/alvorada,
   zonas e mudanças históricas pedem local, astronomia e regra regional.
   Manter para a frente posterior de mapa já definida pelo usuário; não basta
   aplicar um deslocamento fixo em horas.

Minha ordem recomendada: intervalos/incerteza → atalhos contextuais → consulta
história/apresentação → alertas explicáveis. O mapa continua adiado. Estas
prioridades são propostas de produto, não autorização para executar o próximo
corte nem promessa de prazo.
