#pragma once
#include "inde/project/writing.hpp"
namespace inde::project {
// Value equality independent of repository/UI ordering of authorial layers.
[[nodiscard]] bool same_document_state(const Document &a, const Document &b,
                                      bool timestamps = true);
}
