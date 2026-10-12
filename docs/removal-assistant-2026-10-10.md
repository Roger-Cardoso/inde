# Auxiliar de exclusão e tempo ficcional — 2026-10-10

## Contrato deste incremento

Base: `9dadf11dcb0a64e35e91719ea1547f93d3e4c523`. Schema 25, sem migração.
O bloqueio de integridade continua obrigatório; a novidade é tornar o motivo
acionável, sem exigir que o escritor vasculhe o Projeto. Sugestão de solução
não autoriza apagar vínculos. Não há cascata automática, limpeza em lote,
conversão de datas ou purga de lixeira neste auxiliar.

## Fluxo do escritor

1. Solicitar a exclusão no local habitual.
2. Se houver usos, ver nome, localização e orientação na própria confirmação.
   Buscar por texto ou percorrer páginas de dez usos; total e intervalo ficam
   explícitos. Nenhum painel permanente é acrescentado ao workspace.
3. Escolher **Abrir local do uso**, resolver somente o vínculo desejado pelas
   ferramentas normais do destino, ou decidir preservar o item.
4. Usar **Voltar à exclusão**. O Documento, caret e contexto de origem são
   retomados; a lista é recalculada, não reutiliza uma aprovação antiga.
5. Confirmar somente quando a nova prévia permitir. A confirmação revalida o
   estado persistente; concorrência ou troca de Projeto bloqueiam a operação.

Abrir um uso não apaga nada. O escritor pode cancelar em qualquer etapa.
Escrita pendente é salva antes da navegação. Histórico de cadastro e Desfazer
edição permanecem diferentes: âncoras e marcações só são retiradas na Escrita,
com seu histórico textual, nunca por um DELETE genérico no banco.

## Cobertura transversal

| Área | Exclusões integradas / origens encaminhadas |
| --- | --- |
| Escrita | Documentos, grupos, fontes de revisão e referência textual, âncoras e marcações com relações recebidas; trechos semânticos com posição no texto |
| Entidades e relações | Entidade, tipo de entidade, relação, tipo de relação, âmbito na Obra; presenças, participantes de acontecimentos e referências editoriais |
| Catálogo | Obra e Universo; destino nomeado em vez de apenas contagem |
| Editorial | Ramificação, tipo estrutural, estrutura alternativa e modelo; usos em unidades e restauração correspondente no histórico |
| Narrativa | Estrutura alternativa, modelo, linha, unidade e papel; vínculos de unidades, entidades e atribuições de papéis |
| Tempo | Sistema, eixo, ponto e referência temporal; calendário de origem, adoções, versões derivadas, datas e vínculos das datas |
| Cartografia | Mapa; posição de Local encaminhada ao mapa correto |

São 24 categorias no contrato compartilhado. Join simples já removível por uma
confirmação explícita permanece simples: não se impõe um assistente a cada
desvinculação sem risco. Tipos internos, eixo principal e estruturas ativas
continuam protegidos e explicam a condição. Payloads pertencentes ao próprio
item aparecem nos efeitos, não como falsos bloqueadores externos.

## Calendário sem aprisionamento

**Tempo ficcional** passa a mostrar um resumo do calendário do eixo, sua adoção
mais recente ou **Sem calendário · pontos em ordem relativa**. Oferece
**Configurar calendário…** e **Sistemas e datas…**, reutilizando a Biblioteca
de Tempo existente e a estética do workspace.

Na configuração do eixo:

- Sem histórico de adoções, **Desvincular calendário do eixo…** permite deixar
  o eixo sem calendário, inclusive quando aquele é o único sistema existente.
- Com histórico, **Rever a última adoção…** remove explicitamente apenas a
  adoção mais recente. Repetir de trás para frente conserva uma história
  coerente. Só então é possível desvincular o calendário de origem.
- A explicação e a confirmação dos efeitos são obrigatórias. Ordinais dos
  pontos, acontecimentos, referências temporais e vínculos no texto não são
  apagados, completados ou convertidos. A apresentação pode mudar.
- O serviço compara calendário de origem e histórico observado em transação;
  falha ou concorrência provoca rollback. A operação entra no histórico
  reversível do Projeto.

Desvincular o eixo não torna automaticamente o sistema excluível: datas
autorais ainda podem usar sua definição. O auxiliar apresenta essas datas e os
destinos vinculados. O autor escolhe editar, desvincular, remover a referência
pelas suas ferramentas normais ou conservar o sistema para manter a precisão.

## Segurança e limites explícitos

A inspeção usa as FKs reais do schema, com regras adicionais para relações
semânticas protegidas e o âmbito entidade–Obra (chave composta). Percorre
dependências pertencentes a estruturas e as datas de um calendário para expor
usos indiretos. Nomes completos ficam pesquisáveis; cartões têm limite visual
com tooltip. Acima de 20.000 itens de metadados, a inspeção falha fechada com
explicação, nunca apresenta uma lista parcial como completa.

Restaurações antigas que não tenham operação correspondente no histórico
reversível são identificadas como legado e recebem orientação, sem atalho
destrutivo ou promessa de restauração individual. Para esses registros,
preservar o tipo ou recuperar uma cópia anterior continua necessário. Novas
exclusões estruturais encaminham à operação exata do histórico.

A introspecção não certifica automaticamente novos módulos futuros: cada nova
FK ou bloqueio lógico exige mapeamento de dono, navegação e testes. Integridade,
simulação, fingerprint e guards continuam sendo a autoridade de exclusão.
O auxiliar é orientação de relações persistidas, não análise NLP do texto.

## Verificação proporcional e entrega

`tests/removal_assistant_tests.cpp` cobre projeto novo, usos diretos/indiretos,
106 datas sem truncamento, fontes de Documento e posição de trecho, relações
recebidas, âmbito composto, participantes, papéis, lixeira, proteção de tipos,
desvinculação do único sistema, histórico, concorrência, rollback e restart.

`tests/ui_removal_assistant_smoke.cpp` usa MainWindow e workspaces de produção:
lista, busca/paginação, abertura de eixo, desvinculação, retorno ao caret,
abertura do trecho marcado, integração de Tempo ficcional, revisão de adoção
e restart. `tools/run_temporal_qa.py --assistant` usa snapshots descartáveis do
Atlas e Projeto vazio, temas claro/escuro e compara os dados das origens.

Debug e ASan/UBSan do núcleo são focais (teste + três unidades atuais, demais
dependências Release). ASan/UBSan GTK é focal (driver + sete unidades, demais
objetos Release), LSan desativado. Não equivalem a Debug completo ou cobertura
integral do aplicativo. Capturas inspecionadas não certificam leitor de tela,
HiDPI, IME nem aceite humano. A fixture GTK vazia mantém registros QA anteriores;
o teste de núcleo cria Projeto realmente novo.

Resultados finais, hashes e caminhos constam do relatório e manifesto
`reforma-auxiliar-exclusao-*`. Apenas evidências finais completas contam como
verificação; logs intermediários falhos são mantidos como diagnóstico.
O candidato, laboratório e projetos de revisão são congelados separadamente,
sem substituir entregas anteriores, publicar, alterar catálogos linguísticos,
antecipar fusos/mapas ou iniciar reconhecimento de entidades.
