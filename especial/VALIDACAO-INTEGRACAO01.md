# Validação INT-01 · INDE Criação 0.10

Data: 02/11/2026. Estado: **integração e otimização verificadas tecnicamente**,
com demonstração sintética utilizável. Aceite humano e release pública pendentes.

## Entrega e conservação

A fonte P03c original (757 arquivos/schema 37) foi integrada à linha 0.9 do especial.
A cópia adaptada tem 758 arquivos e 18 adaptações locais, registradas por hash em
`INTEGRACAO-INDE.json`: correções estritas das fronteiras numéricas do GTK,
conversões de desenho, fila de revisão e acesso direto às alternativas.
Os 23 patches de otimização/diagnóstico da 0.9 continuam aplicados na cópia de
compilação. A base 28 original tem 641 arquivos conferidos e permanece intacta.
Os ZIPs antigos e a fonte P03c original foram preservados. Nenhuma obra autoral
foi aberta ou migrada e os diretórios de origem consultados não foram editados.

O aplicativo final tem 15.910.416 bytes, SHA-256
`c1f3133bbe7457b83a7d23ea97a6e2e3dbedb0b911c351cf9be13a686f63805c`.
É maior que o aplicativo 0.9 (13.232.256 bytes), pois incorpora planejamento e
editor ampliados. Esse tamanho não mede consumo de RAM ou fluidez.

## Compilação e regressão

| Verificação | Resultado e alcance |
|---|---|
| Release ampliada | Aplicativo e auxiliares compilados; ligação final LTO aprovada |
| Flags reais | 368 comandos / 356 arquivos C++ distintos; C++20, Wall, Wextra, Wpedantic, Wconversion, Werror e O3; sem supressões ou objetos ausentes |
| LTO | Ativo no aplicativo e ponte; auxiliares usam a parte O3 nativa dos objetos fat, como na 0.9 |
| Núcleo CTest | **47/47**, 161,51s, sem compilação ou GUI concorrentes da tarefa |
| Python | **87/87**, 28,481s, sem skips; usa serviços C++ reais |
| Focal após ampliar a demo | Passou novamente com escolhas contextuais e catálogo selecionado |
| Fonte original28 | 641 arquivos /hash do arquivo-base conferidos |
| Fonte integrada | 758 arquivos conferidos contra manifesto e arquivo próprio |

A primeira suíte Python passou 86 casos e falhou na abertura HTTP porque o sandbox
recusava bind em 127.0.0.1 (`PermissionError: Operation not permitted`). A suíte
completa repetida com loopback disponível passou 87/87. Não se alterou o produto
para contornar esse bloqueio. Os logs iniciais/finais ficam preservados no
workspace; `evidencias/integracao01/testes.log` contém os resultados terminais.

A compilação estrita encontrou conversões implícitas nos recursos incorporados.
Foram explicitadas somente nas fronteiras adequadas. As coordenadas de desenho
são limitadas pelo modelo e representáveis exatamente como double. Posições de
texto não representáveis por int são recusadas antes de chegar ao GTK; a sentinela
-1 continua válida. Aritmética inteira autoral, limites e semântica não foram
relaxados. Os percursos gráficos abaixo foram repetidos sobre esses objetos.

## Compatibilidade e publicação

Uma cópia criada pelo executável 0.9/schema 28 foi aberta pela integração e passou
para schema 37. Original e cópia: integridade SQLite ok e FK0. Hashes da amostra
original permaneceram iguais. Os registros prévios de Obra, Documentos, âncoras,
semântica e bindings foram conservados em 7 tabelas verificadas.
O teste também passou alternativas/perfil, reuso contextual, Edição, reabertura,
Salvar cópia e proteção contra overflow de posição GTK. Não é um teste com acervo
real. Resultado: `evidencias/integracao01/migracao-verificada.json`.

A publicação usa o corpo corrente integral. Perfis de trabalho não escolhem texto
público e alternativas encobertas permanecem no Projeto. O painel explica essa
regra antes da preparação. O retrato declara o schema real; o adaptador aceita
somente as bases 28/37 verificadas. O teste de integração conserva duas redações,
omite um parágrafo da visão de trabalho e confere o ZIP: corpo integral presente,
redação encoberta ausente e nenhum banco de autoria na saída pública.

## Percursos GTK e navegador

- Comic e Escrita/Publicação no executável final: navegação, rascunho, recursos,
  histórico, geração, retorno ao mesmo buffer/seleção e desfazer aprovados.
- Editor unificado: 27 fases e subetapas, temas claro/escuro e reinício em outro
  processo. Filtros, formatação, semântica lateral, clipboard/PRIMARY, arrasto,
  busca, composição IME simulada, ordem por vizinho e recuperação aprovados.
