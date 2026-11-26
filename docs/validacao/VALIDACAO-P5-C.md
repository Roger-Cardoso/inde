# Validação P5-C — granularidade e custo da confirmação

Data: 2026-11-26. Base: `e605b3726522fd0d17943287c7950a6ed7777e23`. Branch `feature/inde-p5-single-file`. O [contrato](../arquitetura/ARQUIVO-UNICO-P5-C.md) define formatos, compatibilidade e limites. Os resultados e hashes deste corte ficam em `evidencias-p5-c`, separados dos manifestos históricos A e B.

## Cobertura

- Os casos anteriores de transporte rodam nos dois perfis: ida e volta exata, privacidade do conteúdo, WAL recusado, caminhos inseguros, fonte de outro projeto, destino existente, pedido desatualizado, corrupção histórica e restauração em ambas as direções.
- Os casos de blocos cobrem arquivo vazio, limites de 64 KiB, repetição e compartilhamento de blocos, crescimento, redução, retirada, restauração, alteração pequena em banco grande, receita reordenada sem hash correspondente, comprimento inválido, descritor inválido e perfil futuro.
- A sessão com interrupção real de processo, cópia com histórico, duas escolhas de recuperação, descritor ausente/inválido, caminho inseguro e perda do recibo depois de confirmar roda também no perfil por blocos.
- A prova nativa de Documento completo, alternativas, visibilidade, semântica, composição, objetos, arranjo divergente e planejamento ligado ao texto roda nos dois perfis.
- O percurso GTK verifica confirmação explícita, ausência de captura a cada tecla, histórico, cópias, Recentes, interrupção, retomada, falha ao encerrar, criação e abertura. Um percurso começa com arquivo anterior; o outro começa por blocos. Novos arquivos e cópias do estado atual usam blocos nos dois percursos.

## Resultados locais

| Camada | Resultado |
| --- | --- |
| GCC Release | 4/4 testes CTest, 9,64 s; transporte contém 34 casos Python |
| Clang + ASan/UBSan/LeakSanitizer | 4/4, 18,62 s |
| Suíte nativa completa | 71/71, 175,42 s |
| Regressão completa da interface e integrações | 103/103, 249,04 s |
| Percurso GTK com arquivo anterior | 14 verificações aprovadas |
| Percurso GTK por blocos | 14 verificações aprovadas |
| Perfil persistido pelo editor | Oito arquivos fictícios conferidos por leitura: anteriores, cópias com histórico, cópias atuais e arquivos novos |
| Demonstrador entregue, aplicativo normal | Arquivo direto, Salvar/Sair por teclado e integridade aprovados |
| Comparação P5-B/P5-C | Nove execuções, três tamanhos e seis confirmações por execução |
| Leitor P5-B | Perfil anterior aceito; blocos recusados sem alterar origem |
| Preservação | 758 fontes integradas e três compactados conferidos |

