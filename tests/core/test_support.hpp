#pragma once

#include "inde/project/manifest.hpp"
#include "inde/project/narrative.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace inde::tests {

struct TemporaryDirectory {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
                               ("inde-tests-" + inde::project::new_uuid());
  TemporaryDirectory() { std::filesystem::create_directories(path); }
  ~TemporaryDirectory() { std::filesystem::remove_all(path); }
};

inline const inde::project::EntityType &
find_entity_type(const std::vector<inde::project::EntityType> &types,
                 std::string_view key) {
  const auto found =
      std::find_if(types.begin(), types.end(),
                   [&](const auto &value) { return value.key == key; });
  assert(found != types.end());
  return *found;
}

inline std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

} // namespace inde::tests
