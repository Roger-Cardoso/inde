# P2-C — Trabalho protegido e ordem textual

Na branch `feature/inde-obj-01-foundation`, execute:

```sh
bash scripts/demos/Abrir-demo-obj01-p2c.sh
```

Abra o endereço completo informado por `Demo P2C:`. O servidor aceita somente conexões locais em `127.0.0.1`. Na primeira abertura padrão, se existir a demo P2-B, o iniciador copia somente seu estado salvo, por conexão SQLite somente leitura, para `dados/obj01-piloto-p2c/`. P2-A e P2-B continuam com suas próprias pastas e iniciadores. A cópia exige destino novo e não converte bancos anteriores.

Para começar com uma fixture independente:

```sh
bash scripts/demos/Abrir-demo-obj01-p2c.sh --workspace /tmp/meu-piloto-p2c
```

Para copiar explicitamente uma fixture experimental P2-B:

```sh
bash scripts/demos/Abrir-demo-obj01-p2c.sh --workspace /tmp/copia-p2c --seed-p2b /caminho/da/fixture-p2b
```

Não use pastas de obras. Caminhos `.inde`, bancos sem identificação e versões incompatíveis são recusados. Esta interface é um experimento com núcleo C++ real, dados sintéticos e armazenamento auxiliar; não é o editor GTK integrado.

## Percurso de demonstração

1. Em **Estrutura**, insira a figura de exemplo e uma tabela 2 × 3. Se veio da demo P2-B, esses objetos já podem estar presentes.
2. Mude para **Texto**, clique numa célula e edite. Aguarde o indicador **Trabalho protegido · não salvo**. A edição completa recebida pelo navegador é enviada ao núcleo; eventos de composição de texto aguardam sua conclusão. Tab/Shift+Tab e Ctrl+Enter continuam disponíveis.
3. Em **Estrutura**, escolha um **Grupo completo** e um destino, depois **Mover grupo textual**. O motor textual existente move o grupo com suas camadas. Um grupo de vários parágrafos não pode ser partido. O movimento desfaz somente os arranjos lado a lado dos parágrafos movidos. Ordens autoral/visual divergentes precisam ser alinhadas antes do movimento autoral.
4. Em **Composição**, selecione uma figura e ajuste seu arranjo. A seleção por identidade é compartilhada pelos instrumentos. Desfazer/refazer preserva o instrumento atual e registra o estado resultante para recuperação.
5. Encerre o servidor sem usar Salvar, depois reabra a mesma pasta com o iniciador. O trabalho confirmado é reconstruído e aparece o aviso **Trabalho não salvo recuperado**. O histórico da sessão começa vazio.
6. **Salvar documento** confirma o trabalho como estado autoral salvo. **Reabrir trabalho** confere o registro recuperável. **Reabrir salvo** descarta explicitamente as alterações posteriores ao último Salvar. Ambos reiniciam o histórico da sessão; a interface pede confirmação quando há descarte.
7. **Verificar compatibilidade de publicação** apresenta uma recusa explícita: o piloto não tem tradução para o publicador antigo. Não produz arquivo nem publicação.

## O que o indicador significa

Cada operação aceita em P2-C grava o agregado e seus recursos em uma transação antes de confirmar o resultado na interface. O trabalho protegido pode divergir do estado salvo. Seleção e instrumento também fazem parte do registro e podem marcar diferença em relação ao salvo.

O indicador **Texto ainda sendo aplicado** não confirma proteção. Texto que ainda exista somente no campo do navegador, uma resposta pendente ou uma composição de teclado incompleta pode se perder se o navegador for encerrado abruptamente. A interface avisa ao sair com rascunho pendente ou erro de gravação. Não se promete proteção de cada tecla física.

Se a confirmação da gravação for incerta, novas alterações são bloqueadas. **Reabrir trabalho** confere o que efetivamente ficou no banco; o candidato permanece retido em memória enquanto essa sessão vive. Não há retorno silencioso para um salvo mais antigo se o registro de trabalho estiver corrompido.

## Limites

Formato auxiliar P2-C: `user_version=3`, snapshot `INDE-OBJ01-COMPOSITE-P2C-1`; os schemas de projetos reais permanecem intactos. Somente operações de reordenação integral suportadas pelo motor proprietário podem alterar a cópia textual de trabalho. A origem congelada permanece byte a byte igual.

Histórico em memória limitado a 32 entradas e orçamento de 32 MiB de metadados; não é recuperado após reiniciar. Recursos removidos continuam retidos e conferidos, sem coleta automática. Sem edição livre dos parágrafos, linhas/colunas adicionais, fórmulas, mesclagens, GTK, migração de obras ou arquivo `.inde` único. A recusa de publicação é uma fronteira deste piloto; não implementa integração com o publicador principal. Veja o [relatório P2-C](../../docs/validacao/VALIDACAO-OBJ-01-P2C.md).
