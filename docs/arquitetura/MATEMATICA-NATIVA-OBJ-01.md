# OBJ-01 — Equação e integração do documento no INDE nativo

Data: 2026-11-22. Base `703ba822acd5e7acb256751481d505f7e09c9cf2`; funcionalidade `9336b3d4986360b0e15e4b0299d98101e1130915`; revisão do aplicativo `943e4d8de5a03654ca12a5db0839d66de43364bd`; configuração final de CI `04e391438a604ef3097b7deea9b3200e3d9bac3b`. Branch `feature/inde-obj-01-foundation`. O usuário autorizou acrescentar matemática inspirada em LaTeX e levar ao aplicativo real as capacidades implementadas. O corte inclui interface GTK e escrita de arquivos novos dentro de projetos sintéticos. Main e obras autorais existentes continuam preservados. [Roteiro nativo](../../tools/obj01-pilot/NATIVO.md).

## Capacidades e distinções

**Equação** é um objeto de notação matemática, distinto de **Fórmula**, que avalia valores, condições e comandos. A fonte da equação é autoral; HTML/MathML e tipografia são derivados. Não há execução TeX, shell, leitura de arquivos indicada na expressão ou conversão implícita para a linguagem de Fórmula.

A ocorrência possui ID e objeto próprio, fonte UTF-8, modo `inline`/`block`, nome e revisão. `inline` determina a tipografia compacta do objeto; não significa inserção arbitrária entre caracteres do Documento original. Não há compartilhamento de usos de uma mesma equação neste corte. Numeração e referências automáticas permanecem pendentes.

As equações participam da sequência, movimento autoral/visual, pares lado a lado, largura/alinhamentos, retirada/restauração, histórico, salvamento e recuperação. Retirar conserva fonte/identidades. O candidato completo é validado antes de confirmar; erros e revisões obsoletas não alteram parcialmente o documento.

| Operação | Entrada além de `op`/`expected` | Instrumento |
| --- | --- | --- |
| `create_equation` | `source`, `mode`, `label`, `before` | Estrutura |
| `edit_equation` | `id`, `source`, `mode`, `label` | Texto |
| mover/compor/remover/restaurar | comandos comuns e ID da ocorrência | instrumentos existentes |

Limites: 64 equações conhecidas, incluindo retiradas; fonte até 16 KiB; nome até 256 bytes; modos fechados e revisão não negativa. Os limites agregados do documento e do histórico continuam válidos. Fonte incompleta, desconhecida pelo renderizador ou contendo aparência de markup é preservada como dado; a apresentação mostra diagnóstico. O servidor não confunde sucesso de armazenamento com validade tipográfica.

## Renderizador local

KaTeX **0.19.1** está incluído com JavaScript, CSS, fontes WOFF2, licença MIT e [proveniência do pacote](../../tools/obj01-pilot/vendor/katex/PROVENANCE.json). O download foi verificado pela integridade SHA-512 do pacote e seu SHA-256 registrado. Não há CDN ou carregamento remoto para escrever/abrir matemática.

