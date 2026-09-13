#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace inde::project::geo {

inline constexpr int chunk_side = 64;
inline constexpr int max_terrain_level = 3;
inline constexpr int generator_version = 1;

struct Coordinate {
  double longitude{}, latitude{};
  bool operator==(const Coordinate &) const = default;
};
// Bounds never wrap: a dateline-crossing viewport is split into two ranges.
struct Bounds {
  double west{-180}, south{-90}, east{180}, north{90};
};
struct Planet {
  std::string id, name;
  std::int64_t radius_m{6371000}, seed{1};
  int water_percent{65}, fragmentation{3}, height_m{8000}, depth_m{6000};
  int detail{3}, generator{generator_version};
  std::string created_at;
  int terrain_revision{};
};
struct ChunkKey {
  int level{}, x{}, y{};
  auto operator<=>(const ChunkKey &) const = default;
};
struct TerrainChunk {
  ChunkKey key;
  std::array<std::int16_t, chunk_side * chunk_side> elevation{};
  bool operator==(const TerrainChunk &) const = default;
};
struct GeneratedPlanet {
  Planet planet;
  std::vector<TerrainChunk> chunks;
};
struct Position {
  std::string planet_id, entity_id;
  Coordinate coordinate;
  bool approximate{};
  int importance{70}, min_level{};
  std::string symbol{"place"};
  bool operator==(const Position &) const = default;
};
struct PositionedLocal {
  Position position;
  std::string name;
};

enum class TerrainEditMode { Raise, Lower, Smooth };
struct TerrainBrush {
  std::string planet_id;
  Coordinate center;
  double radius_degrees{3};
  int strength_m{500};
  TerrainEditMode mode{TerrainEditMode::Raise};
};
struct TerrainChunkDelta {
  ChunkKey key;
  TerrainChunk before, after;
  bool operator==(const TerrainChunkDelta &) const = default;
};
struct TerrainPatch {
  std::string planet_id;
  Coordinate center;
  double radius_degrees{};
  std::size_t changed_samples{};
  std::vector<TerrainChunkDelta> chunks;
};
struct TerrainLock {
  std::string id, planet_id, name;
  Coordinate center;
  double radius_degrees{};
  std::string created_at;
  bool operator==(const TerrainLock &) const = default;
};

void validate(Coordinate value);
void validate(Bounds value);
void validate(const Planet &value);
void validate(const Position &value);
void validate(ChunkKey value);
void validate(const TerrainBrush &value);
void validate(const TerrainLock &value);
double normalize_longitude(double value);
double distance_m(Coordinate a, Coordinate b, std::int64_t radius_m);
Bounds chunk_bounds(ChunkKey key);
std::vector<ChunkKey> visible_chunks(Bounds bounds, int level);
// Bounded, cancellable worker operation. No files, GTK, database or network.
GeneratedPlanet generate(Planet planet, std::stop_token stop = {});
// Pure bounded preview. It never writes files and recomputes every LOD from
// the finest stored grid so the pyramid remains exact.
TerrainPatch preview_terrain_edit(const Planet &planet,
                                  const std::vector<TerrainChunk> &all_chunks,
                                  const TerrainBrush &brush);
void validate_terrain_pyramid(const Planet &,
                              const std::vector<TerrainChunk> &all_chunks);
bool overlaps(const TerrainBrush &, const TerrainLock &);

} // namespace inde::project::geo
