#pragma once
#include "inde/project/writing.hpp"
namespace inde::application {
class TextSemanticService {
public:
  static project::Document bind_date(const project::Document &,
                                     const std::string &annotation,
                                     const std::string &reference,
                                     std::string role, std::string notes = {});
  static project::Document unbind_date(const project::Document &,
                                       const std::string &annotation,
                                       const std::string &binding);
  static project::Document create_document_perspective(
      const project::Document &,
      const std::map<std::string, project::SemanticFieldValue> &);
  static project::Document
  bind_fictional_time(const project::Document &, const std::string &annotation,
                      const std::string &axis, const std::string &point,
                      std::optional<std::string> end = {},
                      std::string notes = {});
  static project::Document unbind_fictional_time(const project::Document &,
                                                 const std::string &annotation,
                                                 const std::string &binding);
  struct PerspectiveContext {
    std::optional<std::string> document_declaration;
    std::vector<std::string> interval_declarations;
    bool ambiguous{}, incomplete{};
    std::optional<std::string> effective_declaration;
  };
  // Derived read model only. Overlapping local declarations stay ambiguous;
  // nothing is copied into the body or automatically assigned to an entity.
  static PerspectiveContext perspective_at(const project::Document &,
                                           std::size_t position);
  // Draft operations, committed by the editor as one undoable command.
  static project::Document
  create(const project::Document &, const project::AnnotationTypeDefinition &,
         std::size_t start, std::size_t end,
         const std::map<std::string, project::SemanticFieldValue> &);
  static project::Document
  update(const project::Document &, const std::string &id,
         const std::map<std::string, project::SemanticFieldValue> &,
         const std::vector<project::AnnotationTypeDefinition> &);
  static project::Document archive(const project::Document &,
                                   const std::string &id, bool archived);
  static project::Document remove(const project::Document &,
                                  const std::string &id);
  static project::Document reanchor(const project::Document &,
                                    const std::string &id, std::size_t start,
                                    std::size_t end);
  static project::Document relate(const project::Document &,
                                  const std::string &source,
                                  const std::string &target,
                                  const std::string &type);
  static project::Document unrelate(const project::Document &,
                                    const std::string &relation);
};
} // namespace inde::application
