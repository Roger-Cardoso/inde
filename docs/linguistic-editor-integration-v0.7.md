# Integração do Sistema Linguístico com o Editor — v0.7

## Decisão humana

Em 22 de setembro de 2026, o usuário autorizou a integração com o Editor/UI e,
em seguida, autorizou a primeira fatia vertical proposta para o checkout real
do INDE. Esta decisão remove somente a exclusão de integração com a interface.
L-07, L-09, fontes adicionais, ampliação de lote e binding automático não são
autorizados por ela.

## Fatia implementada

O workspace de Escrita oferece duas operações deliberadamente explícitas:

1. consulta exata de uma palavra/expressão selecionada ou digitada no catálogo
   L-06;
2. exploração nominal de Construction, Frame, SyntacticFrame e MWE no catálogo
   promovido L-08.

O fluxo é `UI -> LinguisticService -> SqliteLinguisticCatalog`. A UI não recebe
SQL, não abre o banco diretamente e não contém regras de identidade lexical.
Os catálogos são globais e externos ao projeto `.inde`; nenhum schema do
projeto foi alterado.

## Invariantes

- abertura SQLite com `SQLITE_OPEN_READONLY` e `PRAGMA query_only=ON`;
- statements fixos e parametrizados; limite entre 1 e 50;
- validação dos metadados dos contratos `l06-query-1` e `l08-promotion-1`;
- L-06 precisa declarar runtime somente leitura e rede ausente;
- L-08 precisa declarar revisão aceita, origem `synthetic_fixture`, estado
  `derived`, escopo `internal_productive` e zero bindings automáticos;
- nenhuma consulta contém vencedor, seleção ou botão de aplicar acepção;
- navegação L-08 não executa match no Documento;
- o conteúdo e a seleção do Documento não são persistidos no catálogo;
- ausência ou incompatibilidade do catálogo desabilita o resultado sem criar
  banco vazio, buscar na rede ou degradar silenciosamente o contrato.

## Configuração local

O aplicativo lê:

- `INDE_L06_CATALOG` para `dictionary-l06-query.sqlite`;
- `INDE_L08_CATALOG` para `dictionary-l08-promoted.sqlite`.

Sem essas variáveis, procura ambos em
`$XDG_DATA_HOME/inde/linguistic/` ou
`$HOME/.local/share/inde/linguistic/`. Os bancos não são distribuídos
automaticamente pelo repositório.

## Fora do escopo

- tokenização ou persistência de ocorrências do texto real;
- análise incremental contínua;
- destaque automático de Construction/MWE;
- `SemanticCandidateSet` persistido no projeto;
- `OPERATION_RESOLVED` ou `USER_BOUND`;
- alteração do texto, substituição lexical ou refatoração semântica;
- telemetria, rede ou serviço remoto;
- L-07 e L-09.

Esses limites impedem que a autorização de UI seja confundida com autorização
para construir o corpus local da Obra ou tomar decisões semânticas pelo autor.

## Verificação

`inde_linguistic_integration_tests` cria catálogos efêmeros, mede os bytes
antes/depois e cobre: consulta com múltiplas acepções, ausência de seleção e
binding, statements parametrizados, limites, catálogo ausente, contrato
incompatível e rejeição de build L-08 que declare binding automático. Quando
as variáveis de ambiente estão definidas, o mesmo executável faz smoke dos
artefatos v0.6 autorizados.
