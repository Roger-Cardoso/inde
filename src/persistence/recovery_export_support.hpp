#pragma once
#include "inde/project/document_recovery.hpp"
namespace inde::persistence::recovery {
std::filesystem::path export_path(const project::Project &, const project::RecoveryRecord &);
std::filesystem::path publish_export(const project::Project &, const project::RecoveryRecord &,
                                    const std::string &payload);
project::RecoveryRecord read_export(const std::filesystem::path &);
}
