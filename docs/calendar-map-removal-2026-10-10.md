# Exclusão de sistemas temporais e mapas

Solicitação de 10/10/2026: adicionar as duas opções como ajuste do candidato gráfico.
A solicitação é autorização de implementação, não aceite humano já concluído.

## Interação

- Tempo → sistema cadastrado → Excluir este sistema… → Excluir sistema….
  A opção exclui a versão cadastrada; não salva o rascunho e não exclui todas
  as revisões da família. Sistemas modelo ainda não salvos não têm exclusão.
- Cartografia → selecionar mapa → Excluir mapa….
  Indisponível durante geração, prévia não aceita ou prévia de relevo.
- Ambos abrem a confirmação comum de operações do Projeto, com identidade,
  efeitos observados, reconhecimento explícito, cancelamento e atualização da prévia.
- Confirmar grava a exclusão e seu histórico na mesma transação. Reverter pelo
  histórico do Projeto é distinto de desfazer a digitação e depende de conflitos.

## Proteções

Sistemas com associações a eixos, adoções/sucessões, referências de data/hora
ou revisões derivadas são bloqueados, com explicação. Não apagamos datas,
convertemos calendários ou eliminamos descendentes automaticamente.
Uma revisão folha sem outros usos pode ser excluída com sua própria aresta
de proveniência; a origem continua intacta. Reverter restaura ambas.

Mapas removem somente o planeta selecionado, sua pirâmide de terreno, suas
posições e bloqueios. Locais/entidades, suas relações, documentos, outros mapas
e exportações externas permanecem. O índice espacial acompanha os gatilhos existentes.
A reversão guarda e restaura o conteúdo do terreno, não somente seus parâmetros
de geração, e preserva posições e bloqueios.

O motor comum prepara uma simulação consistente, calcula os efeitos reais e
revalida a impressão digital na confirmação. Mudanças de conteúdo com a mesma
contagem também bloqueiam a confirmação. Falhas revertem toda a transação.
Limites do histórico continuam 8 MiB/20.000 linhas por operação; não contornamos
esses limites nem aceitamos exclusões sem prévia para operações grandes.

Sem migração: schema 25. As entregas congeladas anteriores permanecem intactas.
Testes, logs e binários do novo candidato devem estar em manifesto separado.

## Verificação

Testes de domínio: prévia sem escrita, revisão folha e origem, sistemas em uso,
novos vínculos, exclusão/reversão de calendário, mapa/terreno/posições/bloqueios,
mudança de conteúdo, falha provocada, reabertura, integridade e índice espacial.
Matriz GTK em cópias descartáveis de Atlas/Projeto vazio e claro/escuro:
cancelamento, confirmação, bloqueio, preservação de Local e reversão.
Nenhum projeto autoral será excluído para testar a funcionalidade.

Suíte CTest: 26/26 passou. Teste focal ASan/UBSan do motor de operações
inclui restauração dos 170 blocos de um mapa padrão e preservação de outro mapa.
Matrizes GTK Release e focal ASan/UBSan: oito cenários cada, com reabertura;
relatórios completos e hashes devem constar do manifesto deste candidato.
UI focal: cinco unidades e driver; núcleo focal: motor de operações e teste.
Demais dependências Release e LSan desligado, não instrumentação integral.
Oito quadros selecionados inspecionados nos dois temas. Projeto vazio da
matriz é uma cópia de revisão com registros de QA anteriores; o núcleo também
testa um Projeto recém-criado. Resultados técnicos não são aceite humano.
