# Consolidação do editor — ordem direta de 2026-10-03

Base: candidato UX preservado em `work/editor-semantico/ux-2026-10-03`;
HEAD-base f155bd866d32c9326e5853a2882e6adbfca6ba23. Novo corte independente,
sem commit, push, publicação ou edição no checkout original.

Ordem vinculante: (1) busca integrada a Camadas, fundo legível e tooltip;
(2) só então retenção/exportação controlada, composição IME e sessão longa.
Não antecipar novos tipos, heurísticas, vínculos ou abordagens semânticas.

## Contratos do corte

- Camadas mantém página, filtros e cartões; Busca é atalho para o escopo do
  Projeto. Documento consulta o estado local; Projeto consulta persistidos.
- Fundos translúcidos não alteram glifos nem formatação persistida. Tooltip
  identifica todos os papéis sobrepostos resolvidos e um campo útil por tipo.
- Retenção é decisão humana explícita, não tarefa automática. Ativos,
  pendentes, desconhecidos e ilegíveis ficam protegidos. Exportação não limpa.
  Limpeza exige arquivo verificado e comparação otimista do registro atual.
- Exportação de checkpoint conserva texto/base, seleção, estilos, identidades
  e semântica; não equivale a backup integral do Projeto nem transplanta
  referências automaticamente. Arquivo existente nunca é sobrescrito.
- GTK continua responsável pela composição. Preedit não vira conteúdo
  persistido; transição que descartaria composição em andamento é recusada.
  Commit composto deve ser reversível sem arrastar digitação adjacente.
- Sessão longa mede latência e memória sem registrar manuscrito. Prova
  automatizada não substitui revisão humana de 45–60 minutos e IME específico.

## Segurança e verificação

Disco com aproximadamente 147 MiB livres na abertura. Compilação e fixtures
temporárias em área exclusiva de RAM, com autorização. Fonte, logs e candidato
final duráveis no workspace. Nenhum predecessor removido. A área em RAM não é
entrega e desaparece ao reiniciar. Revisões novas devem preservar conteúdo
persistido e checkpoints das revisões anteriores por backup SQLite consistente.

Configuração recomendada: GPT-6.1 Sol/high, custo relativo alto, mesma tarefa;
integridade, ciclo de vida GTK e composição. Aceite humano ainda pendente;
este brief não declara etapas anteriores encerradas.

## Operação do candidato

1. No editor, Camadas abre o Documento atual. Busca abre a mesma página no
   escopo Projeto; mude o seletor para retornar às camadas locais. Os filtros
   aplicáveis a cada escopo são apresentados explicitamente. O escopo Projeto
   não incorpora texto ainda não persistido nem força salvamento.
2. Lentes colore o fundo com alpha 0,28; as letras mantêm a cor original. Um
   único fundo prevalece na sobreposição, mas o tooltip identifica todos os
   papéis encontrados, com estado e um campo útil ou tamanho do trecho.
3. Em Recuperação, abra Retenção e exportação… e aguarde a prévia assíncrona.
   O padrão protege fechados recentes de 30 dias; 90 dias e todos os fechados
   são escolhas explícitas. Exportar não limpa. A revisão da seleção abre uma
   segunda confirmação, com quantidade, bytes e destino; cancelar não altera
   registros. Um lote que falha na validação não aplica remoções parciais.
4. Exportações ficam no próprio Projeto, em `recovery/editor-v1/exports`.
   Preservam o JSON original do checkpoint e incluem versões de tipos usados
   disponíveis no registro do Projeto. O envelope é versão 1, checksummado,
   escrito em temporário exclusivo, sincronizado e reaberto antes da limpeza.
   SHA-256 detecta divergência; não comprova autenticidade contra adulteração.
5. Composição nativa: conclua ou cancele o preedit antes de mudar de contexto.
   GTK pode encerrá-lo ao perder foco; a barreira atua enquanto o contexto
   ainda o informa como ativo. Não se promete recuperar texto provisório após
   queda de energia. A confirmação composta tem seu próprio grupo de undo.

## Limites deliberados

- Sem novos tipos, acepções, heurísticas, inferência ou binding automático.
- Exportação de recuperação não é DOCX/EPUB/PDF, backup integral do Projeto ou
  pacote que replanta identidades em outro Projeto. O leitor validado existe
  na API; nenhuma UI de importação foi adicionada neste corte.
- Pendentes podem ser exportados se inativos e válidos, mas não são elegíveis
  para esta limpeza. Registros desconhecidos, corruptos ou com lease ausente
  não são descartados silenciosamente.
- Não há VACUUM durante a edição. A limpeza libera páginas para reutilização
  e preserva arquivos de lease; redução imediata do tamanho físico não é
  prometida. Exportações não são apagadas pela operação.
- A estimativa de memória do histórico inclui metadados, âncoras e referências,
  além do texto; é orçamento de payload, não medição exata de heap ou RSS.
- O ensaio longo automatizado usa mutações do TextBuffer, salvamento, undo/
  redo e reabertura, com lente, estilo e anotação existentes. IME recebe teclas
  físicas no display privado, via IMContextSimple. Isso não certifica IBus,
  todos os idiomas, ausência de perdas com falha física ou conforto humano.
- O diagnóstico `change_to_frame_tick` mede chegada ao tick GTK, não prova a
  apresentação dos pixels. Sanitização focal não equivale a instrumentar
  integralmente GTK, dependências externas, vazamentos ou data races.

## Evidências e continuidade

Os relatórios duráveis ficam em `outputs/editor-consolidacao-*.json` no workspace
de entrega. `execucao` identifica a regressão final; `sessao-longa` só deve ser
aceito com `complete: true` e duas sessões de pelo menos 2700 segundos no mesmo
SHA. Tentativas anteriores interrompidas ou falhas continuam nos logs e não são
contadas como aprovação. A revisão humana é registrada no YAML do novo diretório
`review`, separado dos predecessores e fora do manifest de arquivos imutáveis.

A base Git permanece f155bd8; não há novo SHA de commit para representar este
candidato. O patch incremental parte do HEAD mais o patch UX congelado; o
cumulativo parte do HEAD limpo. Não aplicar o cumulativo cegamente em checkout
sujo: ele também contém as alterações linguísticas preexistentes preservadas.
Schema principal 16, journal auxiliar 1 e payload de checkpoint 2 permanecem
iguais ao candidato UX. Nada foi publicado como release.

Próxima configuração: revisão humana não requer modelo; ajustes delimitados
podem usar GPT-6.1 Sol/medium, custo relativo menor. Se envolver novamente
concorrência, integridade ou ciclo de vida IME, manter high. Não iniciar outro
incremento semântico antes da decisão humana sobre este candidato.
