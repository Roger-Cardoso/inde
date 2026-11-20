#include "object_model.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inde::objects {
namespace {

bool is_sha256(const std::string &value) {
  if (value.size() != 64) return false;
  for (const char c : value)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

bool valid_frame(const Frame &frame) {
  return std::isfinite(frame.x) && std::isfinite(frame.y) &&
         std::isfinite(frame.width) && std::isfinite(frame.height) &&
         frame.width > 0 && frame.height > 0;
}

} // namespace

std::vector<Diagnostic> validate(const Composition &composition) {
  std::vector<Diagnostic> errors;
  auto issue = [&](const std::string &code, const std::string &id) {
    errors.push_back({code, id});
  };
  std::unordered_set<std::string> unique_ids;
  auto register_id = [&](const std::string &id, const std::string &category) {
    if (id.empty()) issue("missing_id", category);
    else if (!unique_ids.insert(id).second) issue("duplicate_id", id);
  };
  register_id(composition.id, "composition");

  std::unordered_map<std::string, const Object *> objects;
  std::unordered_map<std::string, const Occurrence *> occurrences;
  std::unordered_map<std::string, const Layout *> layouts;
  std::unordered_set<std::string> resource_ids;
  for (const auto &resource : composition.resources) {
    register_id(resource.id, "resource");
    resource_ids.insert(resource.id);
    if (!is_sha256(resource.sha256)) issue("invalid_resource_hash", resource.id);
    if (resource.media_type.empty()) issue("missing_media_type", resource.id);
  }
  for (const auto &object : composition.objects) {
    register_id(object.id, "object");
    objects.emplace(object.id, &object);
  }
  for (const auto &occurrence : composition.occurrences) {
    register_id(occurrence.id, "occurrence");
    occurrences.emplace(occurrence.id, &occurrence);
  }
  for (const auto &layout : composition.layouts) {
    register_id(layout.id, "layout");
    layouts.emplace(layout.id, &layout);
  }
  if (composition.root_layout_id.empty() ||
      !layouts.contains(composition.root_layout_id))
    issue("missing_root_layout", composition.root_layout_id);

  for (const auto &object : composition.objects) {
    if (object.kind == ObjectKind::Image && !object.resource_id)
      issue("missing_image_resource", object.id);
    if (object.resource_id && !resource_ids.contains(*object.resource_id))
      issue("missing_resource", object.id);
    if (object.kind == ObjectKind::Container) {
      if (!object.layout_id || !layouts.contains(*object.layout_id))
        issue("missing_container_layout", object.id);
    } else if (object.layout_id) {
      issue("unexpected_container_layout", object.id);
    }
  }

  std::unordered_map<std::string, unsigned> occurrence_uses;
  for (const auto &layout : composition.layouts) {
    if (layout.kind == LayoutKind::Grid) {
      if (layout.columns == 0) issue("missing_grid_columns", layout.id);
    } else if (layout.columns != 0) {
      issue("unexpected_grid_columns", layout.id);
    }
    for (const auto &child_id : layout.children) {
      const auto child = occurrences.find(child_id);
      if (child == occurrences.end()) {
        issue("missing_occurrence", child_id);
        continue;
      }
      const unsigned uses = ++occurrence_uses[child_id];
      if (uses > 1) issue("reused_occurrence", child_id);
      const auto &occurrence = *child->second;
      if (!objects.contains(occurrence.object_id))
        issue("missing_object", occurrence.id);
      if (occurrence.frame) {
        if (!valid_frame(*occurrence.frame)) issue("invalid_frame", occurrence.id);
        if (layout.kind != LayoutKind::Freeform &&
            layout.kind != LayoutKind::Overlay)
          issue("unexpected_frame", occurrence.id);
      } else if (layout.kind == LayoutKind::Freeform) {
        issue("missing_freeform_frame", occurrence.id);
      }
    }
  }
  for (const auto &occurrence : composition.occurrences)
    if (!occurrence_uses.contains(occurrence.id))
      issue("orphan_occurrence", occurrence.id);

  // Explicit stack avoids call-stack exhaustion in deeply nested documents.
  std::unordered_map<std::string, std::uint8_t> color;
  struct Visit { const Layout *layout; std::size_t next{}; };
  for (const auto &layout : composition.layouts) {
    if (color[layout.id] != 0) continue;
    color[layout.id] = 1;
    std::vector<Visit> stack{{&layout, 0}};
    while (!stack.empty()) {
      auto &visit = stack.back();
      if (visit.next == visit.layout->children.size()) {
        color[visit.layout->id] = 2;
        stack.pop_back();
        continue;
      }
      const auto child_id = visit.layout->children[visit.next++];
      const auto occurrence = occurrences.find(child_id);
      if (occurrence == occurrences.end()) continue;
      const auto object = objects.find(occurrence->second->object_id);
      if (object == objects.end() || object->second->kind != ObjectKind::Container ||
          !object->second->layout_id) continue;
      const auto nested = layouts.find(*object->second->layout_id);
      if (nested == layouts.end()) continue;
      auto &state = color[nested->first];
      if (state == 1) issue("layout_cycle", nested->first);
      else if (state == 0) {
        state = 1;
        stack.push_back({nested->second, 0});
      }
    }
  }
  return errors;
}

} // namespace inde::objects
