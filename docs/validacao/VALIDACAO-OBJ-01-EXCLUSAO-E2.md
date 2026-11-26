# Exclusão no planejamento visual e continuidade do corte E2

Data: 26/11/2026. Branch: `feature/inde-obj-01-foundation`. Base anterior: `cc57abee1ad4aabfeb55614bccf4f08950227659`.

O pedido autoriza excluir elementos/composições do planejamento visual e seguir diretamente ao próximo corte da lista. O próximo trabalho remanescente era E2: inserir novos objetos depois de separar disposição visual e ordem de leitura, sem restaurar toda a apresentação. Esta entrega trabalha na superfície nativa de **Planejar → Planejamento visual** e no editor compartilhado de **Escrita**. E3, dedicado à escrita contínua, seleção e coordenação do histórico misto, continua separado.

## Exclusão no planejamento

- Selecionar um ou vários elementos e usar **Delete**, **Superfície → Excluir elementos selecionados…** ou a mesma ação no menu de contexto abre confirmação. Cancelar preserva o estado. Uma faixa não leva seus trechos consigo: só os elementos explicitamente selecionados são retirados.
- Blocos são participações do acontecimento naquela composição. Excluir apenas a geometria faria o bloco reaparecer pelo layout automático. A nova operação retira a participação e sua apresentação, preservando o acontecimento compartilhado, os interiores e os usos em outros conjuntos.
- Faixas e marcos são anotações da disposição. Sua retirada preserva o caminho existente de **Desfazer/Refazer disposição**. Retiradas que envolvem trechos usam o histórico do Projeto; a confirmação e a mensagem final explicam essa diferença.
- **Montagem → Excluir composição visual…** confirma a retirada do conjunto e de seus dados locais: participações, papéis, geometria, encaixes, opções, declarações, dependências e vínculos/referências. Os documentos e demais alvos desses vínculos permanecem. Excluir uma composição não exclui os acontecimentos compartilhados.
- Composições usadas como interior de um trecho ou ligadas a outros conjuntos por pontes são recusadas. Esta é uma proteção conservadora: ainda falta um fluxo dedicado para desvincular/excluir interiores com revisão de todos os usos. Não é implementada uma exclusão em cascata de conjuntos.
- Retirar uma participação isolada com encaixe, declaração, dependência, ponte, referência ou vínculo textual exige desvincular esses dados primeiro. A proteção antiga de `detach()` também recebe a verificação completa, inclusive referências e alvos de escrita introduzidos depois da versão original.

As operações verificam projeto e revisão dentro da transação, distinguem IDs de elementos visuais dos IDs de participação, validam toda a seleção antes de escrever, recusam identificadores ausentes/estrangeiros/repetidos e gravam uma operação reversível no histórico existente. A retirada pela lista resolve a participação para seu elemento visual antes da operação; a revisão é novamente conferida na transação. Um teste cobre IDs dos dois espaços com o mesmo valor, além de IDs visuais personalizados. Nenhuma migração foi acrescentada. Uma confirmação desatualizada mantém o diálogo aberto, explica a recusa e permite cancelar. Após excluir um conjunto, a navegação escolhe um conjunto remanescente ou a superfície vazia; os retornos e seleções obsoletos são limpos.

## Continuidade do editor: E2

O código de inserção anterior exigia `author_order == visual_order` e `insert_id()` terminava copiando a disposição para a ordem de leitura. Isso tornava uma composição lateral divergente incompatível com novas inserções.

A ligação nativa viva agora insere separadamente nas duas ordens. Os elementos que já existiam mantêm suas sequências e a linha conserva membros, identidade, proporções e intervalo. O campo `before` continua sendo o destino visual; o novo `author_before` é um destino de leitura explícito, transitório e validado. Quando omitido, usa a mesma referência nas duas ordens. Não é um novo formato persistido nem uma migração. O perfil experimental antigo continua com sua restrição original.

