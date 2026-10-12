#pragma once
#include <stdexcept>
namespace inde::application {
// A post-commit refresh failure must never be reported as a rolled-back action.
class ProjectOperationCommittedError final : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
} // namespace inde::application
