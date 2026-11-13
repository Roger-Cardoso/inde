# COMIC-01 · primeira integração nativa do estúdio

Data: 02/11/2026. Base: laboratório INDE Publicação 0.6 / PUB-03B, C++20, GTKmm 4, fonte congelada do INDE schema 28. Este corte inicia a expansão; não é a migração integral do estúdio Python 0.4.

## Resultado do corte

O mesmo aplicativo INDE ganha a aba **Publicação → Comics**. A sessão fornece as Obras, os Documentos disponíveis para cópia explícita e os Sites já cadastrados. O modelo comic, validação, geometria de encaixe, persistência, revisão e geração de HTML estão em C++20. Não há processo do estúdio Python, iframe de editor ou dependência da pasta da prova de conceito.

O leitor continua sendo HTML/CSS/JavaScript. O fluxo existente de publicação em Python materializa os arquivos públicos preparados pelo C++, gera o manifesto/ZIP e serve a prévia. Python não interpreta nem grava a autoria comic ou o SQLite dessa extensão.

## Autoridades e persistência

| Informação | Autoridade neste corte |
| --- | --- |
| Projeto, Obra, Documento, personagem e texto de novel | Serviços autorais existentes do INDE. |
| Quadros, elementos, texto específico da comic e recursos incorporados | Serviço C++ `inde::comic::Service`, vinculado à Obra nativa. |
| Identidade, autoria pública, tema, origem e caminho base do Site | Serviço de publicação já existente. |
| Arquivos HTML e pacote público | Saída derivada de uma preparação congelada. |
| Tradução automática e fonte de sistema | Navegador/dispositivo do leitor; o INDE não certifica o resultado. |

A extensão é `data/comics.sqlite3`, schema 1, dentro do `.inde`. O banco autoral permanece schema 28; a extensão de publicação permanece schema 3. Consultar um Projeto sem comics não cria a extensão. Alterações em comics não escrevem o banco autoral.

Cada comic tem identidade própria, Obra vinculada e revisão. A gravação exige a revisão observada e recusa conflito. Retirar/restaurar mantém identidade e conteúdo; o histórico guarda os estados anteriores, mas este corte não oferece uma tela de restauração seletiva do histórico. Descartar recupera o último estado salvo. Remover quadro/elemento é alteração pendente até salvar. Não há undo/redo próprio da composição comic neste corte.

Salvar cópia pelo INDE atualizado adota a extensão para a nova identidade de Projeto e invalida suas preparações. O destino remoto continua sob as regras de cópia da publicação existente. A extensão não autoriza publicação remota.

## Ponte com a escrita

**Copiar texto do Documento** é uma ação explícita. Cria um elemento com cópia do texto e registra ID/hash de origem no modelo privado. Editar esse elemento não reescreve o Documento. Alterar o Documento posteriormente não atualiza a comic automaticamente. A cópia deve ser revisada pelo autor.

A lista permite escolher os Documentos do Projeto. O vínculo ao Documento é proveniência de uma cópia, não uma relação semântica nova, um locutor nativo ou uma presença de personagem. O nome de locutor deste corte é apenas identificação textual.

## Modelo nativo entregue

- Comic: Obra nativa, título, slug, idioma, montagem vertical/mosaico, revisão e retirada.
- Quadro: identidade, título, descrição da arte, imagem, cor e proporção.
- Elemento: balão, texto livre ou onomatopeia; texto comum, legenda de sentido, locutor, idioma, direção, permissão de tradução, forma, posição/tamanho percentual, alinhamento horizontal/vertical, tamanho/fonte, cores e animação simples.
- Recursos: imagens PNG/JPEG/WebP/GIF e fontes TTF/OTF/WOFF/WOFF2, incorporadas ao Projeto. A fonte tem papel geral ou idioma para fallback.
- Editor GTK: lista de comics/quadros/elementos, criação/remoção, edição textual/geométrica, canvas Cairo/Pango, arraste e guia de centro, importação de arte/fontes, salvamento, descarte, retirada/restauração e geração/prévia.

