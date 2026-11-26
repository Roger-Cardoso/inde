# Evidências — exclusão visual e continuidade de E2

O [relatório](../VALIDACAO-OBJ-01-EXCLUSAO-E2.md) descreve o alcance e os limites. Dados usados pelas interfaces são fictícios. Projetos e binários completos ficam em `dados/`, fora do Git; capturas, logs e inventários abaixo permitem conferir o resultado sem abrir projetos autorais.

- `ctest-nativo-65.txt`: regressão dos serviços e domínios nativos, incluindo o planejamento. `compatibilidade-23.txt` e `compatibilidade-asan-23.txt`: contratos do perfil anterior, com instrumentação ASan/UBSan e detecção de vazamentos na segunda execução. Esses sanitizers não instrumentam toda a aplicação GTK nem certificam todos os novos caminhos de exclusão/inserção nativa.
- `python-103.txt`: suíte final, incluindo os três percursos X11 e os testes novos da interface de planejamento. O smoke de planejamento aciona controles GTK de produção; os percursos X11 usam mouse, teclado e clipboard reais, sem setters de texto/estado ou emissão de sinais.
- `planejamento-exclusao.log`: cancelar, retirar trecho preservando outro uso, retirar faixa, desfazer disposição, cancelar exclusão de conjunto, recusar revisão desatualizada, excluir e encerrar. As duas capturas mostram a confirmação e o conjunto remanescente.
- `reordering.json` e `fluency.json`: resultados observados dos percursos. Os tempos incluem o acionador e o observador de baixa prioridade; não são latência pura de pintura, não demonstram a meta de 100 ms e não devem ser comparados como benchmark sem controlar carga e ambiente.
- `primeira-suite-103.txt.gz`, `segunda-suite-103.txt.gz` e `rodadas-anteriores.json`: as duas rodadas amplas que falharam, preservadas sem contar como aprovação. Os ajustes do acionador esperam a rolagem terminar, usam a barra real para retornar ao início e selecionam o destino pelo índice do alvo observado. A inclusão do primeiro PNG exigiu manter visíveis os dois alvos do arrasto e recalcular a escolha do segundo PNG. Os critérios funcionais não foram retirados.
- `fontes-compactadas.json`, `base-schema37.json`: arquivos compactados e base congelada verificados. `codigo.json`: hashes dos arquivos de implementação e teste; o commit identifica a revisão executável, anterior ao registro documental. `integridade.json`: bancos das cópias sintéticas, consultados fechados e em modo somente leitura.
- `ci-final.json` e `ci-nativo-testes.txt`: trabalhos e trecho dos testes do aplicativo da CI associados à revisão final. As execuções das três revisões anteriores foram canceladas após serem superadas; nenhuma é promovida a resultado final.
- `build-*.txt`: compilações incrementais locais; a CI recompila a aplicação a partir do checkout. `resultados.json`: índice dos resultados e proveniência.

Depois de atualizar todos os arquivos finais, execute:

```sh
python3 docs/validacao/evidencias-obj01-exclusao-e2/verificar.py
```

O verificador confere os hashes das evidências e fontes, os três compactados, os resultados registrados e a associação dos 26 trabalhos aprovados ao commit. A modificação posterior de um arquivo de implementação exige regenerar o manifesto e repetir as verificações pertinentes; não se deve apenas aceitar o hash novo.
