#pragma once
#include "inde/project/cartography.hpp"
#include <filesystem>

namespace inde::persistence {
class CartographyStore {
public:
  virtual ~CartographyStore() = default;
  virtual std::vector<project::geo::Planet>
  planets(const std::filesystem::path &) const = 0;
  virtual void create(const std::filesystem::path &,
                      const project::geo::GeneratedPlanet &) const = 0;
  virtual std::vector<project::geo::TerrainChunk>
  chunks(const std::filesystem::path &, const std::string &planet,
         const std::vector<project::geo::ChunkKey> &) const = 0;
  virtual std::vector<project::geo::PositionedLocal>
  locals(const std::filesystem::path &, const std::string &planet,
         project::geo::Bounds, int level, std::size_t limit) const = 0;
  virtual std::optional<project::geo::Position>
  position(const std::filesystem::path &, const std::string &planet,
           const std::string &entity) const = 0;
  virtual void change_position(
      const std::filesystem::path &, const std::string &planet,
      const std::string &entity,
      const std::optional<project::geo::Position> &expected,
      const std::optional<project::geo::Position> &desired) const = 0;
  virtual int apply_terrain(const std::filesystem::path &,
                            const project::geo::TerrainPatch &,
                            int expected_revision, bool reverse) const = 0;
  virtual std::vector<project::geo::TerrainLock>
  terrain_locks(const std::filesystem::path &,
                const std::string &planet) const = 0;
  virtual void add_terrain_lock(const std::filesystem::path &,
                                const project::geo::TerrainLock &) const = 0;
  virtual void remove_terrain_lock(const std::filesystem::path &,
                                   const project::geo::TerrainLock &) const = 0;
};
} // namespace inde::persistence
