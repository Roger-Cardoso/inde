# OBJ-01 — composição comum e remoção reversível

Data: 2026-11-22. Base `93caf049fe575025aaed9f6cb89dc539b2fdeab4`, branch `feature/inde-obj-01-foundation`. O usuário pediu remoção acessível e que posição/alinhamento/lado a lado se aplicassem também a textos e tabelas. [Roteiro](../../tools/obj01-pilot/COMPOSICAO.md), [validação](../validacao/VALIDACAO-OBJ-01-COMPOSICAO-EDICAO.md) e [reconciliação](RECONCILIACAO-OBJ-01.md).

## Aprendizado e alcance

A fundação já possuía ordem autoral e visual independentes, par figura/parágrafo, histórico e retenção de imagens/tabelas/usos de fórmula. Faltava um contrato comum de apresentação e exposição da retirada/restauração. O texto proprietário tem grupos de leitura indivisíveis: remover um parágrafo isoladamente pode cortar intervalos/alternativas/referências. O adaptador continua somente leitura; não se simulou exclusão de offsets no Documento real.

A nova camada privada conserva todo o snapshot proprietário e registra os grupos retirados da composição. A projeção ativa filtra esses grupos, incluindo grafo, listas de parágrafos e leitura. Os objetos de origem permanecem conservados, mas não há ocorrência órfã ativa. A publicação antiga continua recusada para o composto, sem omitir conteúdo silenciosamente.

## Contrato das operações

| Operação | Efeito | Invariantes |
| --- | --- | --- |
| `arrange_objects` | Dois usos ativos, lado explícito, duas proporções e espaço | Aproxima na apresentação, conserva leitura/conteúdo, desfaz pares anteriores dos dois, valida o grafo inteiro |
| `set_presentation` | Largura percentual, posição horizontal, alinhamento textual e vertical | Por ocorrência; não altera fonte/payload/referências; geometria finita e enumerações fechadas |
| `reset_presentation` | Retira estilo individual | Conserva linha, ordens e conteúdo |
| `stack` | Desfaz linha do uso escolhido | Vale a todos os tipos no P2-D2; conserva sequência visual e estilo |
| `move` | Texto participa do mesmo instrumento | Leitura textual usa grupo completo e motor proprietário; visual pode mover um parágrafo sem reescrever origem |
| `remove` | Retira uso ou grupo textual | Não exige alinhamento no P2-D2; retira das duas ordens, desfaz somente linhas afetadas e conserva conteúdo/estilos/recursos |
| `restore_removed` | Reinsere retirado em destino explícito | Exige alinhamento, fronteira válida de grupo e registro conservado; não restaura comando consumido |
| `restore` anterior | Recupera apresentação pela leitura | Desfaz pares, conserva estilos individuais |

Linhas possuem exatamente dois usos distintos ativos; todos os pares entre texto, figura, tabela e fórmula são admitidos. Não há árvore de linhas aninhadas. Proporções positivas somam 1; gap finito entre 0 e 256. A interface oferece proporções de 20% a 80%. A largura individual é finita entre 20% e 100%, relativa à coluna/área disponível. Horizontal/vertical: start/center/end; texto: left/center/right/justify. Selecionar/compor o objeto inteiro limpa seleção de célula. Uma tabela inteira e uma fórmula no fluxo recebem estilos; célula não é promovida implicitamente a objeto livre.

`removed_text` guarda identidades de grupos reais do proprietário, não cópias aproximadas do texto. Nenhum grupo pode ser retirado parcialmente ou repetido. Ordens contêm exatamente os usos ativos. Ordem autoral textual continua igual à projeção proprietária filtrada; grupos ativos permanecem contíguos. Mover/restaurar um grupo aplica `move_reading_block` no snapshot de trabalho completo, inclusive quando há outros grupos retirados.

## Persistência e compatibilidade

Não se alterou tabela, coluna, schema canônico ou arquivo autoral. Auxiliar SQLite `user_version=5`, linguagem de fórmulas 2 e perfil P2-D2 permanecem. Snapshot privado `INDE-OBJ01-COMPOSITE-P2D2-2` acrescenta somente `composition` com grupos retirados e estilos. Ele é usado quando a extensão é necessária; estados antigos continuam com formato 1 e são lidos sem conversão no lugar.

Reconhecimento é estrito: campos desconhecidos/faltantes, identidades ausentes/inativas, grupos inválidos, estilos de usos desconhecidos, linha duplicada/inválida ou geometria fora dos limites recusam o candidato/reabertura. A revisão explícita faz o binário anterior recusar o snapshot novo, em vez de perder metadados silenciosamente. O teste real com o binário anterior conferiu origem e banco inalterados após a recusa.

Tudo entra no mesmo candidato validado e checkpoint existente: fonte proprietária quando reordenada, máscaras, estilos, linhas, duas ordens, conteúdo compartilhado e seleção. Desfazer/refazer restaura o conjunto. Itens retirados sobrevivem ao reinício e podem ser restaurados sem histórico em memória. Restauração manual conserva estilo individual, mas monta novo posicionamento; Desfazer restaura também os pares anteriores. Comando consumido só volta ao desfazer sua execução atômica.

Os limites de retenção preexistentes continuam: 256 figuras, 64 tabelas, 64 fórmulas, 256 usos conservados de fórmula, 128 linhas e 24 MiB de snapshot, além dos orçamentos do motor. Estilos só podem apontar a usos conhecidos ativos/conservados; remoção não libera recursos nem cria GC definitivo. Corpo de texto, alternativas e perfis autorais não são reduzidos pela máscara.

## Interface e limites

Lista comum de objetos, ações junto ao objeto, Compor e área de removidos usam operações reais pelo HTTP autenticado e C++. Sem avaliador/compositor de estado paralelo no JavaScript. No desktop, documento acompanha os controles durante rolagem. Ordem de leitura desconsidera formatação visual; Compor volta à apresentação para mostrar o resultado. Aplicar geometria é explícito, e o rascunho não aplicado não é persistido.

Não estão incluídos: edição livre dos parágrafos originais; remoção parcial de grupo protegido; exclusão definitiva de fórmula compartilhada/biblioteca; arrasto livre, resize por alças, rotação/sobreposição, contorno automático de texto, grades arbitrárias; GTK; publicação canônica; migração de obras; `.inde` único. A validação técnica e a demo não encerram a fase inteira nem substituem o aceite humano.
