# OBJ-01 — validação do hub de fórmulas

Data: 2026-11-22. Base `729404048b5614d86088fdf9730e630110f57c44`; implementação `700fb1fe91af472500138400dc755dc639451caf`; branch `feature/inde-obj-01-foundation`. Ver [contrato](../arquitetura/HUB-FORMULAS-OBJ-01.md), [roteiro utilizável](../../tools/obj01-pilot/HUB.md), [resultados estruturados](evidencias-obj01-hub/resultados.json) e [manifesto](evidencias-obj01-hub/SHA256SUMS).

## Resultado

Hub real com cinco caminhos: condições de texto, conta, valor guardado, alteração de uso único e escrita livre. Referências de fórmula/variável são reveladas, selecionáveis e escolhidas na criação. Título pode mudar sem romper o vínculo. A renomeação de símbolo por edição estruturada é recusada no servidor. Cartões anteriores continuam reconhecidos com sua referência original.

Comando escrito ou visual pode alterar uma variável de cartão, inclusive numérico/textual; ao aplicar, fixa o atual na fonte e se consome. Prévia mostra efeito sem modificar o estado real. Formato auxiliar 5 e linguagem 2 conservados; modelo visual é derivado da fonte, sem nova tabela/coluna e sem projeto autoral.

## Matriz local final

| Configuração | Resultado | Tempo | Evidência |
| --- | --- | --- | --- |
| GCC Debug | 15/15 programas | 102,13 s | [log](evidencias-obj01-hub/gcc-final.txt) |
| Clang Debug | 15/15 programas | 98,54 s | [log](evidencias-obj01-hub/clang-final.txt) |
| GCC ASan + UBSan | 15/15 programas | 283,80 s | [log](evidencias-obj01-hub/asan-final.txt) |
| Clang ASan + UBSan | 15/15 programas | 227,56 s | [log](evidencias-obj01-hub/clang-asan-final.txt) |

Inclui os 13 programas anteriores, uma bateria nova de núcleo com **16 casos** e a bateria HTTP com **sete casos** (quatro herdados comuns e três próprios). Não contar os quatro comuns como cenários novos. Todos os perfis anteriores, cartões e adaptador da origem foram repetidos. Compilação estrita com avisos tratados como erros; sintaxe JavaScript/Python/iniciador e diff conferidos. LeakSanitizer desativado localmente; a matriz remota o habilita.

Casos novos: variável nomeada alcançada por escrita livre; título versus símbolo; conta e valor compartilhando variáveis; alteração textual alimentando condição; prévia sem escrita com antes/depois; prévia com nome escolhido/autorreferência; escrita livre preservando comentários, CRLF e linhas vazias; cartões antigos conservados; operações de conta e seus valores atuais; edição de comando pendente; falha sem consumo parcial; campos/tipos/identidades; célula restaurando texto simples e undo restaurando comando; fonte/frase no mesmo undo; mistura alterar/resultado recusada; SIGKILL e quatro janelas reais de crash.

As quatro janelas (`after_blobs`, `after_snapshot`, `before_commit`, `after_commit`) usam o driver de falhas separado e verificam fonte do alvo + consumo. O protocolo de checkpoint e confirmação incerta preexistente continua coberto pela matriz completa. Não se prova toda falha de energia, disco ou sistema operacional.

## Roteiro observado no navegador

Foram usados o motor C++ real, servidor HTTP real e uma nova fixture sintética em `dados/obj01-piloto-hub/`:

1. O hub mostra os cinco caminhos funcionais. [Captura](evidencias-obj01-hub/hub.jpg) e [estado acessível](evidencias-obj01-hub/hub.txt).
2. Criação por cartões de **Preço da negociação**, referência `Negociacao.Preco`, inicial 16 → “Armand: preço caro.”. Referência exposta na caixa: [captura](evidencias-obj01-hub/referencias.jpg), [estado](evidencias-obj01-hub/referencias.txt).
3. Em Escrever livremente, criação de Negociar com `alterar Negociacao.Preco = 15`. Prévia 16→15 sem efeito real: [captura](evidencias-obj01-hub/comando-escrito.jpg), [estado](evidencias-obj01-hub/comando-escrito.txt). Após inserir e aplicar no documento, regra mostra “vou negociar” e comando sai do fluxo e da biblioteca: [estado após execução](evidencias-obj01-hub/apos-comando-escrito.txt).
4. Reabertura da regra revela inicial 16 e atual 15. **Preparar alteração** abre o cartão com alvo preenchido. Definir 14 gera alternativa “que pechincha” e consome o segundo comando: [captura](evidencias-obj01-hub/alteracao-visual.jpg). Desfazer restaura atual 15 e comando pendente; refazer restaura 14 e consumo: [undo](evidencias-obj01-hub/undo.txt).
5. Guardar valor cria `Personagem.HP` inicial 100. Trocar Número→Texto→Número conserva o rascunho 100. Conta Quinto consulta HP e divide por 5, resultado 20: [captura](evidencias-obj01-hub/conta.jpg).
6. Alterar um valor escolhe HP, Diminuir 20. Prévia também enumera a variável dependente da conta: [estado](evidencias-obj01-hub/cadeia-previa.txt). Ao aplicar, HP vira 80 e conta vira 16; terceiro comando é consumido.
7. Sem Save, SIGKILL foi enviado somente ao motor sintético, seguido de reinício. Working geração 21, snapshot e hash integral reaparecem iguais, Saved permanece revisão 0. Seis registros de fórmula, três consumidos, três usos ativos. Histórico reinicia vazio; integridade SQLite `ok`, chave estrangeira sem violações. [Resumo verificável](evidencias-obj01-hub/reinicio.json), [antes](evidencias-obj01-hub/antes-reinicio.txt), [depois](evidencias-obj01-hub/depois-reinicio.txt).
8. A demonstração foi reaberta com o binário final incluindo conservação CRLF, sem alterar o mesmo estado. Texto dos três usos antes/depois é idêntico em bytes: [antes](evidencias-obj01-hub/texto-antes.json), [depois](evidencias-obj01-hub/texto-depois.json). [Documento](evidencias-obj01-hub/documento-final.jpg) e [hub final](evidencias-obj01-hub/hub-final.jpg).

