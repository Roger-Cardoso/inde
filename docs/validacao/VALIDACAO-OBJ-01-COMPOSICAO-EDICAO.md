# OBJ-01 — validação de composição e remoção

Data: 2026-11-22. Base `93caf049fe575025aaed9f6cb89dc539b2fdeab4`; implementação `db217efa28fe3c1d6773e381ecbb276103a30355`; branch `feature/inde-obj-01-foundation`. [Contrato](../arquitetura/COMPOSICAO-REMOCAO-OBJ-01.md), [roteiro utilizável](../../tools/obj01-pilot/COMPOSICAO.md), [resultados estruturados](evidencias-obj01-composicao-edicao/resultados.json) e [manifesto de hashes](evidencias-obj01-composicao-edicao/SHA256SUMS).

## Entrega

Texto, figura, tabela e fórmula usam o mesmo instrumento de composição: par lado a lado, lado escolhido, proporções, espaço, largura do objeto, posição e alinhamento textual/vertical. A remoção está acessível no objeto e na seleção comum. Itens removidos podem ser restaurados após reabrir, sem depender do undo em memória. Parágrafo de origem é retirado como grupo protegido, sem apagar a origem, alternativas, perfis ou offsets.

Não houve escrita em obra real nem no adaptador proprietário. Banco auxiliar continua versão 5 e a linguagem de fórmulas 2. A camada nova tem revisão privada de snapshot explícita, `INDE-OBJ01-COMPOSITE-P2D2-2`. Snapshot anterior mantém formato 1 até a extensão ser necessária; a demo é separada das sete anteriores. A versão anterior do motor recusa o novo formato e conserva os arquivos: [prova](evidencias-obj01-composicao-edicao/compatibilidade.json).

## Testes locais finais

| Configuração | Programas | Tempo | Log |
| --- | --- | --- | --- |
| GCC Debug | 17/17 | 74,00 s | [GCC](evidencias-obj01-composicao-edicao/gcc-final.txt) |
| Clang Debug | 17/17 | 119,98 s | [Clang](evidencias-obj01-composicao-edicao/clang-final.txt) |
| GCC ASan + UBSan | 17/17 | 315,72 s | [GCC sanitizers](evidencias-obj01-composicao-edicao/asan-final.txt) |
| Clang ASan + UBSan | 17/17 | 269,69 s | [Clang sanitizers](evidencias-obj01-composicao-edicao/clang-asan-final.txt) |

Os 15 programas anteriores foram repetidos, incluindo todos os perfis, fórmulas/cartões/hub, codecs reais e adaptador. A nova bateria de núcleo tem **16 casos**; a HTTP tem **sete** (três próprios + quatro comuns herdados, sem contar os comuns como novos). Compilação estrita com warnings como erros. Sintaxe JS/Python/iniciador e diff conferidos. A mudança final de Compor retornar à apresentação também foi exercitada no navegador. LeakSanitizer desativado localmente; habilitado na matriz remota.

Casos novos: pares entre quatro tipos e leitura independente; texto com texto inclusive mesmo grupo; troca de lado; largura/alinhamento/vertical, save/reopen/undo/reset; campos/geometria/alvos inválidos e atomicidade; retirada/restauração de grupo inteiro; todos os textos retirados; fronteira parcial recusada; remoção em apresentação divergente; figura/tabela/fórmula restauradas com texto, frase e vínculos; comando consumido fora da área de restauração; mover texto pela leitura leva grupo inteiro pelo proprietário; mover só visual não muda origem; formato antigo conservado e perfil anterior recusando operação nova; contexto/revisão; SIGKILL; oito combinações reais de crash em remoção e restauração.

As oito combinações são duas operações × quatro janelas: `after_blobs`, `after_snapshot`, `before_commit`, `after_commit`. O driver de falhas é separado do binário de interface. Não se prova todo tipo de pane de energia/armazenamento. O protocolo anterior de gravação e confirmação incerta continua coberto pelos programas repetidos.

## Demonstração observada

Motor C++, SQLite e servidor autenticado reais, com fixture sintética nova em `dados/obj01-piloto-composicao/`:

