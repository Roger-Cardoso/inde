# Catálogo com várias Obras — demo 0.4

Abra a amostra independente:

```sh
bash scripts/demos/Abrir-demo-catalogo.sh
```

A primeira abertura cria `demo-catalogo/Publicacao.inde` e um perfil próprio. A amostra anterior e seus cadastros ficam preservados. O Projeto novo contém “A estação das marés”, “O observatório das estrelas” e “Arquivo de bordo”. As duas primeiras estão no catálogo; a terceira está desmarcada. Mara é a mesma entidade nativa nas duas Obras selecionadas.

1. Clique em **Publicação** e abra **Catálogo do Site**. Confira as duas Edições marcadas e a terceira desmarcada. Os botões **Subir** e **Descer** mudam a ordem local do formulário.
2. Clique em **Salvar catálogo**. Essa gravação conserva a seleção e a ordem no Projeto. Trocar de aba, sair ou gerar exige salvar ou descartar alterações pendentes.
3. Clique em **Preparar Site**. O gerador prepara todas as Obras escolhidas, confere as fontes e cria um único site e um único `site-publico.zip`. A geração individual continua disponível na aba **Edição e fontes**, pelo botão **Gerar Edição**.
4. Use **Abrir prévia**. A biblioteca mostra as duas Obras na ordem salva. Abra os capítulos de ambas; nos Auxílios, ative a identificação de locutores para conferir Mara. Os textos e as marcações vêm dos Documentos do INDE.
5. Volte ao INDE, desmarque uma Edição no catálogo, salve e prepare outra saída. A nova biblioteca contém somente a Obra restante; a pasta gerada anteriormente permanece como saída anterior.
6. Desmarque todas, salve e prepare. Essa ação produz uma biblioteca vazia. Nenhuma Edição é incluída automaticamente, nem mesmo quando é o único cadastro do Site.
7. Para retirar uma Edição, remova primeiro seu vínculo no catálogo e salve. Depois use **Retirar Edição** na aba própria. A restauração recupera a Edição e seus números, mas a inclusão no catálogo volta a exigir escolha explícita.
8. Feche e reabra a aplicação para conferir a persistência. Reordenar ou retirar e recolocar a mesma Edição mantém seus números e endereços públicos. Uma alteração nas fontes selecionadas exige preparar novamente antes de apresentar a saída como atual.

Para começar em um Projeto novo, use `bash Abrir-INDE-especial.sh --vazio`, cadastre suas Obras e Documentos pelos workspaces do INDE e crie um Site. Salve uma Edição para cada Obra que deseja publicar e depois monte o catálogo. É permitida apenas uma Edição de cada Obra por catálogo, para manter a identidade do leitor e evitar versões concorrentes na mesma biblioteca.

## Atualização dos projetos da entrega 0.3

A extensão schema 1 abre para consulta sem ser alterada e sem ganhar seleção automática. Na primeira operação de gravação, a versão 0.4 cria uma cópia consistente em `data/publicacao-recuperacao/schema1-<id>.sqlite3`, confere sua integridade e atualiza a extensão para schema 2 em transação. Cadastros, identidades, revisões existentes, preparações e reservas são preservados. O banco autoral do INDE continua no schema 28.

A configuração antiga passa a ter um catálogo vazio. Marque explicitamente as Edições desejadas e salve antes de preparar o Site combinado. A produção individual mantém seu percurso anterior.

Para transportar, feche a aplicação e copie a pasta `.inde` completa. Para compartilhar a biblioteca, use somente a pasta `site/` ou o ZIP público. Retratos, histórico, banco e cópias de recuperação pertencem ao Projeto privado. Uma retirada local não remove cópias já hospedadas ou baixadas.

## Limites

Cada Edição ainda seleciona uma Obra e uma estrutura. O catálogo reúne até 100 Obras; o tamanho e os limites de preparação também são conferidos, sem truncamento. A projeção aceita o subconjunto descrito em `ROTEIRO-GTK.md`; campos sem equivalência são recusados. Não são inventadas avaliações de conteúdo para as cenas: a saída indica avaliação pendente quando não existe avaliação autoral traduzida.

Origem, caminho base, tema, repositório e branch pertencem ao Site. Autenticação, envio e implantação continuam pendentes. O leitor está incluído, mas Python e as dependências nativas ainda precisam estar disponíveis; este pacote não é um instalador universal.