Os dumps legíveis de acessibilidade tiveram espaços finais normalizados; os JSONs do texto conservam os espaços autorais. O estado final é “Armand: que pechincha.”, “Vida: 80.” e “Um quinto da vida: 16 pontos.”. O nome Personagem é referência de fórmula, sem materialização de entidade narrativa.

## Compatibilidade e preservação

O teste adicional de escrita livre garante que a validação de strings dos cartões não seja aplicada indevidamente ao corpo de uma fonte arbitrária. O compilador livre valida UTF-8/tamanho, conserva comentários, linhas vazias e CRLF e deixa diagnósticos da linguagem ao motor. Reconhecimento estruturado permanece estrito; não descarta partes de fonte livre para inventar um cartão.

Não foi necessário mudar o schema auxiliar, snapshot ou linguagem. Novos cabeçalhos de autoria ficam na fonte, com vistas derivadas na API. Binários anteriores não oferecem o novo hub; títulos/metadados de autoria não são promessas de interface nova em versão antiga. Não houve conversão no lugar das seis demonstrações anteriores.

Os 12 arquivos de origem/banco dessas seis demonstrações conservaram hashes: [início](evidencias-obj01-hub/preservados-inicio.json), [fim](evidencias-obj01-hub/preservados-final.json). Manifestos A/B/C/D/D2/Cartões foram verificados nas respectivas revisões (37+39+44+49+47+41 = 257 entradas), preservando provas históricas.

Três arquivos compactados novamente verificados por hashes/manifestos e extraídos em diretório novo com bytes conferidos: [fontes](evidencias-obj01-hub/fontes.json). Auditor de recusas, cinco testes: [log](evidencias-obj01-hub/auditor-fontes.txt). Main continua em `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com a alteração de modo preexistente em `Preparar-ponte.sh` preservada. Fontes, metadados, modelo/comandos da fundação anterior, adaptadores, escrita e checkpoint não foram alterados.

A execução [45056096578](https://github.com/Roger-Cardoso/inde/actions/runs/45056096578) corresponde ao commit de implementação. [Resultado completo](evidencias-obj01-hub/ci.json); os 25 trabalhos concluíram com sucesso, no commit exato da implementação. LeakSanitizer foi habilitado nos perfis remotos de sanitizers.

## Limites e pendências

- Alterar é uma ação explícita em dois momentos: inserir prepara; aplicar executa e consome. Prévia/digitação não executam.
- Referências permanecem estáveis após criar; não há refatoração automática de nomes na escrita livre. Trocar título não troca identidade.
- Conta visual tem dois operandos/quatro operações, condições têm até 12 cartões simples. A escrita livre conserva a linguagem existente, incluindo diagnósticos reparáveis; não é biblioteca completa Excel.
- Comandos consumidos contam no limite de 64 registros conservados; não há GC definitivo nem histórico persistente de undo. Rascunho não aplicado fica somente no navegador.
- Formatos de fonte reconhecidos abrem como cartões; fontes livres continuam livres. Frases com outros resultados não são achatadas.
- Entidades, nomes alternativos e pseudônimos continuam pendentes. GTK, offsets do Documento original, publicação canônica, migração de obras e `.inde` único ficam fora. Não se iniciou Fase 3.
- Não comprovados todos os dispositivos/entradas, ampla concorrência, latência/RSS, todos os faults de armazenamento ou aceite humano. Os 61 testes nativos antigos não são apresentados como execução nova; foram repetidos os 15 programas do piloto/adaptador em quatro configurações e a matriz CI da fundação.

Conferência do pacote na raiz deste worktree: `sha256sum -c docs/validacao/evidencias-obj01-hub/SHA256SUMS`.
