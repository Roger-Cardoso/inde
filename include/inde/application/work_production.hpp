#pragma once
#include "inde/project/catalog.hpp"
#include "inde/project/structural_node.hpp"
#include "inde/project/writing.hpp"
#include <filesystem>
#include <map>
#include <stop_token>
namespace inde::application {
enum class ProductionProfile { Manuscript, Reading };
struct ProductionOptions {
  ProductionProfile profile{ProductionProfile::Manuscript};
  std::string author;
  bool docx{true}, epub{true}, pdf{true};
};
struct ProductionChoice {
  bool heading{true};
  std::vector<std::string> documents;
};
using ProductionChoices = std::map<std::string, ProductionChoice>;
struct ProductionUnit {
  project::StructuralNode node;
  std::size_t depth{};
  bool heading{};
  std::vector<project::Document> documents;
};
// A derived snapshot, not a composition, saved manuscript binding or a backup.
struct WorkProduction {
  std::string project_id, structure_id, prepared_at;
  project::Work work;
  ProductionOptions options;
  std::vector<ProductionUnit> units;
  std::vector<std::string> omissions, limitations;
  std::string preview;
};
std::vector<project::StructuralNode>
production_order(const std::vector<project::StructuralNode> &);
WorkProduction
prepare_work_production(const std::string &project_id,
                        const std::string &structure_id, const project::Work &,
                        const std::vector<project::StructuralNode> &,
                        const std::vector<project::Document> &,
                        const ProductionChoices &, const ProductionOptions &);
// Fresh external folder, exclusive files, hashes/readback; report published
// last.
void write_work_production(const WorkProduction &,
                           const std::filesystem::path &new_folder,
                           std::stop_token = {});
} // namespace inde::application
