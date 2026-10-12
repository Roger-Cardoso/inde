# Tempo no texto — fundação da expansão aceita

Base humana aceita: `a8a75a7`. Continuação privada, schema 24 → 25 aditivo.
Este incremento não conclui todas as propostas aceitas nem a reforma do INDE.
O plano autorizado continua em `temporal-context-expansion-2026-10-10.md`.
O reconhecimento de entidades permanece por último e não está implementado aqui.

## Uso e interface

Em **Escrever**, selecionar um trecho e usar **Marcar tempo no trecho…** no
menu de contexto. A anotação **Tempo do trecho** aceita presença explicitamente
declarada, contexto excepcional e explicação; estes campos são opcionais. Depois,
escolher eixo, referência existente na biblioteca de Tempo e papel do vínculo.
Não existe escolha automática de data, de presença ou de identidade.

Qualquer anotação semântica não editorial, inclusive **Perspectiva situada** e
**Perspectiva do Documento**, oferece **Vincular data / horário…** em seu detalhe.
O vínculo local usa a âncora existente; a perspectiva global não fabrica uma
âncora cobrindo todo o texto. O corpo da escrita não ganha etiquetas nem links.
Confirmar devolve o foco ao editor; cancelar não cria o vínculo.

Os papéis explícitos são **Cena**, **Menção**, **Memória** e **Referência**.
Não são inferidos do texto livre. Há múltiplas referências por anotação; vínculos
duplicados de mesma data/papel são recusados. A remoção do vínculo não remove a
referência da biblioteca. Vínculo e remoção participam do desfazer/refazer textual.

O destaque semântico existente continua opcional. Quando ativado, a consulta
do trecho inclui até duas datas e indica outras disponíveis no detalhe.
Não há painel temporal permanente, popup disparado por digitação ou marcação
obrigatória para escrever. Datas não substituem os pontos ordinais ficcionais
antigos: ordem de acontecimentos e calendário permanecem conceitos diferentes.

## Precisão sem inventar instantes

No editor da referência, **Precisão, intervalo e incerteza** fica recolhido por
padrão. Oferece data informada, antes, depois, entre limites, aproximadamente e
estação/época. Os dois últimos exigem uma explicação e não fabricam uma margem.
O limite final do intervalo usa o mesmo calendário e relógio do inicial.

Ano sem mês cobre o ano; mês sem dia cobre o mês; dia sem hora cobre o dia;
hora parcial cobre a unidade declarada. “Antes” e “depois” excluem todo o período
informado. Os limites informados do intervalo são inclusivos; internamente, o
fim exclusivo da unidade torna as comparações conservadoras.
Relógios fracionários são comparados por coordenadas racionais exatas, sem
arredondamento de ponto flutuante ou multiplicação com risco de overflow.

Uma referência compartilhada mostra a quantidade de vínculos e uma lista
recolhível dos documentos que a consultam. Atualizar exige reconhecer o impacto.
A transação também confere se os vínculos/documentos mudaram desde a prévia.
Para outra data, criar referência independente. Não há cópia automática de uma
data em cada trecho, nem atualização silenciosa dos trechos que a consultam.

## Avisos, não erros narrativos

**Camadas Semânticas → Consultar avisos temporais do Documento** é uma consulta
sob demanda, incluindo a edição atual ainda não salva. A primeira regra verifica
presença declarada antes de nascimento declarado formalmente:

1. Uma anotação ativa **Tempo do trecho**, com âncora resolvida, declara entidades
   presentes e exatamente uma referência com papel **Cena**.
2. A entidade possui exatamente um vínculo com papel formal **Nascimento** na
   biblioteca. Escrever “nascimento” no significado livre não basta.
3. Ambas as referências estão no mesmo eixo; se usam calendários diferentes,
   devem ter alinhamento inequívoco já declarado.
4. Todo o intervalo possível da cena antecede todo o intervalo possível do
   nascimento. Se houver dúvida, nenhuma conclusão automática é emitida.

O aviso mostra entidade, referências e datas que sustentam a comparação.
**Viagem no tempo**, **Memória**, **Profecia**, **Narrador não confiável** e outro
contexto autoral podem ser declarados. A exceção é reconhecida no aviso; não
transforma o texto em erro, não some com as fontes e não bloqueia salvar/exportar.
Uma menina em 1990 com nascimento em 2000 pode representar viagem no tempo.
Uma simples menção a essa menina em 1990 não é tratada como presença.

A consulta não escolhe entre vários nascimentos ou tempos concorrentes, não
trata focalizador como pessoa presente e não compara aproximações sem limites.
Ausência de aviso não certifica a continuidade da obra. O teto de 200 avisos é
informado; consultas de morte, deslocamento, duração e relações são frentes
posteriores. Erros de identidade/FK/payload continuam sendo erros de integridade,
distintos de liberdade ficcional.

## Persistência, história e portabilidade

As extensões de precisão ficam em tabela suplementar; referências anteriores
continuam exatas sem fabricação de novas linhas na migração. Os vínculos de
data do texto pertencem ao payload semântico v3 e possuem projeção normalizada
para integridade e consulta reversa. Documentos sem datas continuam escritos
em v2; v1 e v2 permanecem legíveis.

Salvar como preserva os vínculos e transfere a propriedade ao novo projeto.
Distribuição de blocos dentro do projeto preserva a referência e gera novas
identidades de vínculo por ocorrência. O clipboard nativo já transporta texto
e formatação, não todo o grafo semântico; não promete copiar contexto temporal.
Revisões editoriais independentes e recuperação com política de cópia textual
mantêm o limite existente de não duplicar semântica. Não tratar estas operações
como backup completo das marcações.

Excluir referência consultada por documentos é recusado; remover primeiro os
vínculos é decisão autoral separada. Edição de referência integra o histórico
reversível do projeto; edição textual integra o histórico do editor. Âncoras
perdidas exigem revisão humana, nunca reatribuição a um trecho parecido.
Schema 25 não deve ser aberto para escrita por binários antigos de schema 24.

## Continuação já autorizada, ainda pendente

- Atalhos de criação/consulta de datas nos elementos e no fluxo da escrita.
- Comparação entre cronologia da história e ordem de apresentação/leitura.
- Ampliação das regras de continuidade, diagnóstico dos casos não comparáveis
  e contexto temporal efetivo em sobreposições, sempre sem vencedor silencioso.
- Lente mensal por adoção/coexistência/aposentadoria.
- Eras, proveniência de datas, correlações alternativas e festas recorrentes
  opt-in; não presumir variantes religiosas ou políticas históricas regionais.
- Início cultural do dia e integração temporal/regional do mapa. A reforma
  cartográfica técnica/gráfica e mapas-múndi reais continuam etapas separadas.
- Por último: sugestões discretas de entidade por ocorrência, confirmação
  humana, modificador + clique e retorno à posição de escrita salva.

Verificação técnica: CTest 25/25; quatro matrizes GTK 8/8 (nova Release, nova
focal ASan/UBSan, templates e adaptação anterior); núcleo focal ASan/UBSan em
16 unidades, UI focal em cinco; 12 capturas selecionadas revisadas pelo agente.
O comparador racional tem 36.100 pares de frações pequenos verificados, além de
casos no limite de 10^12 subdivisões, dias negativos e entradas inválidas.
Salvar como, reversão da precisão compartilhada, reabertura, rollback e
distribuição em duas ocorrências são cobertos. As cópias dos testes são isoladas.
As evidências e hashes ficam no manifesto separado da entrega. Dependências
restantes estão em Release, LSan desativado; a distribuição é testada em Release.
Testes e capturas de interface não substituem o próximo aceite humano.