Neste corte, algumas propriedades do modelo (cor, proporção e família genérica) têm valores padrão e ainda não dispõem de controles próprios na interface. A prioridade das fontes complementares segue a ordem estável dos recursos; não há gestão visual de reordenação/remoção de fontes. GIF aparece estático no canvas GTK e pode animar no leitor. Fontes incorporadas são aplicadas pelo leitor web; o canvas GTK usa fontes instaladas e é uma aproximação tipográfica. A prévia web é a referência para a leitura.

Os textos da composição são texto comum; o C++ gera e escapa o HTML. Esta primeira integração não importa HTML editorial livre nem converte projetos `.webcomic.json` da prova de conceito. Esses arquivos devem ser conservados no estúdio 0.4 até existir um tradutor completo e uma revisão comparada.

Limites: até 100 quadros, 100 elementos por quadro, 80 recursos, imagens até 12 MiB, fontes até 32 MiB e total de recursos até 128 MiB. Imagens têm validação de decodificação/dimensões. Fontes têm verificação de formato/cabeçalho; isso não é análise completa do arquivo nem certificação de cobertura Unicode. Não se transfere o diagnóstico cmap da versão 0.4 como se já estivesse portado.

## Publicação compartilhada e saída

O autor escolhe um Site existente. A preparação guarda a revisão do comic e do Site, com hash dos arquivos públicos. Alterações em qualquer um invalidam a preparação. A geração usa uma pasta nova, sem substituir saídas anteriores. O empacotamento público exclui SQLite, origem dos Documentos, retratos, histórico e destino GitHub.

O contrato de saída é `inde.web.comic`, versão 1, com `media: comic`. Contém arquivos públicos preparados em C++, não o modelo de autoria. O consumidor reconhece explicitamente esse contrato; o protocolo de novels continua sendo aceito separadamente. Caminhos públicos são validados antes da materialização. A publicação estática oferece:

- entrada da biblioteca com nome/autoria/descrição do Site;
- leitor da comic no slug escolhido;
- página, vertical, celular e quadro a quadro;
- texto real com idioma/direção/permissão de tradução;
- legenda de sentido da onomatopeia e `texto.html` sem scripts;
- fontes locais/fallback por idioma, movimentos simples e respeito a movimento reduzido;
- manifesto de hashes, ZIP, origem/caminho base e CNAME seguindo a configuração existente.

Os arquivos do leitor reaproveitados da prova de conceito estão identificados em `recursos/comic/PROVENIENCIA.json`. A forma Grito e Pensamento são representações iniciais em CSS (borda reforçada/tracejada), não equivalência integral aos contornos e caudas do estúdio 0.4.

## Sequência para a união das mídias

1. **Catálogo misto com seleção tipada.** Uma entrada deve declarar mídia, identidade de edição, Obra, rota e revisão. Reaproveitar a seleção do Site sem tratar um ID comic como Edição de novel. Detectar colisões de rotas entre ambas antes da promoção atômica do site inteiro.
2. **Edições comic e episódios.** Separar composição autoral da seleção pública, incluir/excluir quadros/episódios, preservar reservas de URL e metadados de progresso. A preparação atual publica o comic inteiro escolhido, sem seleção por episódio.
3. **Ponte editorial de texto.** Seleção de trechos/versões de Documentos, decisão explícita sobre sincronização, referências semânticas nativas e revisão do efeito das mudanças. Não inferir ligação de personagem a partir do nome digitado.
4. **Equivalência do estúdio 0.4.** Caudas/conexões, montagem livre de página, novos contornos, cálculo de tamanho por cenários, snap entre elementos, rich text internacional e diagnóstico de glifos. Só então importar o formato 0.4 com relatório de correspondência e perdas bloqueadas.
5. **Leitura e distribuição.** Identidade visual unificada do catálogo, progresso por mídia, versões editoriais por idioma, teste real de tradução, instalação limpa e portabilidade. Publicar remotamente continua sendo uma ação posterior específica.

Essa sequência registra a direção solicitada. A primeira entrega não afirma que o catálogo misto ou a equivalência integral do estúdio estejam concluídos.
