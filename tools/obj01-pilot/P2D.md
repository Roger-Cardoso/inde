# P2-D — Fórmulas no documento e nas células

Na branch `feature/inde-obj-01-foundation`:

```sh
bash scripts/demos/Abrir-demo-obj01-p2d.sh
```

Abra o endereço completo informado pelo iniciador. A demonstração é local, com dados sintéticos, núcleo C++ e SQLite auxiliar reais. Na primeira abertura padrão, copia somente o estado **salvo** de P2-C para uma pasta P2-D nova; não copia trabalho ainda não salvo e não modifica a origem. P2-A/B/C conservam pastas e iniciadores próprios.

Para começar independentemente:

```sh
bash scripts/demos/Abrir-demo-obj01-p2d.sh --workspace /tmp/minhas-formulas-obj01
```

Para copiar outra fixture P2-C salva, use `--workspace /tmp/copia-p2d --seed-p2c /caminho/da/fixture-p2c`. O destino deve ser novo. Nunca use pastas de obras: caminhos `.inde`, perfis incompatíveis e bancos sem identificação são recusados.

## Percurso curto

1. Escolha **Estrutura**. No painel **Fórmulas**, crie `Combate` no documento com a fonte abaixo. Fora da edição, aparece apenas **80**.
2. Crie `Metade` com `resultado Combate.resultado / 2`: mostra **40**. Uma referência também pode consultar `Combate.HP` ou outra variável declarada.
3. Clique numa célula da tabela para selecioná-la. Em Estrutura, escolha **Criar em → Célula selecionada** e crie `Bonus` com `resultado Metade.resultado + 5`. A célula mostra **45**. Fórmulas criadas nas células podem alimentar objetos do documento pelo mesmo nome e identidade.
4. Clique no resultado de Combate. O instrumento passa para **Texto** e abre a caixa de fonte no próprio lugar. Mude o dano para 15 e use **Aplicar fórmula** ou Ctrl+Enter. A fonte se recolhe, e os resultados passam a **70**, **35** e **40**. **Cancelar edição** ou Escape conserva a fonte anterior.
5. Desfaça a edição para retornar ao dano 10. Em Estrutura, crie `Golpe` com a segunda fonte abaixo. Criar, abrir ou mover esse objeto não altera o HP.
6. Em Texto, escolha Golpe na biblioteca e clique **Executar alterações**. O HP atual passa a **80**, Combate a **60**, Metade a **30** e Bonus a **35**. A biblioteca de Combate distingue inicial 100 de atual 80; permite restaurar o inicial. Desfazer/refazer restaura a operação inteira.
7. Encerre e reabra a mesma pasta. Fontes aplicadas, ligações, valores atuais e resultados são reconstruídos. Reabrir não executa Golpe outra vez; o histórico começa vazio. **Salvar documento** confirma o estado como salvo, sem executar comandos.

```text
# Combate
declara HP = 100
declara DanoEspada = 10
resultado HP - 2 × DanoEspada
```

```text
# Golpe
alterar Combate.HP = Combate.HP - 20
resultado Combate.HP
```

O campo Nome recebe `Combate` ou `Golpe`; os comentários acima são opcionais. A biblioteca permite editar fórmulas sem uso visível. **Usar fórmula na célula selecionada** liga a célula ao mesmo objeto já existente; editar sua fonte afeta todos os usos. **Retirar fórmula da célula selecionada** conserva a biblioteca e restaura o texto simples anterior daquela célula.

## Linguagem deste primeiro perfil

- Expressão simples, opcionalmente iniciada por `=`, ou várias linhas com `declara`, `alterar` e um `resultado`. Chaves externas e a forma `resultado final calcula ...` também são aceitas. Comentários começam com `#`.
- Números com ponto decimal e expoente; `+`, `-`, `*`/`×`, `/`/`÷`, `^`, parênteses e comparações `==`, `!=`, `<`, `>`, `<=`, `>=`.
- `SOMA`, `MEDIA`, `MIN`, `MAX`, `ABS` e `SE`. Exemplos: `SOMA(10;20;30)` e `SE(HP > 0; HP; 0)`. `SE` calcula somente o ramo escolhido. Separador de argumentos `;` ou `,`; vírgula não é separador decimal neste perfil.
- Variável local: `HP`. Variável de outro objeto: `Combate.HP`. Resultado de outro objeto: `Combate.resultado`.
- Nomes únicos de fórmulas e nomes de variáveis com até 64 caracteres ASCII, distinguindo maiúsculas/minúsculas, sem espaços: letras, números e `_`, começando por letra ou `_`. Renomear e referências por coordenadas como `A1` ainda não foram implementados.

Isto não é compatibilidade completa com Excel, XLSX ou macros. Os cálculos são numéricos; resultados de texto, datas, intervalos de células, funções extensas e regras automáticas de disparo ficam pendentes.

## Estado e proteção

`declara HP = 100` define a expressão inicial. Enquanto não há alteração explícita, ela alimenta o valor atual. Uma execução pode gravar um valor atual diferente; editar outra fórmula, recalcular, salvar ou reiniciar não restaura o inicial escondido. A restauração é uma operação explícita, reversível. Alterar/remover uma declaração pela edição da fonte pode remover sua substituição atual; desfazer restaura ambos.

Todas as atribuições de uma execução leem o mesmo estado anterior e são aplicadas juntas. Atribuir duas vezes à mesma variável é recusado. Uma execução que tornaria inválido um resultado antes válido é recusada integralmente. Erros que já existam em outros rascunhos não são promovidos a resultados válidos por essa regra. Não há execução automática em cascata de outros comandos: a cadeia automática recalcula resultados e expressões iniciais dependentes.

Uma fonte incompleta pode ser aplicada, salva e recuperada como fonte com diagnóstico; o objeto mostra o erro e pode ser corrigido. Nenhum número antigo é apresentado como resultado válido. **Fonte ainda no campo e sem Aplicar não está protegida**. Salvar e outras operações aplicam a fonte em edição antes de prosseguir; sair com rascunho pendente provoca aviso. A proteção começa depois da confirmação do núcleo.

O perfil usa formato auxiliar 4, sem migrar formatos anteriores nem o schema canônico. Até 64 fórmulas conservadas, fonte de 16 KiB por fórmula, 32 variáveis e 32 atribuições por fórmula; expressões e avaliação têm limites próprios. Números usam ponto flutuante binário, magnitude máxima `10^12`, apresentação de até 12 algarismos significativos; não há aritmética decimal exata. Histórico e limites agregados continuam os de P2-C. Veja o [contrato de Fórmula](../../docs/arquitetura/FORMULAS-OBJ-01.md) e o [relatório](../../docs/validacao/VALIDACAO-OBJ-01-P2D.md).
