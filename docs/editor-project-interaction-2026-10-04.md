# Editor conectado ao Projeto — guia de revisão

O corte está no editor principal e conserva a barra aprovada. Clique direito
na seleção abre a seção **Texto e Projeto** do menu nativo. As mesmas ações
estão acessíveis em **⌘**, sem exigir saída do Documento. **Camadas Semânticas**
tem acesso à distribuição e **Entidades** inclui usos por marcações semânticas.

## Tarefas reais

1. Em Projeto vazio, escreva `Marcio chegou ao porto.`. Selecione `Marcio`,
   clique direito e **Adicionar entidade à seleção…**. Escolha Personagem,
   preencha um resumo e confirme. O texto permanece igual; a ocorrência aparece
   nas camadas. O Personagem passa a existir no Planejamento. Cancelar não cria.
2. Repita com outro trecho e marque **Criar um novo tipo de entidade nesta
   operação**. Informe nome, chave simples (minúsculas/números/hífen) e descrição.
   Tipo, entidade e vínculo são gravados juntos, ou nenhum deles é gravado.
3. Para reutilizar uma identidade, use **Vincular entidade existente…**. O
   seletor pesquisa nomes/resumos e mostra tipos. Nomes iguais não são fundidos
   nem vinculados automaticamente; sempre escolha o resultado desejado.
4. Selecione uma ocorrência já vinculada e use **Adicionar relação à entidade
   do trecho…**. Escolha explicitamente a entidade do trecho, a outra entidade,
   tipo e sentido. A prévia indica origem e destino. Também é possível criar
   um tipo de relação no formulário, incluindo sentido inverso ou simetria.
   Confirmação cria uma relação canônica; o texto não é prova automática do fato.
5. Selecione uma frase e **Associar materialização / informação…**. Escolha um
   Acontecimento, uma Informação ou outro tipo; escolha papel e nota. Materialização,
   evidência, menção e contraponto são associações declaradas pelo escritor.
   Não criam datas, ocorrências temporais ou atualização de fatos canônicos.
   Também pode criar nova entidade e marcar a opção de materialização no mesmo fluxo.
6. Marque trechos como Narração/Descrição/Diálogo/Pensamento usando **Marcar
   Seleção**. Preencha campos humanos. **Distribuição semântica** mostra a união
   de caracteres por categoria, sem marcação e sobreposições. Escopo é seleção,
   ou Documento inteiro se não houver seleção. Há botão para o Documento inteiro.
7. Digite `@Marcio` ou `@personagem:Mar`, `@informacao:causa`,
   `@acontecimento:morte`, `@local:porto`, `@entidade:termo`.
   Popup mostra até seis resultados do Projeto. Escolher abre uma prévia do
   nome/resumo e relações. Escolha nome ou informação, com ou sem associação.
   A confirmação substitui só o comando, num único desfazer/refazer.

## O que avaliar

- Sei qual trecho será associado antes de confirmar? O intervalo e a prévia
  são legíveis? Tipos e nomes iguais são distinguíveis?
- Consigo criar/reutilizar a entidade sem sair do editor, e encontrá-la no
  Planejamento e nos usos textuais após salvar/reabrir?
- Consigo prever o sentido da relação? Entendo que é cadastro do Projeto,
  diferente de uma relação entre marcações ou de uma associação textual?
- Entendo materialização/evidência sem interpretar a associação como fato
  automaticamente confirmado? A nota explica a intenção do escritor?
- Consigo marcar, analisar e revisar os papéis sem que o sistema classifique
  texto por conta própria? Sem marcação não significa necessariamente narração.
- O popup encontra dados úteis sem interromper a escrita, e deixa claro o que
  será inserido/vinculado? Cancelamento não deve apagar o comando digitado.
- Desfazer remove associação/inserção, mas mantém cadastros criados no Projeto?
  A interface explica isso sem surpresa? Excluir cadastro exige o Planejamento.
- Ao substituir um trecho inteiro, sua associação fica ambígua e não é usada
  para criar relações sem reancoragem humana? As camadas permitem recuperar?

## Limites explícitos

Análise usa marcações manuais, caracteres Unicode com espaços, não NLP,
classificador automático ou avaliação literária. Percentuais por categoria
podem se sobrepor; cobertura global é união. Arquivadas são excluídas; campos
incompletos e âncoras não resolvidas são sinalizados. Pendências e globais são
contagens do Documento inteiro mesmo quando o escopo de cobertura é seleção.
Relações canônicas não recebem percentual textual artificial. A contagem
incidente é limitada e declara quando há truncamento; Planejamento explora o todo.

Popup usa busca literal existente de entidades (nome/resumo), com ranking e
limite, não motor linguístico de aproximação nem busca por todo campo futuro.
SQLite NOCASE/LIKE não é prometido como casefold Unicode completo. Resumo
inserido é snapshot de texto, não transclusão viva; a associação conserva a
identidade da entidade, mas não reescreve frases quando seu resumo muda.

Novas ocorrências usam âncoras semânticas com fingerprint. Vínculos nativos
legados continuam com a geometria anterior e não são convertidos silenciosamente.
Corpos, formatação, tipos autorais desconhecidos e relações anteriores são
preservados. Não existe inferência de entidade por seleção ou leitura de palavra.
Não há migração nova, calendário, binding lexical, mudança de backend, nova
persistência de montagem ou exportador nesta entrega.

Revisão técnica e hashes: outputs/editor-interacao-* no workspace de entrega.
Registrar aceitar, pedir ajustes ou rejeitar/adiar. Teste técnico não substitui
aceite humano e não certifica IBus real, 45 minutos ou latência percebida no desktop.
