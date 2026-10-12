# Etapa 1 — grande corte: busca, sessão e transações

Data: 2026-10-03. Autorização direta: combinar o corte recomendado com mais
um ou dois cortes e iniciar. Base: E1.1 sobre Git
`f155bd866d32c9326e5853a2882e6adbfca6ba23`, seis cortes e diff linguístico anterior.
Branch local: `codex/editor-stage1-large-2026-10-03`. Implementação em clone
separado; checkout original, fonte/artefatos E1.1 e projetos originais preservados.

## Três responsabilidades integradas

1. E1.2: localizar/substituir no Documento aberto, navegação e atalhos.
2. DocumentEditSession: estado editável, seleção dirigida, dirty/saved e histórico.
3. EditCommand/EditTransaction: operação composta reversível, deltas e fallback.

Não altera schema v15, contratos/catálogos linguísticos, bindings semânticos,
journal, clipboard, paginação, AppImage, versão publicada ou estado remoto.
Não conclui toda a Etapa 1. O editor semântico continua consultivo; substituir
texto não escolhe acepção, resolve referência ou cria vínculo narrativo.

## Busca

`find_text` é puro, literal e não sobreposto, com offsets em escalares Unicode,
iguais aos offsets GTK. Sem expressão regular, busca aproximada, remoção de
acentos ou normalização NFC/NFD implícita. Case-fold Unicode mantém mapa para
o texto original: `ss` encontra `ß`, mas `s` não encontra metade desse escalar.
Palavra inteira inclui letras, números, marcas combinantes e `_`; hífen e
apóstrofo separam palavras nesta versão. Acentos continuam significativos.

Um coordenador independente executa a busca em background; snapshot próprio,
debounce 150 ms, entrega GTK 30 ms. Edição, novo termo/opção, troca, restauração,
fechamento e reset invalidam imediatamente a geração. Resultados antigos não
ficam substituíveis. Limite de 100.000 resultados: o excesso é explicado,
**Substituir tudo** fica desativado e o domínio rejeita planos truncados.

Ctrl+F/H, F3/Shift+F3 e Esc têm escopo no widget de Escrita, não global atrás de
diálogos ou em outros workspaces. Localizar e Substituir… também são acessíveis
por botão. O resultado seleciona/rola uma ocorrência; não destaca todas com
tags. Se o usuário mudou a seleção, Substituir primeiro seleciona o alvo e
explica que outro clique é necessário. Nenhum resultado é estado silencioso.

## Substituição e camadas

O domínio valida o plano antes de construir o novo estado; ordem, sobreposição,
limites, UTF-8 e truncamento são checados. Busca não implica alteração.
Substituições sem mudança real não alteram camadas nem destroem Refazer.
Vazio significa exclusão. Matches idênticos à reposição são ignorados, inclusive
em um plano misto; formatação/âncoras desses trechos não são reescritas à toa.

A reposição herda os estilos ativos no primeiro escalar do trecho substituído.
Estilos iniciados apenas no interior removido colapsam e são descartados; não
há tentativa de inferir intenção de formatação. Âncoras conservam UUID/rótulo,
com afinidade inclusiva à esquerda no início e à direita no fim, como as marcas
existentes. Uma âncora totalmente apagada pode permanecer com comprimento zero.
Referências e seus UUID/entidade/notas são mantidos; não ocorre interpretação
ou rebinding. Seleção, cursor e direção são remapeados explicitamente.

O adaptador consolida a digitação anterior, aplica o novo estado GTK e registra
as camadas efetivamente normalizadas pela UI. Em falha de aplicação/consolidação,
restaura o estado anterior. Substituir tudo gera um undo, não um por ocorrência.
Organização documental usa o mesmo caminho: só campos aceitos são alterados;
texto/seleção atuais não são substituídos pelo snapshot antigo do formulário.

## Sessão/histórico

Um estado corrente possui Documento e seleção; saved é distinto de undo.
Seleção isolada não suja o Documento. Mutação invalida Refazer imediatamente,
antes do debounce. Edição é agrupada em 650 ms; autosave mantém 900 ms.
Undo/redo restauram texto, metadados autorais, formatação, âncoras, referências
e seleção. Timestamps atuais de persistência não voltam no tempo.

Rascunhos com título temporariamente vazio são reversíveis. Validação completa
de domínio/organização/referências continua obrigatória ao persistir; não se
salva um título inválido. A substituição pura também valida o Documento.
Histórico é de sessão: abrir/reset descarta pilhas; não é persistido no banco.

Prefixo/sufixo comuns, cortados em limites UTF-8, permitem delta textual quando
o texto alterado ocupa no máximo metade dos dois textos combinados. Grandes
mudanças usam snapshots. Metadados/camadas continuam snapshots; o orçamento
de 64 MiB é de **texto retido**, não RSS total. Uma transação maior que o limite
é recusada antes do histórico mudar. Limites: 50 operações, ou 10 acima de
2 MiB. Grupos antigos são removidos ao exceder limites. Hash FNV interno protege
replay/saved; checksums de artefatos usam SHA-256, não esse hash interno.

Histórico compacto não significa buffer GTK incremental: a aplicação de uma
transação composta e undo/redo ainda restaura texto/camadas na thread GTK.
Autosave também permanece síncrono. Queda de energia pode perder edição
posterior ao último autosave; journal/recuperação não fazem parte deste corte.

## Verificação e portão

CTest acrescenta `inde_document_edit_tests`: case-fold/ß, Unicode combinado,
palavra inteira, acentos, limites/cancelamento, exclusão, planos inválidos,
afinidade/camadas, no-op, undo/redo/saved, seleção e orçamento.
50 inserções de emoji em um texto de 1 MiB retêm 200 bytes textuais de histórico
no teste focado — não uma medição da memória total do aplicativo.

`ui_editing_smoke` usa objetos UI de produção, fixture descartável e atalhos X11
na própria janela. Testa busca, navegação, substituição única/todas, seleção,
camadas/metadados, referência autoral à entidade e uso externo da âncora,
autosave/reabertura, no-op/refazer, digitação isolada, troca
e reset. `ui_writing_smoke` repete 1 MiB, 40+16 edições e o fluxo E1.1.
Shell repete primeira carga de entidades, lifecycle e botão Menu do Aplicativo.

ASan/UBSan focados cobrem os três novos módulos puros de aplicação e a suíte
de edição. Dependências preexistentes/GTK não são instrumentadas nessa rodada;
LSan é desativado devido ao ambiente. Não é build Debug/ASan/TSan integral.

Evidências finais/checksums/launcher: `outputs/editor-etapa1-grande-corte-*`
no workspace de entrega. Não confundir build/testes com aceite humano.
E1.1 + este incremento exigem **revisão consolidada agora**, em Projeto vazio
e Atlas, antes de qualquer terceiro incremento. A aprovação de ampliar o corte
não fabrica aceite de todos os cenários de UX.

Configuração para a revisão: GPT-6.1 Sol / high, custo relativo alto, mesma tarefa,
pela superfície de integridade/camadas e regressão. Sem escalada automática.
