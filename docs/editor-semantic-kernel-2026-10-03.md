# Editor semântico — antecipação autorizada, kernel e papéis narrativos

Autorização de 2026-10-03: priorizar novas funcionalidades/camadas semânticas,
adiando retenção/exportação controlada do journal, IME e sessão longa. Isso
altera a ordem de execução, não aprova o incremento 2 nem encerra a Etapa 1.
Predecessor de recuperação congelado; candidato separado em
`work/editor-semantico/kernel/inde`, branch
`codex/editor-semantic-kernel-2026-10-03`. Base HEAD f155bd8 + cumulativo
61c36642f877b0288575784b5ec814e65513ddcf509e08250ee645df3870cdf9.

## Corte e autoridade

Antecipar kernel da Etapa 3 e papéis da Etapa 4, incorporando somente as
dependências de ancoragem necessárias da Etapa 2. Não declarar parágrafos,
estilos nomeados, composição, blocos/variantes, IA ou etapas inteiras entregues.
Portar seleção, papéis, atributos, sobreposição, referências, diagnóstico e
lentes do gen7 como C++ nativo, não executar Tkinter/Python no produto.

O texto permanece `documents.content`. Formatação, âncora, anotação, relação e
entidade são distintos. UUIDs e entidades canônicas do Planejamento, nenhuma
acepção/match/binding silencioso. Modelos declarativos versionados namespaced;
tipo desconhecido conserva payload e informa indisponibilidade. Toda mudança
semântica participa de undo/redo, autosave, comparação otimista e recuperação.

## Entregas verificáveis

- cinco tipos: narração, descrição, diálogo, pensamento e passagem marcada;
- campos tipados, enums/cardinalidade, origem e diagnóstico de incompletude;
- kernel de âncoras independente de GTK, gravidade, fingerprints e estados
  resolvida/ambígua/órfã, reparo somente confirmado por nova seleção;
- sobreposições preservadas e escolha explícita no inspetor;
- criação/edição/arquivo/remoção, lentes desligáveis e navegação;
- relações explícitas entre anotações e referências a entidades existentes;
- registry de tipos pessoais imutáveis por versão, sem executar extensões;
- “encontrar usos” paginado por Documento/Projeto/Obra/papel/entidade/categoria;
- migração aditiva schema 16, rollback e FKs de identidade; não migrar originais;
- codecs versionados, compatibilidade de checkpoints antigos e cópia de resgate
  conservadora sem importar novos vínculos; isso é dependência, não retenção;
- testes domain/application/SQLite/GTK nos dois Projetos e evidência rastreável.

Não coletar/expandir catálogos. Importador gen7 e HTML derivados podem permanecer
um adaptador experimental separado se não couberem com segurança; registrar
precisamente sua cobertura, sem alegar portabilidade plena do protótipo.

## Gates

Nova ordem autoriza implementação antecipada, não validação humana. Registrar
adiamentos e observações sem reescrever artefatos aceitos/congelados. Ao terminar,
entregar candidato para avaliação e manter aceites pendentes. Configuração
recomendada: GPT-6.1 Sol/high, custo alto, mesma tarefa por migração/integridade.
