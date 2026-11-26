# Experimentar o histórico por blocos — P5-C

Execute com Bash [Abrir-demo-arquivo-unico-nativo.sh](../../scripts/demos/Abrir-demo-arquivo-unico-nativo.sh), sem argumentos. A demonstração cria um projeto fictício novo com texto, composição, planejamento, imagem, tabela e equação, agora no perfil por blocos. O INDE nativo abre o arquivo diretamente. O roteiro de comandos continua o de [P5-B](ARQUIVO-UNICO-P5-B.md).

1. Escreva no Documento e use Ctrl+S.
2. Abra Projeto → Estados do arquivo único (Ctrl+Alt+H) e retome um estado anterior. As revisões posteriores continuam na lista.
3. Encerre, reabra e confirme o conteúdo retomado.
4. Crie uma cópia com histórico e outra do estado atual, usando Projeto → Salvar cópia em arquivo único (Ctrl+Alt+S). A segunda passa a ser o arquivo ativo.

Arquivos únicos criados antes deste corte mantêm o perfil anterior ao salvar. Uma nova cópia do estado atual adota blocos e preserva o original. Executáveis anteriores a P5-C não materializam o novo perfil. Os controles de projetos em pasta continuam disponíveis, sem migração automática de obras.

Para reproduzir as medições em projetos fictícios:

```bash
cmake -S tests/project_file -B build-p5 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-p5 --parallel 2
python3 tests/project_file/benchmark_comparison.py /CAMINHO/NOVO/PARA/RESULTADOS
```

Execute a partir da raiz da branch. O último comando exige diretório novo, consulta o commit P5-B `e605b37` no Git local e compila uma cópia isolada dele dentro do destino. Não altera Git, fontes congeladas, projetos autorais ou configurações pessoais. Usa o mesmo gerador de benchmark nos dois executáveis e conserva todos os projetos fictícios e resultados.

A duração depende do computador e do armazenamento. Consulte os números e os limites no [relatório P5-C](../validacao/VALIDACAO-P5-C.md); não extrapole uma medição do serviço para a fluidez completa do editor.
