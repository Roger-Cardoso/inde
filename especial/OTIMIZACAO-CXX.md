# Otimização C++ do INDE · OPT-CXX-01

Este corte aplica o perfil de compilação estrito ao INDE presente nesta entrega,
incluindo núcleo, interface GTK, produção, publicação, Comic, ferramentas e testes
nativos habilitados. Não altera outro checkout do INDE nem os projetos autorais.

## Perfil de compilação

Todos os alvos C++ recebem C++20 sem extensões GNU, `-Wall -Wextra -Wpedantic
-Wconversion -Werror`. Release usa `-O3`, que já existia na versão anterior, e
agora também LTO no aplicativo e na ponte. O CMake verifica o suporte do compilador/linker antes de
habilitá-lo. Não são usados `-ffast-math`, `-march=native`, empacotamento forçado
de estruturas ou desativação de avisos.

O exemplo de um único `main.cpp` é representado por opções de todos os alvos
CMake: o INDE tem centenas de unidades de compilação e depende de GTK, SQLite,
GLib e ICU. A aplicação continua sendo compilada como um conjunto de componentes.

Os lançadores existentes usam esse perfil por padrão. Para compilar e conferir
também as ferramentas e a regressão completa, a partir desta pasta:

```sh
python3 verificar_base.py --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DINDE_STRICT_WARNINGS=ON -DINDE_ENABLE_LTO=ON \
  -DINDE_CODE_OPTIMIZATIONS=ON -DINDE_BUILD_CORE_TESTS=ON
cmake --build build-nativo --target inde_all_native_checks inde_web_bridge \
  inde_comic_tests inde_comic_demo inde_publication_tests inde_catalog_tests \
  inde_import_tests inde_work_import_tests inde_publication_ui_smoke \
  inde_optimization_benchmark inde_optimization_equivalence --parallel 2
ctest --test-dir build-nativo --output-on-failure
python3 -m unittest discover -s tests -v
```

`INDE_CODE_OPTIMIZATIONS=OFF` permite comparar os algoritmos anteriores, mantendo
as correções de diagnósticos. `INDE_ENABLE_LTO=OFF` permite uma comparação sem LTO.
As opções são independentes. Perfis Debug conservam sua finalidade de depuração;
`-O3` e LTO são aplicados a Release.

Com GCC, os objetos contêm a representação LTO e código nativo otimizado
(`-ffat-lto-objects`). Aplicativo e ponte usam LTO na ligação. Testes, geradores
e medições usam a parte nativa (`-fno-lto` na ligação), conservando os avisos
estritos e `-O3`. Isso evita otimizar o programa inteiro novamente para cada
executável auxiliar. O percurso gráfico exercita o aplicativo final com LTO.

A interface também reutiliza cabeçalhos GTK pré-compilados. Esse cache ocupa
cerca de 224 MiB nesta máquina e reduz trabalho repetido de compilação; não faz
parte do executável distribuído. Pode ser desligado com `-DINDE_USE_GTK_PCH=OFF`.

## Alterações de memória e processamento

- Busca: reserva das coleções com a contagem de caracteres Unicode; caminho
  ASCII sem alocar uma string GLib a cada caractere; case folding com expansão
  continua usando GLib nos caracteres que o exigem.
- Corretor: unidade Unicode interna reduzida de 24 para 16 bytes nesta plataforma
  de 64 bits. A posição do caractere é o índice do vetor, eliminando um campo
  redundante. A conversão de caixa percorre os caracteres diretamente, sem
  construir um vetor temporário para cada palavra. A busca do token no início
  de uma frase passa de linear a binária. Cache, cancelamento e worker existentes
  são conservados.
- Produção: índices temporários apontam para nós e Documentos de entrada durante
  a operação síncrona. Documentos escolhidos são copiados uma vez para o resultado
  que precisa possuí-los; o índice não duplica todos os corpos de texto.
- Comic/publicação: expressões regulares e tabelas constantes reutilizadas,
  mensagens de validação sem criar uma string proprietária no caminho de sucesso,
  reserva de arrays e transferência de coleções e elementos com `std::move`.
  Escape HTML acrescenta os caracteres diretamente ao destino.

O tamanho de um tipo não determina sozinho seu alinhamento. Reordenar membros
nem sempre reduz o objeto; pode só deslocar o padding. Aqui, a redução comprovada
veio de remover informação redundante de uma estrutura privada. Estruturas
públicas e inicializadores posicionais não foram reordenados indiscriminadamente.

Uma alocação também não implica necessariamente uma chamada ao sistema
operacional: o alocador pode reutilizar blocos. Os contadores medem chamadas C++
a `new` e bytes solicitados acumulados. Não equivalem à memória viva máxima e
não incluem as alocações C internas de GLib/GTK/SQLite.

## PGO

PGO é opcional e separado da compilação regular. O fluxo GCC usa a mesma pasta
nas duas fases e impede misturar fontes ou compiladores diferentes:

```sh
python3 Compilar-PGO.py gerar --pasta /tmp/inde-pgo-treino
python3 ambiente.py --perfil /tmp/inde-perfil-pgo --recursos . \
  /tmp/inde-pgo-treino/inde-especial
# Exercite o programa e feche normalmente para gravar os perfis.
python3 Compilar-PGO.py usar --pasta /tmp/inde-pgo-treino
```

Escolha uma pasta nova no primeiro comando. O treinamento deve incluir escrita,
digitação, busca, planejamento, revisão, Comic e publicação, com dados e tamanhos
representativos. Não mude as fontes entre as duas fases. O executável final fica
na pasta indicada; o fluxo não substitui automaticamente o executável regular.

O módulo também rejeita PGO USE sem dados e mantém os diagnósticos de perfis
ausentes ou inconsistentes. O ensaio automatizado usa o código real de busca
Unicode para verificar GENERATE → execução → USE com LTO. Esse ensaio comprova
o mecanismo; não constitui treinamento representativo do aplicativo completo.

Referências do compilador: [avisos GCC](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html)
e [otimização/LTO/PGO GCC](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html).

## Base, evidência e limites

Os 641 arquivos arquivados permanecem preservados. `cpp/base_patches.json`
registra substituições exatas, com falha se a origem esperada divergir;
`cpp/otimizar_base.py` materializa a versão alterada dentro da pasta de compilação.
O aplicativo compila essa versão e os componentes nativos de publicação/Comic.
Não edite a cópia gerada: ela será reproduzida a partir da base e das alterações.

A tentativa de alterar a análise estatística foi descartada: no cenário medido,
ela reduzia poucas alocações, mas aumentava o tempo em aproximadamente 8%.
Essa função conserva o algoritmo anterior.

O relatório `VALIDACAO-OPT-CXX-01.md` registra resultados, comandos e limites das
medições. Os números de serviços isolados não medem latência de digitação,
fluidez da interface ou produtividade do escritor. Concorrência adicional não
foi introduzida por regra geral: as tarefas já separadas da interface continuam
usando os mecanismos existentes.
