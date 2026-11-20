# INDE-OBJ-01 — Inventário inicial das demonstrações e da base

Data: 2026-11-20. Referência `main`: `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`.

**Alcance:** auditoria **estática** dos arquivos textuais acessíveis e do manifesto da fonte integrada; **não** é inspeção completa dos bytes dentro de `fontes/inde-integrado-schema37.tar.gz`. O manifesto `metadata/INTEGRACAO-INDE.json` inventaria 758 arquivos e schema autoral 37. Testes de integração documentados anteriormente são históricos, não foram reexecutados nesta etapa.

O inventário compactado registra `include/inde/project/writing.hpp`, `document_paragraphs.hpp`, `document_alternatives.hpp`, `document_visibility.hpp`, `composition.hpp`; `include/inde/application/writing_service.hpp`, `document_edit_session.hpp`, `document_recovery_service.hpp`; `include/inde/persistence/sqlite_writing_repository.hpp`, `project_operation_store.hpp`, `project_database_repository.hpp` e os respectivos testes. É indispensável extrair, auditar e verificar esses componentes antes de integrar OBJ-01 ao núcleo.

| Área | Situação observada | Risco de integração |
| --- | --- | --- |
| Escrita | `cpp/overlay/include/inde/ui/writing_workspace.hpp`: `Gtk::TextView`, tags, âncoras, alternativas, recuperação e sessão de edição | Preservar offsets, identidades e histórico |
| Publicação | `cpp/snapshot_model.cpp`, `ponte_inde.py`, `publicacao.py` traduzem subconjunto de texto e marcações | Objetos novos devem ter tradutor ou erro explícito |
| Comic | `cpp/comic_model.hpp` tem `Panel`, `Element` e `Asset`; geometria por elemento | Não duplicar modelo de layout nem converter Comic em objeto universal |
| Recursos Comic | `cpp/comic_model.cpp` valida mídia e transporta dados em Base64 | Migração BLOB/hash com validação de bytes |
| Persistência Comic | `cpp/comic_service.cpp` usa `data/comics.sqlite3`, schema 1 | Preservar revisões, estados e recursos |
| Publicação SQLite | `cpp/publication_service.cpp` usa `data/publicacao.sqlite3`, schema 3 | Preservar identidade pública e seleção |
| Projeto | Lançadores usam diretório `Publicacao.inde/` | O novo arquivo único é mudança física futura |

## Demonstrações

| Lançador | Cobertura | Situação |
| --- | --- | --- |
| `scripts/demos/Abrir-demo-integrada.sh` | Escrita P03c, alternativas, planejamento, revisão, publicação | Amostra própria do schema 37 |
| `scripts/demos/Abrir-demo-comic.sh` | Espaço Comic nativo, quadros e recursos | Estúdio legado e catálogo misto não integralmente incluídos |
| `scripts/demos/Abrir-demo-catalogo.sh` | Catálogo com várias obras | Não comprova catálogo novel/comic misto |
| `scripts/demos/Abrir-demo-importacao.sh` | Comparação de fontes | Subconjunto com conciliação |
| `scripts/demos/Abrir-demo-obra-importada.sh` | Múltiplos capítulos e IDs públicos | Subconjunto de importação |
| `scripts/demos/Abrir-demo.sh` | Laboratório de publicação Python | Caminho legado independente |

Consolidar demos deve significar unificar **autoridades, contratos e testes**, não apenas reunir telas. Não afirmar integração de conteúdo de outros projetos/repositórios ainda não inventariados.

**Próximas verificações:** extrair/verificar a fonte pelo `python3 verificar_base.py --integrado --extrair`; conferir mecanismos reais de `project::Composition`, `Document`, Unicode, histórico, recovery e transações; testar conversões com projetos sintéticos sem modificar obras autorais; executar regressão GTK/SQLite. A base principal continua preservada.

Ver [contrato](INDE-OBJ-01.md) e [validação](../validacao/VALIDACAO-OBJ-01.md).
