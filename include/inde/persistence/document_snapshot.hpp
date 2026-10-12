#pragma once
#include "inde/project/writing.hpp"
namespace inde::persistence {
// Complete Document snapshot, not an export or a canonical database binding.
std::string encode_document_snapshot(const project::Document &);
project::Document decode_document_snapshot(const std::string &);
} // namespace inde::persistence
