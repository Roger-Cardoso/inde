# Objetos no INDE — interface C++/GTK4

O botão **Objetos**, no cabeçalho do INDE desta branch, abre a composição do documento selecionado na **Escrita**. Os controles são widgets GTK4 escritos em C++, e as equações são desenhadas com Cairo/Pango. O aplicativo não depende de WebKit ou JavaScriptCore para essa interface. O piloto HTML separado continua disponível como laboratório e referência histórica.

A interface WebKit da entrega anterior não atendia ao requisito do usuário e foi substituída. Os relatórios anteriores registram aquele experimento; a [validação da correção GTK](../../docs/validacao/VALIDACAO-OBJ-01-GTK.md) é a referência para esta implementação.

## Abrir a demonstração nova

```sh
bash scripts/demos/Abrir-demo-objetos-nativos.sh
```

O iniciador verifica as fontes, compila o aplicativo e cria somente a amostra nova em `dados/obj01-gtk-nativo/Projeto.inde`, se ela ainda não existir. Não apaga nem substitui projetos ou demonstrações anteriores. Na Escrita, selecione **Matemática na narrativa** e use **Objetos**. Experimente nessa amostra ou em uma cópia antes de trabalhar com uma obra.

## Editar pelo GTK

A lateral reúne seleção de objetos, biblioteca de fórmulas e os instrumentos **Texto**, **Estrutura** e **Composição**. As operações específicas escolhem o instrumento correspondente no motor. Texto de origem, figura, tabela, fórmula e equação participam da mesma seleção, posição, largura e alinhamento.

- **Inserir equação:** fonte LaTeX em `Gtk::TextView`, modelos, nome, modo compacto/em destaque e prévia Cairo. Aplicar preserva a fonte, mesmo se a prévia indicar comando não suportado.
- **Inserir tabela:** título, dimensões e cabeçalho. Clique numa célula para editar seu texto ou inserir uma fórmula. O texto digitado é aplicado antes de abrir o cartão destinado à célula.
- **Inserir figura PNG:** seletor nativo de arquivo, legenda, descrição alternativa ou marca de figura decorativa.
- **Inserir fórmula/cartão:** escolha Texto por condição, Guardar valor, Calcular, Alterar à distância ou Escrita livre. As referências a variáveis e resultados são escolhidas em listas.
- **Mover / colocar lado a lado:** destino, intenção de apresentação ou sequência autoral, parceiro, lado, proporções e espaçamento; também largura, alinhamento e retorno à coluna única.
- **Retirar / restaurar:** objetos e grupos textuais ficam recuperáveis, com suas identidades e conteúdos.

**Aplicar** confirma a edição aberta e registra a operação para recuperação. **Cancelar** conserva a versão anterior. Fechar a janela, fechar o aplicativo ou salvar o projeto exige concluir a edição aberta ou a seleção de arquivo. Os campos ainda não aplicados não são uma gravação de rascunho em disco.

**Salvar documento** confirma a composição. Depois de fechar Objetos, **Salvar projeto** também confirma o trabalho aplicado desse projeto. “Reabrir salvo” exige uma escolha explícita porque substitui o trabalho posterior ao último salvamento. Desfazer/refazer atua no histórico vivo; a recuperação conserva operações confirmadas, e o histórico reinicia em outra sessão.

## Cartões e frases

Nos cartões de condição, preencha a entrada, a comparação, o valor comparado e o texto resultante. Acrescente, retire ou reordene condições. O modo numérico permite comparação de quantidade; o modo textual permite igualdade/diferença com uma entrada. Há uma frase alternativa quando nenhuma condição corresponde.

Os campos antes/depois do resultado continuam editáveis. **Editar frase** permite organizar trechos de texto e resultados ligados, sem transformar o resultado calculado em texto solto. Os trechos podem ser reordenados ou retirados. A fonte completa continua acessível em Escrita livre, conservando os campos preenchidos ao trocar de modo.

Um cartão **Alterar à distância** aponta para uma variável existente e permite definir, acrescentar ou diminuir. O valor fica registrado na fonte original; o comando sai do documento após execução bem-sucedida. Desfazer recupera o comando e o valor anterior. A biblioteca oferece consulta às variáveis e restauração do valor inicial em uma operação separada.

## Tabelas

**Aparência da tabela** oferece modelos Editorial, Caderno, Grade e Noite, fonte, tamanho, cores, cabeçalho, alternância de linhas, bordas, espaçamento interno, altura mínima, proporções de colunas, alturas individuais e detalhes da célula selecionada. A prévia usa controles GTK e não grava; Aplicar envia a alteração completa ao motor.

A largura e o alinhamento da tabela são ajustados em **Mover / colocar lado a lado**, como nos demais objetos. **Linhas e colunas** permite inserir, retirar e restaurar eixos. Conteúdo, fórmulas e estilos dos eixos retirados permanecem preservados.

## Matemática nativa

Equação apresenta notação; Fórmula calcula valores e condições. Os modelos incluem fração, raiz, somatório, integral, matriz e sistema alinhado. A fonte é conservada em UTF-8; a prévia é um desenho nativo derivado.

MicroTeX está incluído com fontes e licenças. A ponte Cairo foi adaptada para GTK4, sem compilador TeX, navegador ou processo de comandos. Antes de interpretar, a prévia limita tamanho, profundidade, células e comandos/ambientes aceitos. Macros de definição, arquivos, imagens e links externos não entram no renderizador. Fonte incompleta ou fora do subconjunto mostra diagnóstico e continua guardável/editável. Isso não equivale a todo o ecossistema LaTeX.

A fonte aceita até 16 KiB e o nome até 256 bytes; o motor mantém o limite de 64 equações conhecidas, incluindo retiradas. “Compacta” usa tipografia matemática de texto dentro do objeto; ainda não é inserção arbitrária entre caracteres do parágrafo original.

## Armazenamento e limites

O motor C++ e o armazenamento já testados são compartilhados com a fundação. A composição fica em `documents/composed/<id-do-documento>/`, dentro do projeto `.inde`, com captura de origem e SQLite auxiliar. O banco canônico continua schema 37; o auxiliar, versão 5. Não houve migração de obras ou alteração de formato por causa da troca da interface.

O corpo textual original continua sendo editado na Escrita. Se ele mudar depois de criada a composição, a reabertura recusa a diferença e preserva os dois estados. A reconciliação e a edição unificada ainda estão pendentes.

A publicação dos objetos ainda não está implementada. O publicador desta branch recusa esses documentos para evitar omitir conteúdo; versões antigas não conhecem essa proteção nem apresentam a composição nova. Use a versão desta branch para experimentar os objetos.

Continuam pendentes: vínculo de variáveis com personagens, nomes alternativos e pseudônimos; edição livre inline no corpo original; equações tipadas em células; numeração/referências automáticas; serviço comum com Comic; publicação dos objetos e `.inde` único. A validação técnica não constitui release, instalação universal ou aceite humano da interface.
