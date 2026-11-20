#include "object_model.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
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
  expect(contains(obj::validate(doc), "missing_media_type"), "missing media type rejected");
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

} // namespace

int main() {
  test_valid_graph();
  test_missing_references();
  test_identity_and_occurrences();
  test_layout_cycles();
  test_geometry_and_columns();
  test_asset_metadata();
  test_unexpected_fields();
  if (failures != 0) {
    std::cerr << failures << " object-model tests failed\n";
    return 1;
  }
  std::cout << "INDE-OBJ-01: 7 test groups passed\n";
}
