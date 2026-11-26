#pragma once
#include "project_file.hpp"
namespace inde::project_file {
// Compatibility bridge: exact directory components, not a semantic migration of
// schema 37. Only a closed rollback-journal project is accepted in this cut.
State pack_directory(const std::filesystem::path &source,
                     const std::filesystem::path &new_file);
State update_directory(Store &, const State &expected,
                       const std::filesystem::path &source,
                       const std::filesystem::path &store_path,
                       const std::string &label);
void unpack_directory(Store &, std::int64_t sequence,
                      const std::filesystem::path &new_directory);
} // namespace inde::project_file
