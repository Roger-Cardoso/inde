#pragma once
#include "inde/application/narrative_builder.hpp"
#include "inde/persistence/sqlite_database.hpp"
namespace inde::application::narrative_builder_detail {
persistence::SqliteDatabase open(const project::Project &);
NarrativeBuilderSnapshot read(persistence::SqliteDatabase &,
                              const project::Project &, const std::string &);
void check(persistence::SqliteDatabase &, const project::Project &,
           const NarrativeBuilderSnapshot &);
void unit(persistence::SqliteDatabase &, const project::Project &,
          const NarrativeBuilderSnapshot &, const std::string &);
void context(persistence::SqliteDatabase &, const project::Project &,
             const std::string &);
std::string read_targets(persistence::SqliteDatabase &, const project::Project &,
                         NarrativeBuilderSnapshot &);
void record_text_detach(persistence::SqliteDatabase &,const project::Project &,
                        const NarrativeBuilderSnapshot &,const std::string &id);
} // namespace inde::application::narrative_builder_detail
