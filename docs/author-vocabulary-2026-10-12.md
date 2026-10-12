# Vocabulário autoral — aplicação de 2026-10-12

## Autoridade e escopo

O usuário aceitou o bloco anterior e aprovou explicitamente todas as propostas
da auditoria de vocabulário de 2026-10-12 neste chat. Esta implementação aplica
a linguagem e as simplificações de interação daquela auditoria ao candidato
privado modificado. Não autoriza publicação, migração de dados autorais,
renomeação de nomes personalizados nem aprovação retroativa deste executável.

Base: `995fbb27f8ee9359643a90f0220bc71269ef5e9a`, branch
`codex/reforma-fluxo-temporal-2026-10-10`, acrescida do trabalho privado aceito.
O pacote de fonte de realização/continuidade de 2026-10-11 é a referência de
preservação; não foi substituído. O schema permanece **28**.

## Contrato de linguagem

| Área | Linguagem da interface | Distinção preservada |
|---|---|---|
| Catálogo | Obras e universos; Conjunto de obras para não ficção | Agrupamento não é registro jurídico nem série obrigatória |
| Mundo da história | Personagens e mundo; Elemento da história; Categoria | A ficha não é necessariamente personagem nem um fato verdadeiro |
| Relações | De/para, sentido contrário, vale nos dois sentidos | Trocar palavras não inverte a direção da relação |
| Planejamento | Plano narrativo; Parte da narrativa; Linha narrativa | Uma parte não é obrigatoriamente cena ou capítulo |
| Quatro ordens | Ordem do plano narrativo, Cronologia da história, Ordem de escrita, Ordem do manuscrito | Não se sincronizam nem se inferem por posição |
| Texto | Trechos marcados; Marcações do texto; Realçar marcações | Realce de ferramenta não é formatação autoral |
| Ponto de vista | Do documento, deste trecho e planejado | Pessoa da narração, focalização e percepção continuam distintas |
| Texto e plano | Texto que desenvolve esta parte; Conferir texto e planejamento | Associar não copia; conferir não aprova a obra |
| Continuidade | Escolha intencional do autor | Avisos não bloqueiam escrita ou viagem no tempo |
| Montagem | Montagem de texto; Uso do bloco; Versão em uso | Rascunho salvo não entra automaticamente na leitura |
| Tempo | Cronologia; Linha do tempo; Calendários e relógios; Data ou período | Linha temporal não é linha narrativa; conversão visual não altera origem |
| Mapas | Lugares e mapas; Nível de detalhe; Visual em pixels; Longitude/Latitude | Nenhuma promessa nova de fusos ou mapas históricos |
| Manuscrito | Organização do manuscrito; Elemento do manuscrito | Organização editorial não é o plano narrativo |
| Exportação | Preparar para exportação | Exportar não significa publicar |
| Segurança | Excluir, Retirar, Desvincular; Onde este item é usado | O verbo indica se apaga o item, sua inclusão ou sua associação |
| Recuperação | Ponto de recuperação; Registro de recuperação | Contexto salvo não é backup nem versão aprovada |
| Dicionário | Sentido da palavra; Fontes e detalhes da revisão | A consulta não escolhe um sentido nem cria associações |

A auditoria original continua sendo o inventário detalhado das famílias de
termos e dos sinônimos. Esta tabela resume os contratos, não os substitui.

## Simplificações concretas

1. **Categorias e relações pessoais:** o identificador é gerado ao abrir o
   formulário. Nome, descrição e sentido da relação são os campos principais.
   O identificador editável fica em uma seção técnica opcional; editar um nome
   não altera sua chave. Falhas conservam o formulário e seu preenchimento.
2. **Marcações pessoais:** formulário com nome, descrição e até 32 campos.
   Campos podem ser texto, inteiro, sim/não, ficha ou lista de opções. Escolhas
   de obrigatório e múltiplos valores permanecem explícitas. Opções vazias ou
   repetidas são recusadas. A definição só é registrada com confirmação.
3. **Editor técnico:** JSON continua disponível sob demanda. Sua utilização
   exige uma escolha explícita. Gerar uma definição não registra a marcação;
   alternar modos não sobrescreve o rascunho técnico já preenchido.
