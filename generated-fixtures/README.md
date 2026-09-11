# Fixtures locais geradas

Este diretório recebe corpora grandes recriados por ferramentas do build. Os
contêineres `.inde` são deliberadamente ignorados pelo Git: a fonte versionada é
o gerador, não o banco binário de centenas de megabytes.

```sh
cmake --build build-release --target inde_colossal_fixture_generator
./build-release/inde_colossal_fixture_generator \
  'generated-fixtures/Arquipélago de Íris — Corpus Colossal Sintético.inde'
```

O destino precisa não existir. O gerador é integralmente procedural e não usa
texto, nomes, estrutura ou fatos de obras protegidas de terceiros.
