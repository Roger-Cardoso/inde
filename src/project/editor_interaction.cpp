#include "inde/project/editor_interaction.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::project {
void validate(const TextEntityCreation &p, const Document &d) {
  validate(p.entity);
  validate(p.type);
  validate(d);
  if (p.entity.entity_type_id != p.type.id ||
      (p.create_type && p.type.is_builtin))
    throw std::runtime_error("Plano de criação de entidade/tipo incompatível");
  const auto has_interval = [&](const std::string &id) {
    return std::any_of(d.anchors.begin(), d.anchors.end(), [&](const auto &a) {
      return a.id == id && a.start_offset < a.end_offset;
    });
  };
  bool linked = std::any_of(d.entity_references.begin(),
                            d.entity_references.end(), [&](const auto &r) {
                              return r.entity_id == p.entity.id &&
                                     r.anchor_id && has_interval(*r.anchor_id);
                            });
  for (const auto &a : d.semantics.annotations)
    if (a.status != "archived" && a.scope == AnnotationScope::Interval &&
        has_interval(a.anchor_id) &&
        std::any_of(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                    [&](const auto &e) {
                      return e.anchor_id == a.anchor_id &&
                             e.state == AnchorResolution::Resolved;
                    }))
      for (const auto &[key, f] : a.fields) {
        (void)key;
        if (f.kind == SemanticFieldKind::Entity &&
            std::find(f.values.begin(), f.values.end(), p.entity.id) !=
                f.values.end())
          linked = true;
      }
  if (!linked)
    throw std::runtime_error(
        "A nova entidade precisa estar vinculada ao trecho");
}
} // namespace inde::project
