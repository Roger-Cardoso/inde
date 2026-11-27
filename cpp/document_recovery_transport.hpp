#pragma once
#include "inde/project/project.hpp"
namespace inde::persistence {
// Native private journal preparation. Never resolves, exports or removes a
// record. Auxiliary files are handled only by the owning SQLite transaction.
void prepare_document_recovery_transport(const project::Project &);
} // namespace inde::persistence
