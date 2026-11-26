# Evidências — retomada E1a de 26/11/2026

Código final: `a3d8159cb21076c0b39ad8279acd17d1d4ba810a`, branch `feature/inde-obj-01-foundation`. O relatório é [VALIDACAO-OBJ-01-FLUIDEZ-E1A.md](../VALIDACAO-OBJ-01-FLUIDEZ-E1A.md). Este pacote registra um corte de estabilização, sem aceite humano e sem encerrar a maturidade do editor.

## Conferência

Na raiz do checkout:

```sh
python3 docs/validacao/evidencias-obj01-fluidez-e1a/verificar.py
```

O verificador apenas lê: confere os hashes deste pacote, os 48 arquivos de implementação/integração/testes declarados em `codigo-validado.json`, os três compactados fixados e a correspondência da CI final. Não abre projetos, não recompila e não altera o banco. Um commit posterior só de documentação pode ter outro HEAD sem mudar os hashes do código validado.

## Conteúdo e alcance

- `python-99.txt`, `fluency.json` e `perfil-editor.txt`: execução local final, 99 testes, dois percursos de entrada real. As amostras de interface incluem envio X11 e observação periódica; não são latência pura de pintura.
- `historico-32.txt`: prova nativa adicional de adoção com 32 passos de estilo, Desfazer, Refazer e reabertura. O mesmo executável de testes participa da suíte final e da CI nativa.
- `sanitizer-live.cpp/.json/.txt` e `build-live-asan.txt`: prova local adicional do vínculo vivo com ASan/UBSan e detecção de vazamentos, usando o domínio e suas dependências instrumentados. Passou com saída 0. O driver liga as mesmas bibliotecas do alvo `inde_object_composite_pilot` na configuração `OBJ01_SANITIZE=ON`, substituindo seu objeto principal. O log `sanitizer-live-ambiente-restrito.txt` registra o impedimento ambiental da primeira tentativa; o mesmo binário passou fora da restrição. Não é teste da memória de toda a aplicação GTK.
- `build-final.txt`: última compilação incremental; dependências e configuração completa constam da CI.
- `fases.json`: resumo das seis fases `object_*` no percurso final inteiro; não há correlação individual de cada fase com uma tecla.
- `integridade-projeto-teste.json`: três bancos do projeto sintético final, fechado, antes de copiar outra demonstração. Os três passaram por `integrity_check` e `foreign_key_check`. Os arquivos do projeto sintético permanecem em `dados/`, fora do pacote Git.
- `benchmark-sincronizacao.cpp`, `benchmark-anterior.jsonl`, `benchmark-final.jsonl` e `benchmark.json`: ensaio sem GTK, nove amostras por versão, com 0/8/32 operações de estilo preparadas. O driver usa `ProjectService`, o comando canônico de edição e `Session`; cria projetos em diretório temporário próprio. Pode ser compilado com os mesmos includes, flags e bibliotecas do alvo `inde_object_native_tests`, substituindo somente seu objeto principal pelo driver. Os hashes dos dois executáveis locais são registrados; os executáveis não são distribuídos neste pacote.
- `referencia-anterior.json`: amostras parciais de interface antes da conservação das vistas. Não é um passe completo e não houve checksum do binário no momento daquele ensaio. Não confundir com o benchmark sem GTK, cujos dois binários foram conservados e têm hashes.
- `ci-primeira-falha.json/.txt`: a CI de `e21b1955ec38ab5fbdfa1c421b0798d35a02cb3f` falhou no novo driver; a tecla foi enviada sem confirmação da posição inicial. O registro fica separado da CI final. `codigo-primeira-iteracao.json` e `python-primeira-iteracao.txt` pertencem à primeira iteração local, não ao código final.
- `ci.json` e `ci-nativo-testes.txt`: conclusão remota e trecho final do log do trabalho nativo referente aos dois percursos reais. A matriz de domínio inclui GCC/Clang e ASan/UBSan com detecção de vazamentos; isso não equivale a instrumentação de memória de toda a aplicação GTK.
- `demo-folha-comum.png` e `demo.json`: folha aberta na aplicação normal, em cópia sintética distinta do projeto fechado de teste. A imagem comprova o estado visível, sem servir de medição de fluidez.
- `base-schema37.json` e `auditor-fontes.json`: conferência da base integrada e proveniência dos três compactados originais. A retomada não altera essas fontes nem migra schemas.
- `ambiente.json`, `resultados.json` e `codigo-validado.json`: ambiente local, síntese dos resultados e identificação do código/binário.

## Limites mantidos

Não estão certificados: resposta de até 100 ms, percentis de latência, 2.000 parágrafos, uso prolongado, histórico global, transação entre bancos ou todos os 45 casos do plano. Há ganho medido em exclusão de vazios e adoção do histórico; colagens grandes e o perfil misto ainda têm pausas. A demonstração é uma cópia sintética no editor nativo, distinta do projeto fechado usado para conferir a integridade.