Os formulários de figura, tabela, equação e fórmula oferecem dois destinos quando as ordens divergem. A escolha visual exclui o segundo membro das linhas, evitando oferecer um ponto que dividiria o pareamento. Ambos os destinos podem ser o fim do documento. A operação recusa um destino de leitura inexistente e mantém a recusa de uma inserção dentro da linha visual. Fórmulas de célula não recebem um destino de inserção no documento.

A03/M09 já tinham movimento estrutural de linha na entrega anterior. Neste corte também foram trabalhados:

- **A04:** seções nomeadas no formulário de posição, explicação de que cada confirmação aplica sua seção e rodapé **Aplicar tamanho e alinhamento**, distinguindo-o de **Mover** e **Compor lado a lado**. As ações permanecem independentes e fecham o formulário ao concluir.
- **A05:** encaminhamento do alinhamento ao `Gtk::TextView` efetivamente usado pelo parágrafo dentro do `Gtk::ScrolledWindow`. A recriação também restaura o alinhamento padrão quando não há apresentação explícita. A instrumentação de teste apenas observa a propriedade real do campo.

O teste nativo cobre destinos diferentes, PNG/tabela/equação/fórmula, restauração de objeto, recusas atômicas, larguras 35/65 e intervalo 24, desfazer/refazer, continuação textual e reabertura. O percurso com mouse e teclado cobre um primeiro PNG, duas tabelas lado a lado, movimento da linha, terceira tabela e segundo PNG com destinos separados, continuação da prosa, alinhamento central efetivo e salvar/reabrir. Os testes usam documentos sintéticos.

## Invariantes e comprovação

| Invariante | Regra aplicada | Prova principal |
|---|---|---|
| Excluir no quadro atinge o elemento escolhido | IDs visuais e IDs de participação são resolvidos em espaços distintos | IDs personalizados e valores coincidentes entre espaços no teste de serviço |
| Conteúdo compartilhado permanece | Nenhuma exclusão de `contextual_moments`, interiores ou documentos alvos | Outro uso, descrição, interior e bytes do documento preservados; conferência na interface |
| Uma seleção não fica parcialmente excluída | Validar todos os IDs e vínculos antes de escrever; uma transação | Seleção mista, elemento protegido, ID estrangeiro/ausente/repetido e revisão antiga |
| A composição não rompe usos externos | Recusar interiores vinculados e pontes entre conjuntos | Serviço, reversão e integridade referencial |
| A retirada pode ser revertida | Captura pelo histórico SQLite existente; geometria pura conserva histórico de disposição | Reversão completa de participação/papel/faixa e de composição com encaixe/opção/declaração/vínculo |
| Inserir não reordena o trabalho já feito | Inserção independente nas duas ordens, com destinos validados | PNG/tabela/equação/fórmula, 35/65, recusa de corte da linha e reabertura |
| O alinhamento chega à área usada pelo escritor | Aplicar ao campo de parágrafo dentro do contêiner | Propriedade real do `Gtk::TextView`, observada após aplicação e reabertura no percurso X11 |
| O perfil anterior continua compatível | A flexibilização depende da ligação nativa viva; formato persistido e schema não mudam | 23 testes do perfil anterior em Release e com sanitizers; schema/integridade dos projetos sintéticos |

## Preservação e organização do código

A base extraída continua imutável. `planning_patches.json` contém substituições exatas revisáveis, e `base_additions/` contém dois novos arquivos de implementação. `otimizar_base.py` aplica ambos somente à cópia de compilação, verificando a quantidade de correspondências e recusando que uma adição sobreponha um arquivo original. O relatório `base-transformada.json` identifica as mudanças. O CMake registra essas dependências, inclusive as adições, para reconfigurar a compilação quando mudam.

A branch principal, projetos autorais, originais compactados e schema 37 não são alterados. A integração continua C++/GTK; exportação, arquivo único `.inde` e ampliação dos formatos de imagem não fazem parte deste corte.

## Demonstração utilizável

Execute `scripts/demos/Abrir-demo-planejamento-exclusao.sh`. O script usa um perfil próprio e um projeto fictício em `dados/obj01-demo-planejamento-exclusao/Demo-planejamento.inde`; não aceita caminhos autorais. Na primeira abertura prepara e verifica a amostra na própria interface, depois abre uma cópia anterior às exclusões. Nas aberturas seguintes preserva as suas alterações nessa cópia.