A apresentação usa `trust:false`, `strict:'error'`, `throwOnError:true`, `maxExpand:200`, `maxSize:20`, `output:'htmlAndMathml'` e macros novas por renderização. Erros entram por `textContent`; a fonte não é inserida como HTML bruto. As opções seguem as [orientações de segurança do KaTeX](https://katex.org/docs/security). O conjunto de comandos é o do renderizador, não todo o ecossistema LaTeX. Os modelos visuais incluem fração, raiz, somatório, integral, matriz e sistema alinhado.

## Entrada real no aplicativo

A ação `win.document-objects`, acessível por **Objetos** no cabeçalho do INDE, captura o documento selecionado na Escrita pelo mesmo `ProjectService` da janela. Exige que a Escrita esteja sem salvamento pendente e não força salvamento implícito apenas para abrir.

`ObjectDocumentWindow` é uma janela GTK pertencente ao aplicativo e transitória/modal em relação à janela principal. O WebKitGTK 6.0 apresenta os controles locais já exercitados no piloto. Os pedidos usam um canal com resposta para a **mesma classe C++ `Session`**, ligada ao executável. Não há subprocesso de motor, servidor Python, HTTP ou navegador externo nesse percurso. WebKit pode usar seus processos internos normais de renderização; isso não é outro editor/servidor.

O protocolo `inde://ui/` fornece somente arquivos locais permitidos. A navegação externa é recusada e a política de conteúdo limita scripts/fontes/conexões à origem local; imagens usam a origem e blobs. O canal confere a origem e limite do pedido. A avaliação/persistência ocorre em tarefas de trabalho serializadas por mutex; somente a resposta retorna ao contexto JavaScript principal. Respostas mantêm a revisão esperada e as recusas do motor. Ao destruir a janela, uma referência de vida impede callbacks de acessarem a interface destruída.

O fechamento não depende apenas de um aviso assíncrono de rascunho: consulta o editor vivo antes de esconder a janela. Rascunhos, operações em andamento e resultado de persistência incerto impedem saída sem conferência. O fechamento da janela principal também respeita a janela de Objetos. O teste demonstrou uma corrida na primeira implementação e passou depois desse ajuste.

**Salvar documento** confirma o agregado. Após fechar a janela de Objetos, **Salvar projeto** confirma também a composição pertencente ao mesmo caminho de projeto, sem salvar a composição antiga de outro projeto. Abrir/criar/fechar projeto libera a janela correspondente. A cópia recursiva existente leva os dados da composição; a captura mantém sua proveniência e as identidades dos conteúdos copiados.

## Armazenamento e vínculo de origem

O ponto de entrada nativo explícito de `Session` recebe projeto e `project::Document` reais. O construtor antigo do piloto continua recusando caminhos `.inde`; não foi liberado genericamente para escrever em projetos. A janela nativa resolve o diretório do projeto pelo serviço, valida o ID do documento e cria pastas privadas, sem links, em:

```text
Projeto.inde/
  data/project.sqlite3                # proprietário canônico, schema 37
  documents/composed/<document-id>/
    origem-documento.json             # captura completa congelada
    obj01-piloto.sqlite3               # agregado, recursos, salvo e trabalho
    .pilot.lock                       # exclusão entre sessões
```

O SQLite auxiliar mantém versão 5 e o proprietário mantém schema 37. No banco canônico, a versão é lida de `schema_migrations`; `PRAGMA user_version` permanece 0 e não deve ser confundido com o schema. Não há DDL novo de migração nesse corte.

A fonte real é capturada com o codec do proprietário, inclusive camadas e identidades, e conservada integralmente. As operações do composto não escrevem o corpo do Documento canônico. Se esse Documento mudar depois na Escrita, a abertura detecta diferença no snapshot e recusa substituir/reinterpretar a composição antiga. Ambos são preservados. A reconciliação/edição unificada ainda é uma pendência real, e impede tratar este modo como substituto irrestrito da Escrita.

A nova matemática usa `INDE-OBJ01-COMPOSITE-P2D2-5`, com `composition.equations` e `composition.removed_equations`. Os formatos anteriores mantêm sua representação até usar a capacidade nova. O motor V4 foi executado contra uma cópia V5 e recusou com `unsupported_snapshot_version`, sem alterar origem/banco.

## Publicação e abrangência

O publicador existente verifica os documentos selecionados e recusa a captura quando encontra a composição por objetos desse documento. Não publica apenas o texto antigo omitindo os novos conteúdos. A publicação anterior dos demais documentos continua funcionando, com regressão própria. Tradução de objetos para Sites/Edições ainda não foi implementada. Esse bloqueio pertence ao aplicativo desta branch: executáveis anteriores desconhecem a composição auxiliar e podem exibir/publicar apenas o corpo canônico. A compatibilidade de leitura do projeto antigo não significa suporte dos objetos novos nessas versões.

Este corte integra ao INDE as capacidades já implementadas do composto. Não conclui automaticamente serviço universal de recursos com Comic, vínculo das variáveis com entidades/pseudônimos, edição livre inline do corpo original, equações tipadas em células, catálogo/numeração de equações, publicação desses objetos ou arquivo único `.inde`. Não houve promoção a main, release ou migração de obras autorais.

## Evidência exigida

A validação separa motor, aplicação Release, integração gráfica, persistência, preservação e revisão humana. Foram exercitados testes negativos, 12 janelas de crash para matemática, recuperação, cópia de projeto, conflito da origem, abertura pela ação do INDE, figura, três equações, tabela, fórmula, comando 100→80 consumido com undo/redo, fechamento com rascunho e Salvar projeto/reabertura. A captura visual é da janela real; não constitui aceite humano do produto. Ver [relatório](../validacao/VALIDACAO-OBJ-01-MATEMATICA-NATIVA.md).


## Portabilidade de Release encontrada na integração contínua

A primeira compilação nativa limpa na CI usou GCC 13 e recusou a movimentação de temporários JSON (`std::variant`) por diagnóstico `maybe-uninitialized` em `encode_fragment`. O problema foi reproduzido localmente com os mesmos avisos/otimização. A transformação revisada de build em `base_patches.json` constrói o valor diretamente com `emplace_back`, sem alterar a fonte congelada nem desativar diagnósticos. A regressão `inde_editor_scale_tests`, que cobre copiar/colar texto formatado, passou.

A compilação limpa local também encontrou o diagnóstico em `with_inserted_column`. A construção foi reescrita para acrescentar diretamente o novo valor numérico e copiar os pesos existentes em ordem, conservando os cálculos anteriores. Os quatro programas de aparência/estrutura passaram após a correção. Essas correções são de portabilidade de construção, sem mudança de formato, migração ou supressão de avisos. A validação final identifica a revisão e a execução remota que as incluem.

A CI gráfica também encontrou uma restrição real do runner Ubuntu 24.04: o Bubblewrap do WebKit não conseguia configurar seu namespace de rede. A configuração passa a carregar o perfil restrito distribuído pelo Ubuntu e provar o namespace antes da compilação. Não há desativação do isolamento WebKit nem da restrição global de AppArmor; a mudança é exclusiva do runner descartável. A causa e a necessidade de perfil explícito constam nas [notas oficiais do Ubuntu](https://documentation.ubuntu.com/release-notes/24.04/#unprivileged-user-namespace-restrictions). A execução completa final é registrada no relatório.
