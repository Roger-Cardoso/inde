# Teclado e fluxo de trabalho — reforma de 12 de outubro de 2026

Implementação privada da proposta de teclado aprovada neste chat. A aprovação
autoriza este trabalho; o executável resultante requer novo aceite humano.
Não é uma release, promoção de branch ou encerramento da reforma inteira.

## Modelo de interação

O teclado tem três camadas: comandos diretos frequentes; navegação e ações
contextuais; paleta transitória pesquisável. A proposta anterior inventariou
417 famílias em 20 contextos e 873 sítios de interação, não 417 teclas exclusivas.
Controles de baixa frequência continuam acessíveis por Tab, Shift+Tab, setas,
Space e Enter no controle nomeado. Menus contextuais abrem por Menu/Shift+F10.
Não se adicionam painéis permanentes para mostrar o catálogo.

`KeyboardCommands` registra identidades estáveis, rótulos autorais, aliases,
grupo, atalho padrão, escopo de foco, motivo de indisponibilidade e callback.
O catálogo aparece na própria interface e pode ser pesquisado pelos termos
antigos: entidade, propriedade intelectual, estrutura/unidade narrativa,
composição, ponto temporal. A paleta revalida o escopo antes da execução.
Callbacks de menus reutilizam ações declaradas; não procuram qualquer botão
da janela pela aparência para executar uma ação desconhecida.

O laboratório tem registro próprio. Ele não herda ações `win.*` da aplicação
principal, nem confunde o histórico da montagem com o desfazer de um campo.

## Comandos de entrada

| Intenção | Padrão |
| --- | --- |
| Buscar comandos | Ctrl+Shift+P |
| Ajuda de teclado, catálogo e personalização | Ctrl+F1; F1 abre ajuda |
| Menu principal | F10 |
| Próxima/anterior região visível | F6 / Shift+F6 |
| Painel de contexto | F9; ferramentas de escrita também F4 |
| Novo / abrir / salvar / salvar cópia de projeto | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Documento livre / biblioteca / montagens de texto | Ctrl+Shift+N / Ctrl+Shift+O / Ctrl+Shift+8 |
| Sair da vista / fechar projeto / sair do programa | Ctrl+W / Ctrl+Shift+W / Ctrl+Q |
| Escrever / Planejar / Revisar | Ctrl+1 / Ctrl+2 / Ctrl+3 |
| Manuscrito / Cronologia / Mapas / Obras e universos | Ctrl+4 / Ctrl+5 / Ctrl+6 / Ctrl+7 |
| Espaços de trabalho / histórico de projeto | Ctrl+Shift+9 / Ctrl+Shift+H |
| Modo Foco | Ctrl+Shift+F |

## Escrita

Ctrl+Z/Shift+Z, X/C/V, B/I/U, alinhamentos L/E/R/J e os demais comandos de
texto pertencem ao editor em foco. Campos comuns conservam a edição nativa.
Ctrl+Shift+C/V copia/cola estilos por escolha; vínculos não são copiados.
Ctrl+Shift+X aplica tachado; Ctrl+Shift+R limpa apenas formatação da seleção.
Ctrl+F/H abre busca/substituição e F3/Shift+F3 navega nos resultados.

Ctrl+Shift+D consulta uma seleção de uma linha de até 256 bytes no dicionário.
Ctrl+Shift+M comenta ou abre comentários quando não existe seleção; Ctrl+M
marca um trecho. Ctrl+Shift+T marca tempo, Ctrl+Shift+L vincula elemento
existente, Ctrl+Shift+B associa uma parte, Ctrl+Shift+G sugere alteração.
Ctrl+Shift+K substitui o antigo Ctrl+Alt+Space para comandos inline.
Ctrl+Shift+E prepara saída: não exporta nem cria Documento silenciosamente.

## Planejamento, tempo, manuscrito e revisão

Insert, Delete, F2, Alt+Enter e Ctrl+D são contextuais; uma ação só existe
quando há lista/alvo apropriado. Em cards de elementos, o menu declarado
carrega o ID do próprio card, evitando usar a seleção antiga de outro card.
Menus de três pontinhos podem ser abertos sem o mouse. Enter abre a seleção
nas listas que já possuem esse contrato; setas não são convertidas em abertura.

Comparar usa Ctrl+Shift+C na lista de planos ou no detalhe da revisão, nunca
no domínio do editor, onde esse acorde copia estilos. Ctrl+Enter confirma os
alvos escolhidos, escolhe um alvo de escrita ou abre texto para revisão de
acordo com a página. F8 apenas abre avisos de continuidade.