Código validado: `e1659e4565aa2ca73490ea1df9a1af5471813803`. A [CI 45895692547](https://github.com/Roger-Cardoso/inde/actions/runs/45895692547) terminou em 26/11/2026, às 21:15:18 UTC, com **5/5 jobs aprovados**: GCC e Clang com e sem ASan/UBSan/LeakSanitizer (quatro testes em cada combinação), mais integração nativa (seis testes P5, demonstração anterior, percursos gráficos com arquivo anterior e por blocos, e medição com restauração e crescimento de recursos inferior a 1/8 do perfil inteiro no ensaio de 4 MiB). Os trechos de resultados e passos da execução são preservados nas evidências. Os manifestos A e B continuam sendo evidências das revisões históricas, não dos arquivos atuais. A [conferência deste corte](../../tests/project_file/verify_block_evidence.py) verifica fontes e evidências C.

## Medições

A comparação usa GCC Release e o mesmo programa de medição contra P5-B e P5-C. Para cada tamanho, gera outro banco determinístico, abre uma sessão persistente, realiza seis alterações pequenas, mede `FileSession::save`, testa confirmação sem mudança, restaura o estado inicial e confere o histórico. Conserva os dados estáveis, além do texto alterado.

O tempo de Salvar é medido depois da alteração no banco de trabalho, incluindo captura, publicação do estado e atualização do descritor. Não inclui digitação, renderização ou o primeiro salvamento do Documento pelos serviços do editor. São observações locais sequenciais, sem garantia de tempo real, isolamento térmico ou meta universal. O crescimento físico inclui o envelope e os índices; os bytes de recursos são medidos separadamente.


Resultados locais — mediana de seis confirmações por execução:

| Dados estáveis | P5-B, ms | P5-C perfil anterior, ms | P5-C blocos, ms | Novos recursos P5-B | Novos recursos por blocos |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 MiB | 52.16 | 54.41 | 47.02 | 6.328 MiB | 384 KiB |
| 4 MiB | 212.67 | 174.98 | 139.91 | 24.891 MiB | 384 KiB |
| 16 MiB | 748.64 | 759.42 | 554.49 | 99.141 MiB | 384 KiB |

Os bancos gerados ocupam aproximadamente 1,05, 4,15 e 16,52 MiB, incluindo suas tabelas e páginas SQLite. O crescimento considera as seis alterações, separadamente da captura inicial. No maior perfil, o crescimento físico do arquivo passou de 99,24 MiB para 1,03 MiB; os novos recursos caíram de 99,14 MiB para 384 KiB. A mediana de confirmação caiu cerca de 25,9%, mas ainda ficou em 554 ms: não satisfaz uma meta geral de até 100 ms.

A otimização do perfil antigo não trouxe melhora uniforme: no maior perfil, sua mediana ficou ligeiramente acima da anterior. Não se promete aceleração universal. A memória máxima observada no processo do perfil de 16 MiB caiu de 95.676 KiB para 79.336 KiB com blocos; esse pico inclui geração, captura inicial, confirmação, restauração e verificação, não só Salvar.

O leitor P5-B materializou um arquivo no perfil anterior escrito pela versão atual e recusou o perfil por blocos, mantendo o hash do arquivo de origem. A [comparação completa](evidencias-p5-c/comparacao.json) contém amostras individuais, bytes, memória e proveniência das fontes anteriores.

## Observação da experiência existente

Os 103 testes aprovam os resultados funcionais dos percursos exercitados. As [amostras de interface](evidencias-p5-c/observacao-interface.json), porém, ainda mostram aproximadamente 396–506 ms em exclusões com 200 parágrafos e 817–1.078 ms na reordenação de prosa com 200 linhas. O passe anterior B já registrava atrasos dessa ordem; não é uma comparação controlada de melhora/piora da edição.

Essas observações incluem entrada X11, consulta por observador de baixa prioridade e polling, e não isolam o tempo de pintura. Ainda assim, impedem tratar aprovação funcional como fluidez concluída. As pendências de E1/E3–E7 continuam no plano de maturidade; P5-C não altera os algoritmos de exclusão/reordenação de parágrafos nem encerra esse trabalho.

## Preservação e limitações

Main, projetos autorais e schemas internos ficam preservados. As fontes e compactados são novamente conferidos. Os testes usam somente conteúdo sintético. Os bancos existentes no perfil anterior continuam nele; a comparação inclui materialização com o leitor P5-B e recusa segura do formato novo por esse leitor.

A confirmação permanece síncrona e lê os componentes completos. A economia de histórico não prova a maturidade da edição composta, a latência de telas, a recuperação de toda falha física ou o desempenho em acervos grandes. As áreas reservadas não são apagadas. Aceite humano e adoção autoral geral continuam separados.

## Aceite informado

Na continuidade após esta entrega, o usuário informou “teste aprovado” e autorizou prosseguimento. O relato registra aceite do teste C, sem estendê-lo aos cortes posteriores. A continuidade D1 tem relatório e evidências próprios.
