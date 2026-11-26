# Experimentar o arquivo único P5-A

Este é o primeiro corte da fundação de armazenamento. Usa um projeto fictício novo, os serviços C++ reais e o editor GTK do INDE. Não altera obras existentes. O arquivo único ainda não é aberto diretamente pelos comandos Abrir/Salvar do editor.

## Abrir a demonstração

Execute [Abrir-demo-arquivo-unico.sh](../../scripts/demos/Abrir-demo-arquivo-unico.sh) com Bash, sem argumentos. O demonstrador compila o necessário e cria uma pasta nova em `dados/p5-demo-DATA-HORA-NÚMERO`.

O projeto contém um documento com dois parágrafos, negrito e âncora; uma tabela, equação e imagem incorporada; uma linha lado a lado com ordem visual diferente da leitura; e planejamento ligado ao documento. Antes da abertura o demonstrador já executa e confere esta sequência:

1. Criar a obra com os serviços nativos e fechar.
2. Converter para `Projeto-portatil.inde`, que é um arquivo SQLite único.
3. Materializar uma pasta em outro local e comparar os bytes.
4. Reabrir com `ProjectService`, comparar Documento completo, objetos e planejamento; editar texto e nota.
5. Registrar estado 2, restaurar estado 1 como estado 3, materializar ambos e conferir que a continuação do estado 2 permanece disponível.

Ao abrir o editor, experimente escrever, trocar de especialidade e observar os objetos. **Salvar grava a pasta `Edicao-da-demo.inde`.** Ao encerrar normalmente o editor, o demonstrador captura essa pasta no arquivo único, confere a integridade e grava os relatórios. Se a janela ou o demonstrador falhar, a pasta de trabalho e o arquivo anterior permanecem no diretório da demonstração; não se anuncia uma gravação bem-sucedida.

## O que fica disponível

| Item | O que contém |
| --- | --- |
| `Original-sintetico.inde/` | Projeto original fictício, preservado |
| `Projeto-portatil.inde` | Arquivo único, recursos incorporados e estados anteriores |
| `Estado-anterior.inde/` | Conteúdo restaurado do primeiro estado |
| `Estado-posterior.inde/` | Continuação preservada do segundo estado |
| `Edicao-da-demo.inde/` | Cópia efetivamente aberta para sua experiência |
| `verificacao.json` | Comparações dos serviços reais e preservação da origem |
| `estado-antes-da-edicao.json`, `estado-apos-edicao.json` | Estado esperado e resultado da captura final |
| `integridade-final.json`, `historico-final.json` | Conferência e lista de estados após encerrar |

O demonstrador não limpa versões de experiências anteriores e não publica conteúdo. A validação técnica automatizada não substitui sua avaliação da experiência.

## Ferramenta para conferência

O executável compilado é `build-nativo/inde_project_file`. Também pode ser compilado sem GTK:

```bash
cmake -S tests/project_file -B build-p5 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-p5 --parallel 2
ctest --test-dir build-p5 --output-on-failure
```

Comandos sobre caminhos explícitos:

```text
inde_project_file converter PASTA_DE_COPIA_FECHADA.inde ARQUIVO_NOVO.inde
inde_project_file verificar ARQUIVO.inde
inde_project_file inspecionar ARQUIVO.inde
inde_project_file historico ARQUIVO.inde
inde_project_file reabrir ARQUIVO.inde PASTA_NOVA.inde [SEQUENCIA]
inde_project_file registrar ARQUIVO.inde PASTA_DE_TRABALHO.inde SEQUENCIA_ESPERADA HASH_ESPERADO "Descrição"
inde_project_file restaurar ARQUIVO.inde SEQUENCIA_ALVO SEQUENCIA_ESPERADA HASH_ESPERADO
```

`inspecionar` inclui o conteúdo descritivo dos elementos; seu resultado não deve ser enviado a terceiros sem revisar o conteúdo. O esperado deve vir da observação anterior ao trabalho, não de uma atualização automática que contorne a recusa de conflito. O comando `restaurar` muda o estado atual do arquivo, acrescentando uma revisão; `reabrir` apenas materializa a revisão escolhida em pasta nova.

Não usar obras autorais para a primeira experiência. O [contrato](../arquitetura/ARQUIVO-UNICO-P5-A.md) descreve limites, recusas e trabalho ainda necessário para o editor usar o arquivo diretamente.
