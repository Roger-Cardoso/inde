#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// INDE-OBJ-01 v0: an isolated, in-memory composition graph.
// This is not a replacement for project::Document or any persisted schema.
namespace inde::objects {

enum class ObjectKind { Paragraph, Image, Table, Equation, Container };
enum class LayoutKind { Stack, Row, Grid, Overlay, Freeform };

struct Resource {
  std::string id;
  std::string sha256;    // Lowercase hex digest of original bytes.
  std::string media_type;
  std::uint64_t byte_size{};
};

struct Object {
  std::string id;
  ObjectKind kind{ObjectKind::Paragraph};
  std::optional<std::string> resource_id;  // Logical reference, never a file path.
  std::optional<std::string> layout_id;    // Required only for containers.
};

// Positions are relative to the layout's coordinate system; the renderer
// determines pixels, page size and responsive breakpoints.
struct Frame {
  double x{}, y{}, width{1.0}, height{1.0};
};

struct Occurrence {
  std::string id;
  std::string object_id;
  std::optional<Frame> frame;
};

struct Layout {
  std::string id;
  LayoutKind kind{LayoutKind::Stack};
  std::size_t columns{};             // Positive only for Grid.
  std::vector<std::string> children; // Ordered occurrence IDs = logical reading order.
};

struct Composition {
  std::string id;
  std::string root_layout_id;
  std::vector<Object> objects;
  std::vector<Occurrence> occurrences;
  std::vector<Layout> layouts;
  std::vector<Resource> resources;
};

// Stable machine-readable codes; no throwing for malformed input.
struct Diagnostic {
  std::string code;
  std::string subject_id;
};

[[nodiscard]] std::vector<Diagnostic> validate(const Composition &composition);

} // namespace inde::objects
