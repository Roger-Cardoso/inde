#pragma once
#include "inde/persistence/cartography_store.hpp"

namespace inde::persistence {
class SqliteCartographyRepository final : public CartographyStore {
public:
  std::vector<project::geo::Planet>
  planets(const std::filesystem::path &) const override;
  void create(const std::filesystem::path &,
              const project::geo::GeneratedPlanet &) const override;
  std::vector<project::geo::TerrainChunk>
  chunks(const std::filesystem::path &, const std::string &,
         const std::vector<project::geo::ChunkKey> &) const override;
  std::vector<project::geo::PositionedLocal>
  locals(const std::filesystem::path &, const std::string &,
         project::geo::Bounds, int, std::size_t) const override;
  std::optional<project::geo::Position>
  position(const std::filesystem::path &, const std::string &,
           const std::string &) const override;
  void
  change_position(const std::filesystem::path &, const std::string &,
                  const std::string &,
                  const std::optional<project::geo::Position> &,
                  const std::optional<project::geo::Position> &) const override;
  int apply_terrain(const std::filesystem::path &,
                    const project::geo::TerrainPatch &, int expected_revision,
                    bool reverse) const override;
  std::vector<project::geo::TerrainLock>
  terrain_locks(const std::filesystem::path &,
                const std::string &planet) const override;
  void add_terrain_lock(const std::filesystem::path &,
                        const project::geo::TerrainLock &) const override;
  void remove_terrain_lock(const std::filesystem::path &,
                           const project::geo::TerrainLock &) const override;
};
} // namespace inde::persistence