Alt+Shift+Up/Down mantém a confirmação de reordenação e a ordem declarada.
Ordem narrativa, cronológica, de escrita e editorial permanecem distintas.
Reordenar partes não reinterpreta cronologia nem muda publicação.

No calendário gráfico, setas navegam dias com a largura da semana selecionada;
Home/End vão ao primeiro/último dia do período. PageUp/Down troca o período;
Shift+PageUp/Down troca o ano sem escolher uma data. Esses comandos não atuam
nos campos de ano ou relógio. Precisões parciais não ganham um dia inventado.
Nos itens CRUD do calendário, F2 edita os campos do item, Insert acrescenta,
Ctrl+Enter adiciona/atualiza o rascunho e Delete remove apenas na lista.
Alt+Shift+Up/Down reordena itens do rascunho. Salvar o sistema é outra ação.

Na cronologia somente leitura, setas/Home/End selecionam itens e Enter consulta
o item. Não se grava uma ordem por navegar na projeção.

## Mapas, montagens e operações sensíveis

O mapa preserva setas e zoom no canvas, rejeitando modificadores excedentes.
Ctrl+0 enquadra o planeta; Ctrl++/Ctrl+- e aliases do teclado numérico dão zoom.
Há formulários de duas coordenadas para medir e de centro do pincel para
preparar relevo. Medir sobre a esfera não calcula rota ou tempo de viagem.
Ctrl+Z/Shift+Z usa o histórico do modo cartográfico, não o editor ao fundo.

Salvar uma montagem preserva seu rascunho; não publica uma versão nos usos
vivos. Aplicar exibe o número de usos vivos afetados e de usos fixados
preservados. Descartar pede confirmação separada e mantém bloco/origens/versões.
O laboratório começa confirmações pelo botão de cancelar, não pelo de confirmar.
Remover um dicionário pessoal também requer um segundo consentimento explícito.

Delete solicita o fluxo existente de exclusão; não executa apagamento em lote.
Nenhum Shift+Delete é criado. Ctrl+Enter é opt-in nos formulários seguros e não
um consentimento universal para remover, exportar ou aplicar uma decisão incerta.

## Foco, IME e preferências

O roteador rejeita modificadores excedentes e nível AltGr; locks não criam
outro acorde. Shift+numerais usa a tradução do evento, preservando o acorde
declarado mesmo quando o layout produz pontuação. A composição preedit de
GtkText/GtkTextView suspende atalhos. Esc não abandona a composição para abrir
uma ação do programa. A documentação do GTK descreve esses sinais em
[TextView::preedit-changed](https://docs.gtk.org/gtk4/signal.TextView.preedit-changed.html)
e os níveis em [Gdk.KeyEvent](https://docs.gtk.org/gdk4/class.KeyEvent.html).

Paleta e modal guardam foco fraco: não mantêm ponteiros para controles mortos.
Ao fechar a paleta, a seleção e a posição do editor permanecem. Um sinal de
fechamento atrasado não devolve foco ao fundo de um modal recém-aberto.
Tab/Shift+Tab e Ctrl+Tab ficam dentro do modal. O cabeçalho é desabilitado
enquanto um formulário modal está ativo; atalhos globais não atravessam o modal.

Personalização grava apenas `keyboard-v1` no `interface.ini` do perfil do usuário,
capturando a combinação pressionada em um campo próprio, com nome legível —
não exige que o escritor digite a sintaxe técnica dos aceleradores.
nunca no banco autoral. Permite alterar, retirar e restaurar padrões. Conflitos
são recusados conservadoramente, inclusive entre comandos de contextos distintos;
essa restrição é explicada na interface. Não se promete detectar reservas de
todos os gerenciadores de janelas. Acordes AltGr, Super, Unicode Ctrl+Shift+U,
texto sem modificador e teclas estruturais permanecem protegidos.

## Verificação e aceite

Resultados finais, identidade do executável, capturas, hashes, comandos
registrados em runtime e limitações ficam no manifesto de entrega desta fase.
As matrizes de política não substituem os testes GTK. Debug/sanitizers focais
não representam instrumentação de todas as dependências.

O passe humano deve cobrir Projeto vazio e Atlas, ambos os temas: iniciar sem
Obra, seleção/clipboard/desfazer em editor e campos, busca, paleta com seleção,
comparação, as quatro ordens, calendário autoral, mapa por coordenadas,
exclusão assistida, montagem com usos vivos/fixados e teclado físico ABNT2/IME.
Testes automáticos não certificam leitor de tela, IBus, Wayland, HiDPI ou uma
sessão longa de escritor. Não há alteração de schema nesta fase (permanece 28).
