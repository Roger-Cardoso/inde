#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// INDE-OBJ-01 v0.2: isolated, in-memory composition graph.
// Not a replacement for project::Document, project::Composition or schema 37.
namespace inde::objects {

// TextBlock captures a source block revision (possibly multiline). TextReference
// is navigation-only; neither kind implies paragraph identities or publication.
enum class ObjectKind { Paragraph, Image, Table, Equation, Container, TextBlock, TextReference };
enum class LayoutKind { Stack, Row, Grid, Overlay, Freeform };

struct Resource {
  std::string id;
  std::string sha256;    // Claimed lowercase hex digest, not verified without bytes.
  std::string media_type;
  std::uint64_t byte_size{};
  bool operator==(const Resource &) const = default;
};

struct Object {
  std::string id;
  ObjectKind kind{ObjectKind::Paragraph};
  std::optional<std::string> resource_id; // Logical reference, never a file path.
  std::optional<std::string> layout_id;    // Required only for containers.
  bool operator==(const Object &) const = default;
};

// Relative to the containing layout's coordinate system. Negative positions
// are allowed for off-canvas freeform work; dimensions must be positive/finite.
struct Frame {
  double x{}, y{}, width{1.0}, height{1.0};
  bool operator==(const Frame &) const = default;
};

struct Occurrence {
  std::string id;
  std::string object_id;
  std::optional<Frame> frame;
  bool operator==(const Occurrence &) const = default;
};

struct Layout {
  std::string id;
  LayoutKind kind{LayoutKind::Stack};
  std::size_t columns{};             // Positive only for Grid.
  std::vector<std::string> children; // Ordered occurrence IDs = reading order.
  // Spatial layouts only: bottom to top. Empty uses children as legacy default.
  // Commands freeze that default before changing reading order.
  std::vector<std::string> paint_order{};
  bool operator==(const Layout &) const = default;
};

struct Composition {
  std::string id;
  std::string root_layout_id;
  std::vector<Object> objects;          // Reusable logical objects.
  std::vector<Occurrence> occurrences; // Exactly one placement per occurrence.
  std::vector<Layout> layouts;          // Single rooted layout tree.
  std::vector<Resource> resources;      // Metadata only: no binary data.
  bool operator==(const Composition &) const = default;
};

// Stable machine-readable codes; no throwing for malformed input.
struct Diagnostic {
  std::string code;
  std::string subject_id;
  bool operator==(const Diagnostic &) const = default;
};

// Preflight caps apply before allocating graph indexes. Strings include references.
struct ValidationLimits {
  std::size_t entities{100000};
  std::size_t references{400000};
  std::size_t string_bytes{32 * 1024 * 1024};
};
// Read-only validation. Diagnostics are deterministic and do not mutate data.
[[nodiscard]] std::vector<Diagnostic> validate(const Composition &composition,
                                               ValidationLimits limits = {});

} // namespace inde::objects
