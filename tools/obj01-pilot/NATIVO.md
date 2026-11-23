# Objetos no editor de Escrita do INDE

A composição agora pertence ao editor de **Escrita**. A barra de especialidades reúne o trabalho anterior com **Composição**, **Tabelas**, **Fórmulas** e **Matemática**. O texto é editado pelo editor original, e as novas especialidades acompanham suas alterações. Não há outra janela de editor.

Os controles são C++/GTK4; as equações são desenhadas em Cairo/Pango. O executável não precisa de WebKit ou JavaScriptCore para essa interface. O piloto HTML continua como referência histórica de desenvolvimento.

## Abrir a amostra

```sh
bash scripts/demos/Abrir-demo-objetos-nativos.sh
```

O iniciador verifica as fontes, compila a aplicação e cria somente a amostra `dados/obj01-escrita-integrada/Projeto.inde`, se ainda não existir. Demonstrações anteriores são preservadas. Na Escrita, abra **Matemática na narrativa** e use a barra de especialidades do próprio editor.

## Roteiro de uso

1. Em **Texto**, escreva pelo editor que já existia; Semântica, Linguística, Estrutura e Vínculos continuam disponíveis.
2. Clique em **Matemática**, **Fórmulas**, **Tabelas** ou **Composição** para abrir seus instrumentos. Use **Instrumentos** para recolher a lateral e ampliar o documento.
3. Use o clique direito sobre um objeto para editar, posicionar, alinhar ou retirar. Pelo teclado, foque o objeto e use Menu ou Shift+F10. As ações não ficam ocupando o corpo do documento.
4. Para mudar um parágrafo pela composição, escolha **Editar no texto**. A Escrita volta ao mesmo parágrafo; ao retornar à composição, a alteração e sua identidade permanecem.
5. Em uma tabela, clique na célula para editar seu texto ou inserir um cartão de fórmula. Aparência, tamanho, alinhamento, linhas e colunas ficam na especialidade Tabelas.
6. Em Fórmulas, escolha Texto por condição, Guardar valor, Calcular, Alterar à distância ou Escrita livre. Condições podem comparar números ou texto. As frases podem combinar texto editável e resultados. Alterações de uso único atualizam a variável de destino e retiram seu comando.
7. Em Matemática, escolha um modelo ou digite a fonte LaTeX. A fonte permanece salva mesmo se a prévia indicar um comando não suportado. Aplicar confirma; Cancelar conserva a versão anterior.
8. Salve pelo Documento/projeto. Rascunhos precisam ser aplicados ou cancelados antes de sair ou trocar de especialidade.

A origem arquivada do Documento continua preservada, e o estado de trabalho acompanha a Escrita. O histórico textual e o de objetos são contextuais; não se apresentam como uma cronologia global única. O banco canônico continua no schema 37; não há migração de projetos autorais em lote.

A exportação dos objetos está adiada, com prioridade futura no documento HTML. O publicador ainda recusa esse conteúdo para não omitir objetos. Ligações de variáveis a entidades/pseudônimos, equivalência com Comic e arquivo `.inde` único também não estão concluídos.

Arquitetura e fronteiras: [Escrita integrada](../../docs/arquitetura/OBJ-01-ESCRITA-INTEGRADA.md). A validação técnica será registrada separadamente do aceite humano.
