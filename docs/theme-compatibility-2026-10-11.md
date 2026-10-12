# Compatibilidade da base GTK — correção de 2026-10-11

Correção complementar ao acesso inicial/interação narrativa. Base Git
`995fbb27`, fonte privada não commitada, schema 27 sem migração.
Aceite humano pendente; não é publicação.

## Causa comprovada

O desktop publica `Orchis-Dark-Compact` via XSettings. Seu
`gtk-4.0/gtk.css` contém `:root`, propriedades `--accent-bg-color`,
`var(...)`, `color-mix(...)` e valores Sass não expandidos.
O GTK 4.14.5 carregado neste host recusa partes desse arquivo.
As posições do arquivo e os tipos de erro correspondem ao terminal anexado.

O candidato anterior foi reproduzido, sem modificar o desktop, num Xvfb
com o mesmo tema herdado por `settings.ini`: 112 avisos de CSS.
O teste anterior sem gerenciador XSettings herdava uma base diferente e
não cobria esse problema. O registro anterior de ausência de avisos era
restrito àquele ambiente; não deve ser usado como prova deste cenário.

## Política corrigida

- Principal e laboratório preparam `GTK_THEME=Adwaita` antes de criar a
  aplicação GTK quando não existe uma escolha explícita não vazia.
- Adwaita fornece uma base compatível com a biblioteca carregada; o CSS e a
  paleta do INDE continuam sendo aplicados depois. Não se tenta reinterpretar
  ou editar o CSS de terceiros.
- A propriedade herdada `gtk-theme-name` não é substituída. Modo Sistema
  acompanha nome/preferência claro/escuro sem exigir importar o CSS externo.
  A detecção de “Dark” agora ignora diferenças de maiúsculas/minúsculas.
- Fontes, ícones e configuração do desktop não são alterados.
- Uma variável `GTK_THEME` explicitamente definida permanece respeitada,
  inclusive para diagnóstico com um tema incompatível. Seus avisos continuam
  visíveis. Não existe filtro de logs nem desativação de warnings.
- A configuração vale no ambiente do processo INDE, não no shell pai ou
  nas preferências persistentes do desktop.

O ajuste precisa acontecer antes de GtkSettings construir seu provedor de
tema; mudar uma propriedade depois de inicializar não evita o primeiro parse.
O uso de `GTK_THEME` é uma ponte de compatibilidade restrita: a documentação
GTK classifica variáveis de ambiente como ferramentas de diagnóstico, não
como API estável de personalização. A política deve ser revalidada ao trocar
a biblioteca GTK, e não serve como novo controle de preferência do usuário.

Fontes primárias:
[GTK 4.14.5, escolha e carregamento do tema](https://github.com/GNOME/gtk/blob/4.14.5/gtk/gtksettings.c)
e [variáveis de execução do GTK](https://docs.gtk.org/gtk4/running.html#gtk_theme).

## Verificação deste complemento

- CTest Release: 32/32, incluindo novo teste de política.
- Auditoria encadeada da recompilação: última compilação de cada uma das
  286 unidades habilitadas sem avisos; Ninja sem trabalho pendente.
  Histórico de avisos/reparos anteriores preservado.
- Política sem display: Release, Debug e ASan/UBSan passaram vazio,
  ausência, idempotência, overrides e detecção de modo escuro.
- Matriz normal: sete verificações positivas sem avisos e dois controles
  negativos que reproduzem deliberadamente os erros.
- Matriz com driver focal ASan/UBSan: mesmos sete cenários; os três ensaios
  de tema usam driver instrumentado, enquanto quatro inicializações dos
  executáveis principal/laboratório continuam em Release. Quatro unidades de
  produção e o driver instrumentados, dependências GTK Release, LSan desligado.
- Principal/laboratório abrem cópias de Projeto vazio e Atlas com tema
  herdado; inicialização de seis segundos, integridade/FKs válidas.
- Sonda do desktop real inicializa GTK sem janela e sem Projeto:
  Orchis herdado, base Adwaita, fonte Sans 12, ícones Papirus-Adapta-Nokto e
  Sistema escuro; nenhum aviso. Não representa sessão humana no desktop.
- Asserções sobre pixels da superfície; dois quadros claro/escuro do
  driver nativo inspecionados pelo agente. Não certificam toda a interface.
- Arquivos CSS do Orchis e configuração Xfce conferidos por hash:
  não alterados. Candidatos congelados anteriores preservados.

Correção do próprio ensaio: as primeiras execuções do gerador registraram três
cópias sintéticas na lista global de recentes. Essas três entradas foram retiradas
com permissão, preservando as sete remanescentes e todas as pastas. O gerador
agora recebe XDG_CONFIG_HOME próprio antes de criar a fixture. As duas matrizes
finais conferem também o hash da lista global de recentes, que permanece igual
antes/depois. O cache tem limite de dez; não se inventaram entradas antigas para
recompor um histórico anterior não registrado.

O manifesto `outputs/reforma-acesso-planejamento-tema-entrega-2026-10-11.json`
na raiz da tarefa registra executáveis, fonte completa e evidências.
O novo lançador reutiliza as cópias principais já entregues, sem recriar
seu conteúdo. O laboratório vazio recebe uma cópia descartável própria
corretamente marcada; não se acrescentam Documentos autorais.

Na revisão humana, fechar a versão anterior, abrir com o novo lançador,
conferir o terminal e alternar Claro/Escuro/Sistema. HiDPI, leitor de tela,
IME, contraste WCAG integral e escrita prolongada não foram certificados.
As quatro ordens e a sequência das frentes posteriores não mudam.
