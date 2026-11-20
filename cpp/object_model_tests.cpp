#include "object_model.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace obj = inde::objects;

namespace {

int failures{};

bool contains(const std::vector<obj::Diagnostic> &items, const std::string &code) {
  return std::any_of(items.begin(), items.end(), [&](const auto &d) {
    return d.code == code;
  });
}

void expect(const bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

obj::Composition sample() {
  obj::Composition doc;
  doc.id = "doc";
  doc.root_layout_id = "root";
  doc.resources = {{"resource", std::string(64, 'a'), "image/png", 128}};
  doc.objects = {
      {"paragraph", obj::ObjectKind::Paragraph, {}, {}},
      {"picture", obj::ObjectKind::Image, "resource", {}},
      {"group", obj::ObjectKind::Container, {}, "inner"},
  };
  doc.occurrences = {
      {"intro", "paragraph", {}},
      {"side", "group", {}},
      {"image-in-row", "picture", {}},
      {"image-reused", "picture", {}},
  };
  doc.layouts = {
      {"root", obj::LayoutKind::Stack, 0, {"intro", "side", "image-reused"}},
      {"inner", obj::LayoutKind::Row, 0, {"image-in-row"}},
  };
  return doc;
}

void test_valid_graph() {
  auto doc = sample();
  expect(obj::validate(doc).empty(), "valid nested document with reused image object");
  doc.layouts[1].kind = obj::LayoutKind::Grid;
  doc.layouts[1].columns = 2;
  expect(obj::validate(doc).empty(), "valid grid with two columns");
}

void test_missing_references() {
  auto doc = sample();
  doc.occurrences[0].object_id = "nonexistent";
  expect(contains(obj::validate(doc), "missing_object"), "missing object rejected");
  doc = sample();
  doc.objects[1].resource_id = "nonexistent";
  expect(contains(obj::validate(doc), "missing_resource"), "missing asset rejected");
  doc = sample();
  doc.objects[1].resource_id.reset();
  expect(contains(obj::validate(doc), "missing_image_resource"), "image must reference a resource");
  doc = sample();
  doc.root_layout_id = "nonexistent";
  expect(contains(obj::validate(doc), "missing_root_layout"), "missing root rejected");
}

void test_identity_and_occurrences() {
  auto doc = sample();
  doc.objects[0].id = "picture";
  expect(contains(obj::validate(doc), "duplicate_id"), "duplicate object ID rejected");
  doc = sample();
  doc.layouts[0].children.push_back("intro");
  expect(contains(obj::validate(doc), "reused_occurrence"), "occurrence cannot have two placements");
  doc = sample();
  doc.layouts[0].children.erase(doc.layouts[0].children.begin());
  expect(contains(obj::validate(doc), "orphan_occurrence"), "orphan occurrence reported");
}

void test_layout_cycles() {
  auto doc = sample();
  doc.objects.push_back({"recursive", obj::ObjectKind::Container, {}, "root"});
  doc.occurrences.push_back({"loop", "recursive", {}});
  doc.layouts[1].children.push_back("loop");
  expect(contains(obj::validate(doc), "layout_cycle"), "recursive layout rejected");
}

void test_geometry_and_columns() {
  auto doc = sample();
  doc.layouts[1].kind = obj::LayoutKind::Freeform;
  expect(contains(obj::validate(doc), "missing_freeform_frame"), "freeform needs coordinates");
  doc.occurrences[2].frame = obj::Frame{0.2, 0.1, 0.5, 0.4};
  expect(obj::validate(doc).empty(), "valid freeform rectangle");
  doc.occurrences[2].frame->width = std::nan("");
  expect(contains(obj::validate(doc), "invalid_frame"), "NaN rectangle rejected");
  doc = sample();
  doc.layouts[1].kind = obj::LayoutKind::Grid;
  expect(contains(obj::validate(doc), "missing_grid_columns"), "grid needs column count");
}

void test_asset_metadata() {
  auto doc = sample();
  doc.resources[0].sha256 = "SHA256";
  expect(contains(obj::validate(doc), "invalid_resource_hash"), "invalid hash rejected");
  doc = sample();
  doc.resources[0].media_type.clear();
  expect(contains(obj::validate(doc), "invalid_media_type"), "missing media type rejected");
}

void test_unexpected_fields() {
  auto doc = sample();
  doc.objects[0].layout_id = "inner";
  expect(contains(obj::validate(doc), "unexpected_container_layout"),
         "non-container cannot own nested layout");
  doc = sample();
  doc.occurrences[0].frame = obj::Frame{};
  expect(contains(obj::validate(doc), "unexpected_frame"),
         "flow layout cannot use absolute frames");
}

void test_layout_parentage() {
  auto doc = sample();
  doc.occurrences.push_back({"second-group", "group", {}});
  doc.layouts[0].children.push_back("second-group");
  expect(contains(obj::validate(doc), "layout_multiple_parents"),
         "reused container must not reuse same nested layout");
  doc = sample();
  doc.objects.push_back({"other-group", obj::ObjectKind::Container, {}, "inner"});
  doc.occurrences.push_back({"second-group", "other-group", {}});
  doc.layouts[0].children.push_back("second-group");
  expect(contains(obj::validate(doc), "layout_multiple_parents"),
         "different container objects must not share nested layout");
  doc = sample();
  doc.objects.push_back({"recursive", obj::ObjectKind::Container, {}, "root"});
  doc.occurrences.push_back({"loop", "recursive", {}});
  doc.layouts[1].children.push_back("loop");
  expect(contains(obj::validate(doc), "root_layout_nested"),
         "root layout cannot be nested");
}

void test_layout_reachability() {
  auto doc = sample();
  doc.layouts.push_back({"detached", obj::LayoutKind::Stack, 0, {}});
  expect(contains(obj::validate(doc), "unreachable_layout"),
         "detached layout rejected");
  doc = sample();
  doc.layouts[0].children.erase(doc.layouts[0].children.begin() + 1);
  expect(contains(obj::validate(doc), "unreachable_layout"),
         "unreachable child layout rejected");
  doc = sample();
  doc.layouts = {{"root", obj::LayoutKind::Stack, 0, {}}};
  doc.occurrences.clear();
  doc.objects.clear();
  doc.resources.clear();
  expect(obj::validate(doc).empty(), "empty rooted composition valid");
}

void test_unknown_kinds_and_identities() {
  auto doc = sample();
  doc.objects[0].kind = static_cast<obj::ObjectKind>(500);
  expect(contains(obj::validate(doc), "unknown_object_kind"), "unknown object kind rejected");
  doc = sample();
  doc.layouts[1].kind = static_cast<obj::LayoutKind>(500);
  expect(contains(obj::validate(doc), "unknown_layout_kind"), "unknown layout kind rejected");
  doc = sample();
  doc.resources[0].id = "root";
  expect(contains(obj::validate(doc), "duplicate_id"), "cross-category ID collision rejected");
  doc = sample();
  doc.occurrences[0].id.clear();
  expect(contains(obj::validate(doc), "missing_id"), "empty occurrence ID rejected");
}

void test_strict_resource_and_geometry() {
  auto doc = sample();
  doc.resources[0].byte_size = 0;
  expect(contains(obj::validate(doc), "empty_resource"), "zero-sized resource rejected");
  doc = sample();
  doc.resources[0].media_type = "image/png; bad";
  expect(contains(obj::validate(doc), "invalid_media_type"), "malformed MIME rejected");
  doc = sample();
  doc.resources.push_back({"resource-copy", doc.resources[0].sha256, "image/png", 128});
  expect(obj::validate(doc).empty(), "distinct resource IDs may share content hash");
  doc = sample();
  doc.layouts[1].kind = obj::LayoutKind::Freeform;
  doc.occurrences[2].frame = obj::Frame{-5, 0, 1, 1};
  expect(obj::validate(doc).empty(), "finite negative canvas coordinate allowed");
  for (const double invalid : {0.0, -1.0, std::nan(""),
                               std::numeric_limits<double>::infinity()}) {
    doc.occurrences[2].frame->width = invalid;
    expect(contains(obj::validate(doc), "invalid_frame"), "invalid frame width rejected");
  }
  doc.occurrences[2].frame = obj::Frame{std::nan(""), 0, 1, 1};
  expect(contains(obj::validate(doc), "invalid_frame"), "NaN x coordinate rejected");
}

void test_deterministic_diagnostics() {
  auto doc = sample();
  doc.objects[1].resource_id = "missing";
  doc.layouts[0].children.push_back("intro");
  const auto first = obj::validate(doc), second = obj::validate(doc);
  expect(first.size() == second.size(), "stable number of diagnostics");
  if (first.size() == second.size())
    for (std::size_t i = 0; i < first.size(); ++i)
      expect(first[i].code == second[i].code &&
             first[i].subject_id == second[i].subject_id, "stable diagnostic order");
  expect(doc.objects[1].resource_id == "missing", "validation must not mutate object");
  expect(doc.layouts[0].children.back() == "intro", "validation must not mutate layout");
}

void test_deep_composition() {
  constexpr int depth = 7000;
  obj::Composition doc;
  doc.id = "deep";
  doc.root_layout_id = "layout-0";
  doc.layouts.reserve(depth + 1);
  doc.objects.reserve(depth);
  doc.occurrences.reserve(depth);
  for (int i = 0; i <= depth; ++i) {
    doc.layouts.push_back({"layout-" + std::to_string(i), obj::LayoutKind::Stack, 0, {}});
    if (i == depth) continue;
    const auto occurrence_id = "occ-" + std::to_string(i);
    doc.layouts.back().children.push_back(occurrence_id);
    doc.objects.push_back({"obj-" + std::to_string(i), obj::ObjectKind::Container,
                           {}, "layout-" + std::to_string(i + 1)});
    doc.occurrences.push_back({occurrence_id, doc.objects.back().id, {}});
  }
  expect(obj::validate(doc).empty(),
         "7000 nested containers validated without recursive call stack");
}

} // namespace

int main() {
  test_valid_graph();
  test_missing_references();
  test_identity_and_occurrences();
  test_layout_cycles();
  test_geometry_and_columns();
  test_asset_metadata();
  test_unexpected_fields();
  test_layout_parentage();
  test_layout_reachability();
  test_unknown_kinds_and_identities();
  test_strict_resource_and_geometry();
  test_deterministic_diagnostics();
  test_deep_composition();
  if (failures != 0) {
    std::cerr << failures << " object-model tests failed\n";
    return 1;
  }
  std::cout << "INDE-OBJ-01: 13 test groups passed\n";
}
