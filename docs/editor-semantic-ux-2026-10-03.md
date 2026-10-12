# Editor semântico — correções de leitura e exploração

Pedido humano de 2026-10-03: corrigir o fundo luminoso das lentes, retirar
Marcar seleção e Pesquisar usos no Projeto do inspetor, colocar ambos na barra
do texto (o segundo como Busca semântica) e organizar as camadas como o
explorador de entidades do Planejamento.

## Implementação

- Lentes somente com sublinhado simples de cor atenuada. Nenhum fundo ou cor
  de glifo é atribuído pelos tags semânticos. Não acumulam iluminação em
  sobreposições; a identificação textual dos papéis pertence aos cartões.
  Formatação autoral e diagnóstico ortográfico têm precedência visual sobre
  os tags das lentes.
- Barra com Camadas semânticas, Lentes semânticas, Marcar seleção e Busca
  semântica. Rolagem horizontal preserva a geometria em larguras menores.
- Camadas é página exclusiva do stack de Escrita, não uma lista de formulários
  dentro de um modal. Voltar ao texto preserva seleção e sessão documental.
- Cartões compactos responsivos, 24 por página, com papel, estado, situação da
  âncora, prévia Unicode e quantidade de pendências. Clique abre detalhe.
  Cor do texto e dos rótulos secundários explicitada na superfície escura,
  sem depender da cor de texto de botões herdada do tema GTK do sistema.
- Pesquisa explícita casefold Unicode por trecho completo, campos, nomes das
  entidades, notas, fonte e UUID; filtros por papel, estado, âncora e pendências.
  Não há busca automática de entidades nem reassociação semântica.
- Filtros recolhíveis; limpar, contagem, página e explicações distintas para
  Documento vazio e resultado vazio. Retorno do detalhe conserva filtros/página;
  troca de Documento inicia um recorte limpo.
- Detalhe organiza resumo, campos com rótulos legíveis, diagnóstico, navegação,
  edição, relações, reancoragem e arquivo/remoção. UUID/proveniência ficam em
  seção avançada recolhível e copiável. Tipos autorais também são progressivos.
- Reancoragem exige seleção explícita; ação de ir ao trecho recusa âncora
  não resolvida, em vez de navegar a uma posição obsoleta.
- Núcleo, schema principal 16, journal auxiliar 1 e checkpoint 2 inalterados.

## Segurança e critérios

Candidato separado em `work/editor-semantico/ux-2026-10-03/inde`, sem sobrescrever
o kernel congelado, as cópias humanas anteriores ou o checkout original.
Testar: sobreposições sem pintura de fundo/glifo; seleção e undo/redo;
criação e consulta diretas na barra; filtros, vazio e paginação; detalhe e
reancoragem; geometria/menu; persistência e domínio dos fixtures.
Capturas são evidência técnica em Xvfb com o tema nativo, não aceite humano.

Retenção/exportação controlada, IME e sessão longa permanecem adiados. Os
demais gates anteriores não são encerrados por este ajuste de interface.
Não há migração nova, commit, push, release ou expansão lexical.
