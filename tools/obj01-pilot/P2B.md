# P2-B — Tabela editável e histórico da sessão

Na branch `feature/inde-obj-01-foundation`, execute:

```sh
bash scripts/demos/Abrir-demo-obj01-p2b.sh
```

Abra o endereço completo informado por `Demo P2B:`. O servidor é local, acessível só por `127.0.0.1`. A demo P2-A continua disponível por seu iniciador anterior.

Na primeira abertura padrão, se existir `dados/obj01-piloto-p2a/`, o iniciador copia **somente seu estado salvo**, por uma leitura SQLite consistente, para `dados/obj01-piloto-p2b/`. Origem, parágrafos, figuras, bytes e identidades são conservados. P2-A não é convertido ou regravado. A partir daí, P2-B reabre sua própria pasta. Alterações ainda em memória de P2-A não são copiadas.

Para uma fixture nova, independente:

```sh
bash scripts/demos/Abrir-demo-obj01-p2b.sh --workspace /tmp/meu-piloto-p2b
```

Para copiar explicitamente uma fixture P2-A para outra pasta nova:

```sh
bash scripts/demos/Abrir-demo-obj01-p2b.sh --workspace /tmp/copia-p2b --seed-p2a /caminho/da/fixture-p2a
```

Não use pastas de obras. O núcleo recusa caminhos `.inde`, bancos sem identificação ou com versão/perfil incompatível e cópia sobre uma pasta experimental já existente.

## Percurso curto

1. Em **Tabela de notas**, confira título, cabeçalho e ponto de inserção; clique **Inserir tabela 2 × 3**.
2. Clique numa célula e digite. **Tab/Shift+Tab** aplicam o texto e mudam de célula. **Ctrl+Enter** aplica sem sair. Sair do campo também aplica o texto; **Salvar** confere e aplica os campos ainda em edição antes de gravar.
3. Use **Selecionar a tabela inteira** para mover/remover o objeto. O título e as seis células continuam vinculados às mesmas identidades. A tabela fica na coluna principal; não entra no Row de figura/parágrafo.
4. Experimente **Desfazer** e **Refazer**, inclusive depois de remover a tabela ou um uso de figura. Remover um uso conserva conteúdo e recursos na biblioteca experimental.
5. Salve, encerre o servidor e reabra a mesma pasta. Células, figuras, ordem e seleção são restauradas. O histórico de desfazer começa vazio no processo novo.

A edição é texto simples UTF-8, com até 4.096 escalares Unicode por célula e orçamento agregado de strings de 8 MiB. Fórmulas, mesclagens, edição de linhas/colunas e compartilhamento de uma mesma tabela entre usos ficam pendentes. O texto dos parágrafos de origem continua somente leitura.

## Gravação, conservação e limites

P2-B tem `user_version=2` e snapshot `INDE-OBJ01-COMPOSITE-P2B-1`; o identificador SQLite e as tabelas físicas do laboratório continuam próprios. P2-A conserva seu formato 1, sem migração automática. O mesmo executável escolhe o perfil explicitamente e recusa abrir um no modo do outro.

Desfazer/refazer conserva o agregado: conteúdo, objetos/usos, bytes, ordens, perfis e seleção por identidade. As revisões da sessão são monotônicas; uma edição nova após desfazer invalida refazer. Selecionar outra célula não é edição de conteúdo e não apaga o refazer.

O histórico é limitado a 32 entradas e 32 MiB de metadados serializados contando o estado corrente; bytes imutáveis são compartilhados, com até 32 MiB de blobs distintos no conjunto retido. Entradas antigas podem ser retiradas para cumprir limites e permanecerem válidas sob a biblioteca conservada. Não se promete quantidade fixa de passos para documentos grandes nem um limite de RSS equivalente a esses orçamentos.

Recursos e registros removidos são retidos na biblioteca, inclusive ao desfazer uma inserção. Isso permite salvar depois de desfazer sem criar blobs órfãos ou apagar bytes necessários. Por essa razão, desfazer toda alteração visível pode ainda deixar uma mudança de biblioteca para salvar. Não há coleta automática nem exclusão definitiva de recursos.

**Save conserva o histórico da sessão viva; Reabrir salvo e reiniciar o processo abrem uma sessão sem esse histórico. Alterações não salvas não são recuperadas automaticamente.** Esse trabalho permanece para P2-C. A recuperação textual antiga não é apresentada como cobertura de tabela ou mídia.

A validação técnica, a demonstração e o aceite humano são registros distintos. Veja o [relatório P2-B](../../docs/validacao/VALIDACAO-OBJ-01-P2B.md).
