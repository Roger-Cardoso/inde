#pragma once
#include "inde/project/document_recovery.hpp"
namespace inde::persistence::recovery {
void validate(const project::RecoveryRecord &);
std::string encode(const project::RecoveryRecord &);
project::RecoveryRecord decode(const std::string &payload,
                               const std::string &checksum);
std::string checksum(const std::string &);
void safe_id(const std::string &);
} // namespace inde::persistence::recovery
