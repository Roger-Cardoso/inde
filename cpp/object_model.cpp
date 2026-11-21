#include "object_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inde::objects {
namespace {

bool known_object_kind(const ObjectKind kind) {
  switch (kind) {
    case ObjectKind::Paragraph:
    case ObjectKind::Image:
    case ObjectKind::Table:
    case ObjectKind::Equation:
    case ObjectKind::Container:
    case ObjectKind::TextBlock:
    case ObjectKind::TextReference:
    case ObjectKind::Formula:
      return true;
  }
  return false;
}

bool known_layout_kind(const LayoutKind kind) {
  switch (kind) {
    case LayoutKind::Stack:
    case LayoutKind::Row:
    case LayoutKind::Grid:
    case LayoutKind::Overlay:
    case LayoutKind::Freeform:
      return true;
  }
  return false;
}

bool is_sha256(const std::string &value) {
  if (value.size() != 64) return false;
  for (const char c : value)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

bool valid_media_type(const std::string &value) {
  const auto slash = value.find('/');
  if (slash == std::string::npos || slash == 0 || slash + 1 >= value.size() ||
      value.find('/', slash + 1) != std::string::npos) return false;
  for (const char c : value) {
    if (c == '/') continue;
    const bool alnum = (c >= 'a' && c <= 'z') ||
                       (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9');
    if (!alnum && c != '.' && c != '+' && c != '-' && c != '_') return false;
  }
  return true;
}

bool valid_frame(const Frame &frame) {
  return std::isfinite(frame.x) && std::isfinite(frame.y) &&
         std::isfinite(frame.width) && std::isfinite(frame.height) &&
         frame.width > 0 && frame.height > 0 &&
         std::isfinite(frame.x + frame.width) && std::isfinite(frame.y + frame.height);
}

} // namespace

std::vector<Diagnostic> validate(const Composition &composition, ValidationLimits limits) {
  // Overflow-safe accounting of all input vectors/strings before graph work.
  std::size_t entities{}, references{}, bytes{};
  auto add = [](std::size_t &used, std::size_t amount, std::size_t cap) {
    if (used > cap || amount > cap - used) return false;
    used += amount;
    return true;
  };
  bool within = true;
  for (const auto n : {composition.objects.size(), composition.occurrences.size(),
                       composition.layouts.size(), composition.resources.size()})
    within = add(entities, n, limits.entities) && within;
  if (!within) return {{"input_limit", composition.id}};
  auto string = [&](const std::string &s) {
    within = add(bytes, s.size(), limits.string_bytes) && within;
  };
  auto reference = [&](const std::string &s) {
    within = add(references, 1, limits.references) && within;
    string(s);
  };
  string(composition.id); reference(composition.root_layout_id);
  for (const auto &r : composition.resources) {
    string(r.id); string(r.sha256); string(r.media_type);
  }
  for (const auto &o : composition.objects) {
    string(o.id);
    if (o.resource_id) reference(*o.resource_id);
    if (o.layout_id) reference(*o.layout_id);
  }
  for (const auto &o : composition.occurrences) {
    string(o.id); reference(o.object_id);
  }
  // Refuse oversized adjacency lists before iterating over their strings.
  for (const auto &l : composition.layouts) {
    string(l.id);
    within = add(references, l.children.size(), limits.references) && within;
    within = add(references, l.paint_order.size(), limits.references) && within;
    if (!within) return {{"input_limit", composition.id}};
    for (const auto &id : l.children) string(id);
    for (const auto &id : l.paint_order) string(id);
  }
  if (!within) return {{"input_limit", composition.id}};
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
  objects.reserve(composition.objects.size());
  occurrences.reserve(composition.occurrences.size());
  layouts.reserve(composition.layouts.size());
  resource_ids.reserve(composition.resources.size());
  for (const auto &resource : composition.resources) {
    register_id(resource.id, "resource");
    resource_ids.insert(resource.id);
    if (!is_sha256(resource.sha256)) issue("invalid_resource_hash", resource.id);
    if (!valid_media_type(resource.media_type)) issue("invalid_media_type", resource.id);
    if (resource.byte_size == 0) issue("empty_resource", resource.id);
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
    if (!known_object_kind(object.kind)) issue("unknown_object_kind", object.id);
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

  std::unordered_set<std::string> placed_occurrences;
  std::unordered_map<std::string, std::size_t> layout_parents;
  std::unordered_map<std::string, std::vector<std::string>> edges;
  placed_occurrences.reserve(composition.occurrences.size());
  layout_parents.reserve(composition.layouts.size());
  edges.reserve(composition.layouts.size());
  for (const auto &layout : composition.layouts) {
    if (!known_layout_kind(layout.kind)) issue("unknown_layout_kind", layout.id);
    if (layout.kind == LayoutKind::Grid) {
      if (layout.columns == 0) issue("missing_grid_columns", layout.id);
    } else if (layout.columns != 0) {
      issue("unexpected_grid_columns", layout.id);
    }
    if (!layout.paint_order.empty()) {
      if (layout.kind != LayoutKind::Overlay && layout.kind != LayoutKind::Freeform)
        issue("unexpected_paint_order", layout.id);
      std::unordered_set<std::string> painting(layout.paint_order.begin(), layout.paint_order.end());
      if (layout.paint_order.size() != layout.children.size() ||
          painting.size() != layout.paint_order.size() ||
          std::any_of(layout.children.begin(), layout.children.end(),
                      [&](const auto &id) { return !painting.contains(id); }))
        issue("invalid_paint_order", layout.id);
    }
    for (const auto &child_id : layout.children) {
      const auto child = occurrences.find(child_id);
      if (child == occurrences.end()) {
        issue("missing_occurrence", child_id);
        continue;
      }
      if (!placed_occurrences.insert(child_id).second)
        issue("reused_occurrence", child_id);
      const auto &occurrence = *child->second;
      const auto object = objects.find(occurrence.object_id);
      if (object == objects.end()) {
        issue("missing_object", occurrence.id);
      } else if (object->second->kind == ObjectKind::Container &&
                 object->second->layout_id &&
                 layouts.contains(*object->second->layout_id)) {
        const auto &nested = *object->second->layout_id;
        edges[layout.id].push_back(nested);
        const auto parents = ++layout_parents[nested];
        if (parents == 2) issue("layout_multiple_parents", nested);
      }
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
    if (!placed_occurrences.contains(occurrence.id)) {
      issue("orphan_occurrence", occurrence.id);
      if (!objects.contains(occurrence.object_id)) issue("missing_object", occurrence.id);
    }
  if (layout_parents.contains(composition.root_layout_id))
    issue("root_layout_nested", composition.root_layout_id);

  // Check every component, including disconnected components. Iterative DFS
  // avoids exhausting the C++ call stack for deeply nested content.
  std::unordered_map<std::string, std::uint8_t> color;
  color.reserve(composition.layouts.size());
  struct Visit { std::string id; std::size_t next{}; };
  for (const auto &layout : composition.layouts) {
    if (color[layout.id] != 0) continue;
    color[layout.id] = 1;
    std::vector<Visit> stack{{layout.id, 0}};
    while (!stack.empty()) {
      auto &visit = stack.back();
      const auto adjacent = edges.find(visit.id);
      if (adjacent == edges.end() || visit.next == adjacent->second.size()) {
        color[visit.id] = 2;
        stack.pop_back();
        continue;
      }
      const auto nested = adjacent->second[visit.next++];
      auto &state = color[nested];
      if (state == 1) issue("layout_cycle", nested);
      else if (state == 0) {
        state = 1;
        stack.push_back({nested, 0});
      }
    }
  }

  // An active composition has a single rooted layout tree. Unattached layouts
  // belong in a separate draft/library, not in the current composition.
  std::unordered_set<std::string> reached;
  if (layouts.contains(composition.root_layout_id)) {
    std::vector<std::string> pending{composition.root_layout_id};
    while (!pending.empty()) {
      auto current = std::move(pending.back());
      pending.pop_back();
      if (!reached.insert(current).second) continue;
      if (const auto adjacent = edges.find(current); adjacent != edges.end())
        for (const auto &nested : adjacent->second) pending.push_back(nested);
    }
  }
  for (const auto &layout : composition.layouts)
    if (!reached.contains(layout.id))
      issue("unreachable_layout", layout.id);
  return errors;
}

} // namespace inde::objects