4. **Não ficção:** em Obras → Filtros, o autor escolhe como chamar o agrupamento.
   A preferência atualiza catálogo, navegação e criação de obra. Não renomeia
   o universo cadastrado, não muda relações e não acrescenta etapa obrigatória.
5. **Glossário:** Ajuda → Vocabulário do INDE pesquisa nomes novos e antigos.
   As explicações esclarecem consequências, compartilhamento e limites. Não
   é outro painel permanente. Marcações existentes também conservam o nome
   antigo como parte do texto pesquisável.
6. **Dicionário:** definição, classe e estado de revisão permanecem visíveis.
   Fontes, identificadores, evidências, pontuações e bloqueios técnicos ficam
   em seção expansível. Dados experimentais e revisão pendente não são ocultos
   nem apresentados como aprovação linguística.
7. **Erros e exclusão:** diagnósticos conhecidos são apresentados por
   correspondência exata, com orientação concreta. Diagnósticos desconhecidos
   permanecem íntegros. Efeitos e títulos de exclusão usam o objeto real; nomes
   autorais e registros históricos permanecem preservados.

## Proteções e ausência de migração destrutiva

- Chaves internas, ações, tabelas, JSON, tipos e campos persistidos não foram
  renomeados. Não há migração de schema para esta aplicação.
- Adaptadores de apresentação mudam apenas rótulos de definições do INDE;
  nomes de marcações e campos pessoais são exibidos literalmente.
- `Local` nativo aparece como `Lugar`; uma categoria pessoal chamada `Local`
  continua chamada `Local`. Nome não concede capacidade reservada de tipo.
- Os textos de origem, versões em montagem, datas, presenças e regras entre
  partes mantêm suas autoridades e estados. Não há interpretação automática.
- Nenhum projeto autoral ou cópia congelada foi usado para testes de escrita.
  Os testes gráficos operam em novas cópias, com configuração isolada.

## Evidência técnica e limite de aceite

O manifesto e o relatório de entrega da tarefa identificam os binários,
fontes, logs e resultados finais. Incluem o teste novo de vocabulário,
regressões, verificação de integridade, Debug/ASan/UBSan **focais**, tarefas GTK
em cópias de projeto vazio/Atlas, temas claro/escuro e startup dos executáveis.
Resultados históricos não se transferem a este hash. Instrumentação focal
não é compilação Debug ou sanitizer integral de todo o programa.

Os ensaios automáticos não certificam preferência de escritores, leitores de
tela, IBus, HiDPI nem sessão longa. A aprovação das propostas autoriza esta
implementação; **o aceite humano desta nova entrega permanece pendente**.

## Roteiro humano da nova entrega

1. Em projeto vazio, comece pelo planejamento e crie uma obra. Explique com
   suas próprias palavras o que é obra e o que é agrupamento.
2. Cadastre personagem e lugar, depois uma categoria pessoal, sem abrir o
   identificador técnico. Crie relação e confira os dois sentidos.
3. Crie um plano e mova uma parte em cada ordem permitida. Confira que não
   mudou a cronologia nem a organização do manuscrito.
4. Em um documento longo, marque trecho, declare ponto de vista e associe data.
   Distinga menção, lembrança e presença; corrija um trecho não localizado.
5. Crie marcação pessoal com lista de opções. Provoque opção duplicada,
   corrija e confirme; cancele outro formulário sem perder o trabalho anterior.
6. Consulte palavra selecionada. Identifique definição, fonte, estado de
   revisão e base experimental sem interpretar pontuação como certeza.
7. Na montagem, explique a diferença entre salvar rascunho, usar versão,
   repetir bloco e criar cópia independente. Retire apenas um uso.
8. Tente excluir item usado; siga Onde este item é usado, retire associações
   escolhidas e retorne. Confira o efeito antes de confirmar.
9. Repita tarefas nos temas claro/escuro, com teclado e leitor de tela.
   Verifique quebra de linha, foco, rolagem, botões do rodapé e retorno ao texto.
10. Em não ficção, escolha Conjunto de obras e reabra. Os nomes dos trabalhos,
    fichas, textos e vínculos devem permanecer exatamente como foram criados.
