# Objetos no editor de Escrita do INDE


A versão atual usa uma folha comum em todas as especialidades. Abra com
`scripts/demos/Abrir-demo-folha-comum.sh`; a demonstração sintética fica em
`dados/obj01-folha-comum-r5/Projeto.inde`, com perfil separado. As amostras anteriores
continuam preservadas. Veja [contrato e limites](../../docs/arquitetura/OBJ-01-FOLHA-COMUM.md).

Roteiro curto: abra **Matemática na narrativa**, alterne Texto, Composição e
Semântica, observe que tabelas/equações continuam presentes, digite diretamente
em um parágrafo, desfaça pela escrita e use o clique direito para ajustar um objeto.
A seleção por arrasto entre vários parágrafos ainda tem limites; não se declara
paridade completa de gestos com o editor linear anterior.


A composição agora pertence ao editor de **Escrita**. A barra de especialidades reúne o trabalho anterior com **Composição**, **Tabelas**, **Fórmulas** e **Matemática**. Os parágrafos são editados diretamente nessa folha, usando os guardas e o histórico textual do editor original. Não há outra janela de editor.

Os controles são C++/GTK4; as equações são desenhadas em Cairo/Pango. O executável não precisa de WebKit ou JavaScriptCore para essa interface. O piloto HTML continua como referência histórica de desenvolvimento.

## Abrir a amostra

```sh
bash scripts/demos/Abrir-demo-folha-comum.sh
```

O iniciador verifica as fontes, compila a aplicação e cria somente a amostra `dados/obj01-folha-comum-r5/Projeto.inde`, se ainda não existir. Demonstrações anteriores são preservadas. Na Escrita, abra **Matemática na narrativa** e use a barra de especialidades do próprio editor.

## Roteiro de uso

1. Em **Texto**, escreva diretamente na folha mista. Tabelas, figuras, fórmulas e equações continuam visíveis ao alternar Semântica, Linguística, Estrutura e Vínculos.
2. Clique em **Matemática**, **Fórmulas**, **Tabelas** ou **Composição** para abrir seus instrumentos. Use **Instrumentos** para recolher a lateral e ampliar o documento.
3. Use o clique direito sobre um objeto para editar, posicionar, alinhar ou retirar. Pelo teclado, foque o objeto e use Menu ou Shift+F10. As ações não ficam ocupando o corpo do documento.
4. Em qualquer especialidade, clique no parágrafo para escrever. O menu **Escrever neste parágrafo** também leva ao campo da mesma folha. Enter cria outro parágrafo; o histórico textual continua no Desfazer da Escrita.
5. Em uma tabela, clique na célula para editar seu texto ou inserir um cartão de fórmula. Aparência, tamanho, alinhamento, linhas e colunas ficam na especialidade Tabelas.
6. Em Fórmulas, escolha Texto por condição, Guardar valor, Calcular, Alterar à distância ou Escrita livre. Condições podem comparar números ou texto. As frases podem combinar texto editável e resultados. Alterações de uso único atualizam a variável de destino e retiram seu comando.
7. Em Matemática, escolha um modelo ou digite a fonte LaTeX. A fonte permanece salva mesmo se a prévia indicar um comando não suportado. Aplicar confirma; Cancelar conserva a versão anterior.
8. Salve pelo Documento/projeto. Rascunhos precisam ser aplicados ou cancelados antes de sair ou trocar de especialidade.

A origem arquivada do Documento continua preservada, e o estado de trabalho acompanha a Escrita. O histórico textual e o de objetos são contextuais; não se apresentam como uma cronologia global única. O banco canônico continua no schema 37; não há migração de projetos autorais em lote.

Durante a edição de um objeto, a lateral é recolhida e os campos usam a área central; Aplicar ou Cancelar devolve a apresentação do Documento.

A exportação dos objetos está adiada, com prioridade futura no documento HTML. O publicador ainda recusa esse conteúdo para não omitir objetos. Ligações de variáveis a entidades/pseudônimos, equivalência com Comic e arquivo `.inde` único também não estão concluídos.

Arquitetura e fronteiras: [Folha comum](../../docs/arquitetura/OBJ-01-FOLHA-COMUM.md). Veja também a [validação desta integração](../../docs/validacao/VALIDACAO-OBJ-01-FOLHA-COMUM.md), separada do aceite humano.

Para documentos com objetos novos, use a aplicação desta branch. Versões anteriores não conhecem a composição auxiliar nem a proteção de exportação que evita omiti-la.
