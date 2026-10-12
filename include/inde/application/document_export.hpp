#pragma once
#include "inde/project/writing.hpp"
#include <filesystem>
namespace inde::application {
struct DocumentExport {
  std::string project_id, document_id, title, source_updated_at;
  std::string text, html;
  std::vector<std::string> limitations;
};
// A derived, immutable reading snapshot. Not a Project backup or round-trip codec.
DocumentExport prepare_document_export(const std::string &project_id,
                                       const project::Document &document);
// Creates a new exclusive folder. Never overwrites a previous export.
// report.json is published last, only after exact file readback and sync.
void write_document_export(const DocumentExport &, const std::filesystem::path &new_folder);
}
