# Evidências — reordenação e arrasto lateral

Código final: `49b9313a4a81a8a5147b954f11683bf3cee4e105`. Branch `feature/inde-obj-01-foundation`. [Relatório de comportamento, velocidade e limites](../VALIDACAO-OBJ-01-REORDENACAO-LATERAL.md).

## Conferir

Na raiz do checkout correspondente:

```sh
python3 docs/validacao/evidencias-obj01-reordenacao-lateral/verificar.py
```

O verificador somente lê. Confere evidências, 51 arquivos de implementação/integração/testes/instruções, os três compactados fixados, a suíte local e a CI do mesmo código. Não abre projeto, não recompila e não grava no banco. Um commit posterior somente de evidências/documentação pode ter outro HEAD sem mudar o código conferido. Mudanças posteriores nesses 51 arquivos devem ser detectadas; para reproduzir esta prova, use a revisão correspondente.

## Registros

- `codigo-validado.json`: hashes do código e do binário Release local. `codigo-versao-anterior.json` identifica a implementação anterior. O binário anterior foi copiado antes das mudanças e seu hash coincide com o manifesto daquela entrega. Os executáveis ficam em `dados/`, fora do Git.
- `python-referencia.txt` e `referencia.json`: percurso anterior completo, com 20/200 linhas, três movimentos, Desfazer, salvamento e saída. Foi executado sem compilação concorrente, imediatamente antes da suíte final. Não cobre os novos destinos laterais.
- `python-100.txt`: 100 testes aprovados, incluindo três percursos de entrada real. `reordering.json` conserva as amostras de movimento e soltura; `fluency.json` registra a regressão de exclusão de vazios.
- `perfil-reordenacao.txt`, `perfil-fluidez.txt` e `fases.json`: instrumentação opcional sem texto autoral. As fases são aninhadas/sobrepostas e não devem ser somadas. A pausa na barreira de salvamento aparece no perfil preservado da primeira iteração; não foi excluída da tabela de velocidade.
- `drop-before.png`, `drop-after.png`, `drop-left.png`, `drop-right.png`: indicadores capturados com o botão do mouse pressionado, no aplicativo real. As imagens não são simulações de interface ou medidas de pintura.
- `dominio-asan.txt/.json` e `build-dominio-asan.txt`: prova local do domínio com ASan/UBSan e detecção de vazamentos, incluindo proporções personalizadas, revisão obsoleta, semântica, grupos indivisíveis e reabertura. Não instrumenta a memória de toda a aplicação GTK.
- `ci.json`, `ci-dominio.json` e `ci-nativo-testes.txt`: resultado remoto, prova estrutural nas quatro configurações GCC/Clang com/sem sanitizers e trecho do log dos três percursos X11. A CI compila o aplicativo real a partir do checkout do commit declarado.
- `integridade.json`: três bancos do projeto sintético final, conferidos com o projeto fechado, antes de abrir outra cópia de demonstração. Canonical e auxiliar também foram comparados no percurso de entrada real.
- `build-final.txt`, `build-motor.txt` e `build-coordenadas.txt`: compilações incrementais locais das alterações; configuração/compilação completas pertencem à CI. O teste novo de domínio tem sua própria compilação registrada.
- `auditor-fontes.json` e `base-schema37.json`: proveniência e conferência da base fixada, retomadas do pacote anterior. Os hashes dos três compactados são novamente conferidos pelo verificador; este corte não altera seus conteúdos ou schemas.
- Os arquivos `*-primeira-iteracao.*` e `ci-primeira-falha.json/.txt` preservam a versão anterior `ef5fc7e`, seu passe local e a CI que falhou em dois casos de foco. Não são os registros finais.
- `demo.json` e `demo-folha.png`: cópia sintética aberta normalmente no editor nativo, separada do projeto fechado de teste e das janelas anteriores.

## Alcance

As medidas incluem entrada X11 e observação periódica; não são latência pura de pintura. São três amostras por tamanho, não percentis ou ensaio prolongado. O ganho na mediana não elimina o pico de 2,40 s registrado. A execução final não repetiu esse pico em suas três amostras de 200 linhas, sem certificar sua eliminação. A meta de 100 ms, perfis maiores, inserção após arranjo divergente e maturidade completa permanecem abertos. Há somente duas colunas por linha no perfil atual. Aceite humano e promoção para main não fazem parte desta prova técnica.