- Planejamento visual/operativo: arraste, dimensão, curva, faixas/marcos, grupo,
  prévia/interior próprio, lista, câmera, revisão de sequência e organização;
  claro/escuro e reinício aprovados.
- Contextual: 38 fases de origem entre composições, edição/retorno, referência
  derivada, edição única, seleção Unicode, ocorrência e marcação/abertura atômicas.
- Revisão: 111 Documentos sintéticos; a pendência além dos 100 iniciais apareceu na
  primeira página filtrada. Resolver e voltar à fila conservou a decisão.
- IAB: biblioteca → Obra → capítulo da Edição integrada. PUBLICO CORRENTE,
  CORPO INTEGRAL e FIM visíveis. A ausência da alternativa privada também foi
  verificada em todos os bytes do ZIP, sem depender de ocultação por CSS.

Os drivers foram ligados aos objetos reais do aplicativo. Editor, planejamento,
contextual e revisão usaram G_DEBUG=fatal-criticals; os logs finais desses percursos
não têm críticos. O aviso do módulo ibus ausente aparece nos auxiliares diretos e
não é certificado como resolvido. A aplicação pelo lançador conserva a adaptação
de ambiente da 0.9. São testes por sinais/controladores, sem certificar teclado,
IME ou arrasto físicos, leitor de tela, HiDPI, outros sistemas ou sessão longa.
Os sanitizadores históricos não foram tratados como validação nova desta integração.

## Paridade com a otimização 0.9

Três pares alternados de processos 0.9/integrado, cada um com aquecimento e cinco
repetições por cenário. Sem build ou teste gráfico concorrentes da tarefa;
o desktop permaneceu em uso normal. Mesma assinatura funcional:
`5e97f31a4c8d8e0a6273f6151bae0f360c99dbcddd435c23bbc6d2c2d942a5cc`.

| Cenário | 0.9 ms | Integrado ms | Alocações C++ por operação |
|---|---:|---:|---:|
| Busca ASCII sem distinguir caixa | 23.093 | 22.636 | 20 → 20 |
| Busca Unicode sem distinguir caixa | 21.519 | 21.764 | 20 → 20 |
| Estatísticas completas | 26.339 | 26.220 | 52 → 52 |
| Corretor aquecido / provedor sintético | 84.975 | 85.169 | 28 → 28 |
| Validação Comic | 2.472 | 2.619 | 18813 → 18813 |
| Serialização e reconstrução Comic | 12.085 | 12.694 | 52836 → 52836 |
| Ordem editorial / 10.000 unidades | 9.788 | 10.176 | 30018 → 30018 |

As contagens de alocações são iguais nos sete cenários. As pequenas variações de
latência não sustentam uma afirmação de aceleração ou regressão global; este
ensaio não foi feito em máquina dedicada e mede serviços, não digitação/GTK ou
produtividade. Alocações/bytes solicitados acumulados não equivalem a RAM viva.
Dados: `evidencias/integracao01/comparacao/resultado.json`.

PGO: recusa de USE sem perfil, GENERATE, dois perfis gravados e recompilação USE
com o módulo final e busca Unicode real passaram. O binário regular continua
O3+LTO, sem PGO. Não houve treinamento representativo do aplicativo completo.

## Demonstração, pacote e limites

`bash Abrir-demo-integrada.sh` abre amostra própria, com alternativas/perfil,
reuso em duas composições, possibilidades locais e catálogo explícito. As demos
anteriores continuam disponíveis. O roteiro está em `ROTEIRO-INTEGRACAO.md`.
O ZIP de fontes inclui os dois arquivos-base, a fonte integrada, scripts, leitor,
testes e evidências selecionadas; dados e perfis de trabalho ficam fora.
O registro pós-empacotamento é externo ao ZIP em
`evidencias/integracao01/pacote-verificado.json`, vinculado ao hash do pacote.
Configuração independente não equivale a rebuild integral independente ou
instalação em outro computador. Instalador autossuficiente e Windows pendentes.

A fila de revisão agora filtra antes de completar 50 resultados+sentinela,
consultando resumos em lotes de 50 e cancelando entre Documentos. Não é índice
persistente; páginas posteriores podem exigir nova varredura e mudanças externas
exigem Atualizar. Itens pendentes aparecem antes de decididos, sem apagar histórico.

Layouts próprios completos, nomenclatura final das alternativas, fusão futura de
modularização, P04, estados globais, economia, plantas e cartografia ampliada
permanecem trabalhos próprios. A integração não inventa essas capacidades.
Edições Comic/catálogo misto, migração restante do estúdio anterior, tradutores
autorais adicionais, publicação remota e distribuição final continuam nos limites
já declarados. Nenhum aceite humano ou promoção pública foi inferido.