1. Texto Água e texto Horizonte colocados lado a lado, colunas 40/60 e espaço 18. Água com 80% da coluna, posição/texto centralizados e vertical no meio. [Captura](evidencias-obj01-composicao-edicao/texto-texto.jpg), [estado](evidencias-obj01-composicao-edicao/texto-texto.txt).
2. Água removida pela ação junto ao objeto: fora do fluxo, origem conservada e item restaurável. [Captura](evidencias-obj01-composicao-edicao/texto-removido.jpg), [estado](evidencias-obj01-composicao-edicao/texto-removido.txt). Restauração manual antes de Horizonte conserva estilo; undo de restauração e remoção recupera o par antigo: [estado](evidencias-obj01-composicao-edicao/undo-remove.txt).
3. Tabela inserida e posta à direita do texto Casa, colunas 45/55. Texto alinhado verticalmente no meio. [Captura](evidencias-obj01-composicao-edicao/texto-tabela.jpg), [estado](evidencias-obj01-composicao-edicao/texto-tabela.txt).
4. Remoção de tabela e undo conferidos. Célula editada para “Nota preservada após remover”; tabela removida novamente e restaurada manualmente, com o texto conservado. [Removida](evidencias-obj01-composicao-edicao/tabela-removida.txt), [restaurada](evidencias-obj01-composicao-edicao/tabela-restaurada.txt).
5. Figura inserida, largura 60% e posição à direita; retirada e restaurada com estilo. [Remoção](evidencias-obj01-composicao-edicao/figura-removida.txt).
6. Valor Personagem criado pelo hub; uso removido e restaurado. Fórmula permanece na biblioteca, sem apagar referências. [Remoção](evidencias-obj01-composicao-edicao/formula-removida.txt).
7. Três pares montados: texto/texto, texto/tabela, figura/fórmula. Remover Horizonte retira o grupo de dois parágrafos e desfaz somente os dois primeiros pares. Undo recupera os três pares: [grupo retirado](evidencias-obj01-composicao-edicao/grupo-removido.txt).
8. Ordem de leitura apresenta fluxo simples; **Compor objeto selecionado** retorna à Apresentação com três pares reais. Uma segunda ocorrência da mesma imagem foi criada e retirada; ficou disponível na área de removidos, sem apagar a imagem ativa.
9. Sem Salvar, SIGKILL somente no motor sintético; servidor encerrado e reiniciado. Snapshot/hash/geração reaparecem idênticos, inclusive fonte, seleção, quatro estilos, três linhas, célula editada e um retirado. Working geração **53**, Saved revisão **0**, seis usos ativos. Banco íntegro, sem violações de chaves estrangeiras; histórico reinicia vazio. [Resumo verificável](evidencias-obj01-composicao-edicao/reinicio.json), [antes](evidencias-obj01-composicao-edicao/antes-reinicio.txt), [depois](evidencias-obj01-composicao-edicao/depois-reinicio.txt).

[Captura final da demonstração](evidencias-obj01-composicao-edicao/composicao-final.jpg). Capturas são prova visual; os testes de núcleo/HTTP e a comparação do snapshot provam a persistência. Os dumps acessíveis conservam o conteúdo autoral; só espaços finais de linhas de evidência são normalizados no pacote.

## Preservação e CI

O relatório antigo da ponte de leitura `VALIDACAO-OBJ-01-COMPOSICAO.md` e seus 11 arquivos de evidência foram conservados sem alterações. Esta entrega usa o sufixo COMPOSICAO-EDICAO e um pacote novo, distinguindo implementação de edição da auditoria histórica da ponte.

14 arquivos de origem/banco de sete demos anteriores com hashes iguais: [início](evidencias-obj01-composicao-edicao/preservados-inicio.json), [fim](evidencias-obj01-composicao-edicao/preservados-final.json). Manifestos A/B/C/D/D2/Cartões/Hub foram verificados nas revisões correspondentes (37+39+44+49+47+41+48 = **305 entradas**). Manifestos anteriores são históricos: não foram reescritos para ocultar diferenças legítimas da nova implementação.

Três arquivos compactados conferidos novamente por hashes/manifestos e extraídos em diretório novo, com bytes iguais: [fontes](evidencias-obj01-composicao-edicao/fontes.json). Auditor de recusas, cinco testes: [log](evidencias-obj01-composicao-edicao/auditor-fontes.txt). Modelo/comandos da fundação, adaptadores, escrita/checkpoint, linguagem/hub/cartões e os arquivos compactados/metadados permanecem sem alterações neste corte.

Main permanece em `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com a alteração preexistente de modo de `Preparar-ponte.sh` conservada. Sem schema real, projeto autoral, GTK, publicação canônica ou `.inde` único. A fase inteira não é declarada concluída.

A execução de implementação [45060187650](https://github.com/Roger-Cardoso/inde/actions/runs/45060187650) corresponde ao commit exato. [Resultado completo](evidencias-obj01-composicao-edicao/ci.json); os 25 trabalhos concluíram com sucesso no commit exato, com LeakSanitizer habilitado nos perfis remotos de sanitizers.

## Limites reais

Linhas de dois objetos, sem encaixe de linhas, arrasto X/Y, rotação, sobreposição ou contorno de texto em imagem. Parágrafos originais continuam em leitura; remoção é de grupo completo e não apaga o corpo proprietário. Restauração manual pede apresentação alinhada, destino válido e não recupera pares antigos; undo recupera a operação completa. Tabelas continuam 2×3. Retirados contam nos limites de retenção; não há exclusão definitiva de recurso/fórmula compartilhada ou GC.

Não foram repetidos os 61 testes nativos antigos; o relatório não os apresenta como novos. Foram repetidos os 17 programas do piloto/adaptador em quatro configurações e a CI da fundação. Não comprovados todos os dispositivos/entradas, ampla concorrência, benchmark de latência/RSS, todas as falhas de armazenamento ou aceite humano. Rascunho de geometria não aplicado permanece no navegador.

Conferência na raiz do worktree: `sha256sum -c docs/validacao/evidencias-obj01-composicao-edicao/SHA256SUMS`.
