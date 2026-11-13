# Validação OPT-CXX-01 · INDE Criação 0.9

Data: 02/11/2026. Validação técnica local em Linux x86_64, GCC 15.2.0.
Base arquivada: `89b47f27e83d76e1ef61a46cb6c29b8c75c305c4`, schema 28.
As extensões de publicação (schema 3) e Comic (schema 1) conservam seus formatos.

## Compilação e cobertura

- 304 arquivos C++ distintos, em 314 comandos C++ e um cabeçalho pré-compilado.
- Todos os 315 comandos conferidos têm C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror -O3`.
- Todos os objetos previstos existem; nenhuma flag desativa avisos.
- Aplicativo e ponte ligados com LTO. Os auxiliares usam o código nativo O3 dos objetos fat, conforme `OTIMIZACAO-CXX.md`.
- Núcleo, interface, produção, publicação, Comic, testes, geradores e benchmarks habilitados foram compilados. O laboratório opcional `INDE_BUILD_TEXT_SPIKE`, desativado nesta configuração, não integra essa cobertura.
- 23 arquivos da base recebem alterações reproduzíveis na cópia de compilação. Os 641 arquivos originais conferem com a base arquivada.
- Evidências: `flags-verificadas.json`, `base-alteracoes.diff`, `base-verificada.json` e logs de compilação em `evidencias/otimizacao01/`.

## Regressão

- 33/33 suítes CTest aprovadas, com asserções habilitadas nos testes que as utilizam.
- 86/86 testes Python aprovados, incluindo os serviços C++ reais de publicação, catálogo, importação e Comic. Nenhum skip. Os 39 checks internos do serviço Comic são parte dessa execução, não testes adicionais à soma.
- A suíte de produção foi executada novamente após limitar a reserva ao teto de 1.000 documentos por unidade: aprovada.
- Comparador funcional: 62 textos UTF-8 (dois casos fixos e 60 amostras determinísticas), consultas em diferentes modos de caixa/palavra inteira, estatísticas completas, offsets e sugestões do corretor; dois casos de bytes inválidos e uma árvore editorial de 80 unidades. Assinatura anterior e final idêntica: `5e97f31a4c8d8e0a6273f6151bae0f360c99dbcddd435c23bbc6d2c2d942a5cc`.
- Percurso GTK Comic aprovado no executável final com LTO: barra Criar/Planejar/Revisar/Manuscrito/Cronologia/Mapas/Obras, escolhas reais de Criar, edição, rascunho, zoom, contexto, histórico, recursos, publicação e retorno à mesma Escrita.
- Percurso GTK Escrita/publicação de novel aprovado: bloqueios de edição pendente, cadastro, gravação, geração, seleção e desfazer preservados.
- Ambos os percursos usam projetos e perfis sintéticos separados; capturas e resultados ficam em `evidencias/otimizacao01/gtk/`.

## Medições finais

Três pares alternados de processos anterior/final. Cada processo aquece uma vez e mede cinco repetições por cenário; a tabela usa a mediana das três medianas. As compilações e regressões haviam terminado. O desktop permaneceu em uso normal; não é um ensaio em máquina dedicada.

A comparação mantém O3 e ligação sem LTO nos dois executáveis de medição, isolando as mudanças de código. A aplicação entregue usa LTO. O corretor da medição usa um provedor sintético que aceita todas as palavras, com cache aquecido; não mede o custo real do Enchant nem sugestões remotas.

| Cenário | Antes (ms) | Final (ms) | Redução de tempo | Alocações C++ por operação |
|---|---:|---:|---:|---:|
| Busca sem distinguir caixa, consulta ASCII (1 MiB) | 106.16 | 23.76 | 77.6% | 84 → 20 |
| Busca sem distinguir caixa, consulta com acentos (1 MiB) | 104.77 | 22.61 | 78.4% | 83 → 20 |
| Estatisticas de texto (algoritmo preservado) | 25.02 | 25.53 | sem ganho atribuido | 52 → 52 |
| Corretor aquecido, provedor sintetico (1 MiB) | 168.64 | 92.28 | 45.3% | 2016635 → 28 |
| Validacao de comic (80 quadros / 1.280 textos) | 31.34 | 2.60 | 91.7% | 182955 → 18813 |
| Serializar e reconstruir o mesmo comic | 54.54 | 11.33 | 79.2% | 344595 → 52836 |
| Ordenacao editorial (10.000 unidades) | 31.38 | 9.74 | 68.9% | 60032 → 30018 |

O algoritmo das estatísticas foi preservado. A variação residual de cerca de 2% nessa função não é apresentada como ganho. A tentativa anterior, com menos alocações, piorou o tempo em aproximadamente 8% e foi retirada; a coleta experimental permanece em `comparacao-estatisticas-descartada/`.

Os contadores registram chamadas C++ a `new`, incluindo as dos contêineres, e bytes solicitados acumulados. Não incluem `malloc` interno de GLib/GTK/SQLite; não medem memória viva máxima. No corretor, o volume solicitado caiu de 279.415.144 para 67.043.264 bytes por operação; isso não equivale a dizer que o aplicativo ocupava 279 MB de RAM. A estrutura Unicode privada caiu de 24 para 16 bytes nesta plataforma.

O binário local passou de 17,367,432 para 13,232,256 bytes (16.56 → 12.62 MiB; redução de 23.8%). Esse resultado pertence ao conjunto das mudanças, não a uma medição isolada do LTO.

## PGO e limites

O ensaio `tests/verificar_pgo.py` passou com o módulo CMake final e o código real de busca Unicode: rejeição de USE sem dados, GENERATE, execução com gravação de dois perfis e recompilação USE com LTO, mantendo o resultado. Os avisos de perfil ausente ou incompatível permanecem ativos.

O binário regular está em O3 + LTO, sem PGO. `Compilar-PGO.py` disponibiliza o treinamento completo em uma pasta própria e verifica fontes, compilador e destino entre fases. A sessão sintética da busca não é treinamento representativo do INDE inteiro.

Os benchmarks de serviços não medem fluidez da interface, latência de digitação ou produtividade do escritor. Os percursos gráficos são regressões técnicas, sem substituir aceite humano e uso prolongado. Não houve publicação, envio ao GitHub ou migração de projetos autorais.

Instruções de compilação e reprodução: `OTIMIZACAO-CXX.md`. A entrega de fontes contém o arquivo-base, patches, código nativo, leitor, scripts e evidências selecionadas. O empacotamento verifica os hashes antes de escrever o ZIP; o relatório externo `pacote-verificado.json` registra a extração e a configuração isoladas.
