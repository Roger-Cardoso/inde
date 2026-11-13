# Disponibilidade no laboratório 0.6

Este roteiro descreve o fluxo simples PUB-03A, mantido. Para uma Obra com vários capítulos e identidades públicas verificadas, use `scripts/demos/Abrir-demo-obra-importada.sh` e `ROTEIRO-OBRA-IMPORTADA.md`. O bloqueio citado abaixo continua aplicável à importação simples sem mapa, inclusive aos Projetos 0.5 já criados.

---

# Importação comparada — PUB-03A / laboratório 0.5

Esta é uma implementação real, com janela GTK e criação pelos serviços do INDE schema 28. O corte converte um capítulo por vez para uma nova Obra em um Projeto novo. A migração de uma biblioteca/Obra inteira e a continuidade das identidades públicas pertencem ao próximo corte.

## Demo utilizável

```sh
bash scripts/demos/Abrir-demo-importacao.sh
```

1. A janela mostra dois arquivos fictícios. O HTML aplicado diz “Ainda havia tempo”; a autoria salva diz “Ainda há tempo”. Confira texto e recursos associados em ambos os lados.
2. Escolha explicitamente **Usar HTML aplicado** ou **Usar autoria salva**. A data do arquivo não decide qual fonte prevalece.
3. Preencha o nome da nova Obra e uma pasta de resultado que ainda não exista. A pasta padrão fica dentro da conferência, separada das fontes.
4. Clique **Criar Projeto importado**. As duas fontes são revalidadas por conteúdo antes da conversão. Uma mudança exige nova conferência.
5. **Abrir no INDE** abre o Projeto criado no aplicativo completo. A Obra tem estrutura editorial, capítulo, Documento e Personagem reais; fala/pensamento e perspectiva suportados usam os tipos semânticos nativos.
6. O resultado fica em `PASTA-ESCOLHIDA/Importado.inde`. Uma segunda tentativa na mesma pasta é recusada. Para repetir a demo, escolha outra pasta; o lançador não apaga o resultado anterior.

O recibo `data/importacao-literature.json` registra as fontes, seus SHA-256, a escolha e os remapeamentos do capítulo para o nó Capítulo e para o Documento, Personagem, cena/âncora e anotação. As duas fontes brutas acompanham o Projeto em `data/importacao-fontes/`. Documentos, estruturas e entidades são gravados pelos serviços C++; o Python não lê nem escreve SQLite.

## Conferir um par escolhido

```sh
bash scripts/importacao/Revisar-importacao.sh "/caminho/capitulo-aplicado.html" "/caminho/autoria-salva.narrproj"
```

O comando cria outra conferência e apenas abre a revisão. Ele não converte o acervo automaticamente nem decide entre backups/lixeira/exportações. As fontes originais permanecem em seus lugares; a comparação guarda cópias e exige que os originais continuem inalterados até a conversão. Depois de criado, o Projeto não precisa dos arquivos originais para ser aberto no INDE.

## Correspondências desta etapa

| Literature | INDE |
| --- | --- |
| Capítulo explícito e texto | Obra nova, estrutura editorial, nó Capítulo e Documento |
| Personagem com ID/nome/tipo | Entidade nativa Personagem e participação na Obra; ID novo registrado no mapa |
| Fala com emissor | Anotação tipada `inde.semantic.dialogue`, com referência à entidade nativa |
| Pensamento com pensador | Anotação tipada `inde.semantic.thought` |
| Terceira pessoa limitada com personagem explícito | Perspectiva do Documento, terceira pessoa, focalização interna e ponto de vista |
| Cena plana com intervalo e identidade | Âncora nativa com novo ID registrado |
| Negrito, itálico, sublinhado | Formatação do Documento nos mesmos intervalos Unicode |

Aceita autoria `.narrproj` versão 5 e HTML no subconjunto declarado em `importacao.py`. Múltiplos capítulos no mesmo HTML, tipos de entidade adicionais, campos sem equivalência, anotações extras, hierarquia de cenas, perspectiva local, listas, glossário, recap/prévia e estruturas inline não são descartados para viabilizar a importação: impedem a conversão daquela fonte e aparecem no diagnóstico. Nomes iguais não fundem entidades. Arquivos muito grandes, intervalos inválidos, IDs ausentes/repetidos e cópias adulteradas são recusados.

## Limites e sequência

A comparação de fontes e a migração de um capítulo estão implementadas; não constituem importação integral do acervo. Nenhum texto autoral existente foi convertido nesta validação. A inspeção do export autoral encontrado no workspace identificou recursos que ainda exigem tradutores; ele não foi tratado como se fosse a demo suportada.

O ID interno novo do INDE não prova continuidade de URLs, números, progresso e discussões do site anterior. Por isso, **preparar publicação desta Obra é bloqueado enquanto suas identidades públicas estiverem pendentes**. Não edite o recibo para contornar o bloqueio. O próximo corte PUB-03B deve reconciliar identidades públicas, estruturar múltiplos capítulos e ampliar os tradutores a partir de um conjunto de fontes escolhido pelo autor.

As configurações de Site/Edição e o catálogo já entregues continuam disponíveis para as Obras nativas da demo. Não houve sincronização bidirecional, envio GitHub, implantação, instalador completo, ensaio de interrupção em todos os pontos ou instalação física em outro computador.