Em **Planejar → Planejamento visual**, escolha **Obra** e a composição **Composição para excluir**. Experimente selecionar o trecho reutilizado ou a faixa, pressionar Delete e cancelar; repita confirmando. Na outra composição, o trecho reutilizado continua disponível. Para retirar todo o conjunto, use **Montagem → Excluir composição visual…**. Consulte o histórico do Projeto para reverter uma retirada que envolveu trechos/composição. O roteiro não exige abrir o projeto autoral.

## Validação

Revisão executável final: `40b9baf443f5120d80318e517f66704b722b14b1`. A implementação inicial está em `c0dac1c3ce66b9d695a44222c431d1ee482fc8ef`; os ajustes do acionador e a separação final das identidades estão registrados nos commits seguintes.

| Verificação | Resultado local | Alcance |
|---|---|---|
| Suíte Python integral | **103/103**, 240,781 s | Inclui três percursos X11, planejamento GTK, publicação e preservação dos contratos existentes |
| CTest nativo | **65/65**, 155,05 s | Serviços/contexto, exclusão, inserção, escrita, recuperação, semântica e demais domínios configurados |
| Compatibilidade do perfil anterior | **23/23**, 121,54 s | Operações, HTTP local, recuperação, fórmulas, composição e tabelas |
| Mesmo perfil com ASan/UBSan e detecção de vazamentos | **23/23**, 438,86 s | Domínio anterior instrumentado; não instrumenta toda a aplicação GTK nem todos os novos caminhos nativos |
| Bancos sintéticos fechados | **7/7 íntegros**, nenhuma violação de chave estrangeira | Três projetos conservados como amostras; bancos canônicos no schema 37 |
| Demonstração normal | **Abertura e encerramento aprovados** | Atalho real, sem modo de teste na janela final; registros de planejamento preservados |
| CI remota da revisão final | **26/26 trabalhos aprovados** | Checkout e recompilação da revisão declarada; contratos em quatro configurações e aplicativo nativo |
| Fontes congeladas | **Três compactados e base integrada verificados** | Sem alteração dos originais ou da base extraída |

A CI final é a [execução 45862971684](https://github.com/Roger-Cardoso/inde/actions/runs/45862971684), da revisão declarada acima. **26/26 trabalhos aprovados**, incluindo a recompilação do aplicativo, os testes novos de planejamento/inserção e os três percursos X11. As execuções das revisões superadas foram canceladas e não entram nessa contagem.

As duas primeiras rodadas integrais tiveram duas falhas cada e não são apresentadas como aprovação. Os logs completos estão preservados, comprimidos. As falhas levaram a ajustes no acionador de entrada: barra de rolagem real e espera pelo fim da animação, enquadramento de ambos os alvos e índice do destino calculado da ordem observada após o primeiro PNG. A rodada seguinte passou em 103/103. Após a revisão das identidades, a suíte inteira e os 65 testes nativos passaram novamente; estes são os resultados da tabela.

O inventário, os hashes e o verificador ficam em [evidencias-obj01-exclusao-e2](evidencias-obj01-exclusao-e2/README.md).

## Limites e próximo corte

Esta entrega não cria exclusão global de acontecimentos compartilhados, não remove interiores encadeados em cascata e não unifica o histórico de disposição, participação, texto e objetos. A revisão de histórico misto, seleção entre parágrafos, Enter, remoção e foco após diálogos pertence a E3. A restauração de grupos textuais retirados em arranjos divergentes ainda passa por `place_text()` e mantém a exigência de alinhamento anterior; a restauração ampliada e testada neste corte é de objetos. Essa lacuna permanece explícita para E3, sem apresentar restauração global da apresentação como parte do percurso concluído de inserção. Os ensaios de 20/200 parágrafos e dados fictícios não certificam documentos autorais extensos, 2.000 parágrafos nem uma meta de resposta de até 100 ms. A confirmação humana do conforto de uso continua distinta da validação automatizada; não foi tratada como requisito de autorização para executar este pedido.
