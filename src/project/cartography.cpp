#include "inde/project/cartography.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <set>
#include <stdexcept>

namespace inde::project::geo {
namespace {
constexpr double radians = std::numbers::pi / 180.0;
std::uint64_t mix(std::uint64_t x) {
  x += 0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}
double lattice(int x, int y, int z, std::uint64_t seed) {
  const auto h =
      mix(seed ^ mix(static_cast<std::uint64_t>(x)) ^
          mix(static_cast<std::uint64_t>(y) + 0x517cc1b727220a95ULL) ^
          mix(static_cast<std::uint64_t>(z) + 0x6eed0e9da4d94a4fULL));
  return static_cast<double>(h >> 11) / 9007199254740992.0 * 2.0 - 1.0;
}
double noise(double x, double y, double z, std::uint64_t seed) {
  const int ix = static_cast<int>(std::floor(x)),
            iy = static_cast<int>(std::floor(y)),
            iz = static_cast<int>(std::floor(z));
  const auto smooth = [](double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
  };
  const double fx = smooth(x - ix), fy = smooth(y - iy), fz = smooth(z - iz);
  double value = 0;
  for (int dz = 0; dz < 2; ++dz)
    for (int dy = 0; dy < 2; ++dy)
      for (int dx = 0; dx < 2; ++dx)
        value += lattice(ix + dx, iy + dy, iz + dz, seed) * (dx ? fx : 1 - fx) *
                 (dy ? fy : 1 - fy) * (dz ? fz : 1 - fz);
  return value;
}
void check_stop(std::stop_token stop) {
  if (stop.stop_requested())
    throw std::runtime_error("Geração cancelada");
}
} // namespace

double normalize_longitude(double value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Longitude inválida");
  double result = std::fmod(value + 180.0, 360.0);
  if (result < 0)
    result += 360.0;
  return result - 180.0;
}
void validate(Coordinate v) {
  if (!std::isfinite(v.longitude) || !std::isfinite(v.latitude) ||
      v.longitude < -180 || v.longitude > 180 || v.latitude < -90 ||
      v.latitude > 90)
    throw std::runtime_error("Coordenadas fora dos limites do planeta");
}
void validate(Bounds v) {
  validate(Coordinate{v.west, v.south});
  validate(Coordinate{v.east, v.north});
  if (v.west >= v.east || v.south >= v.north)
    throw std::runtime_error("Região cartográfica inválida");
}
void validate(const Planet &p) {
  if (p.id.empty() || p.name.empty() || p.name.size() > 512 ||
      p.created_at.empty() || p.radius_m < 1000 || p.radius_m > 1000000000 ||
      p.seed < 0 || p.seed > 2147483647 || p.water_percent < 5 ||
      p.water_percent > 95 || p.fragmentation < 1 || p.fragmentation > 8 ||
      p.height_m < 100 || p.height_m > 30000 || p.depth_m < 100 ||
      p.depth_m > 30000 || p.detail < 0 || p.detail > max_terrain_level ||
      p.generator != generator_version || p.terrain_revision < 0)
    throw std::runtime_error(
        "Parâmetros do planeta inválidos ou gerador não suportado");
}
void validate(const TerrainBrush &b) {
  validate(b.center);
  if (b.planet_id.empty() || !std::isfinite(b.radius_degrees) ||
      b.radius_degrees < 0.5 || b.radius_degrees > 15 || b.strength_m < 50 ||
      b.strength_m > 4000)
    throw std::runtime_error("Pincel de terreno inválido");
}
void validate(const TerrainLock &lock) {
  validate(lock.center);
  if (lock.id.empty() || lock.planet_id.empty() || lock.name.empty() ||
      lock.name.size() > 256 || lock.created_at.empty() ||
      !std::isfinite(lock.radius_degrees) || lock.radius_degrees < 0.5 ||
      lock.radius_degrees > 15)
    throw std::runtime_error("Bloqueio cartográfico inválido");
}
void validate(const Position &p) {
  validate(p.coordinate);
  if (p.planet_id.empty() || p.entity_id.empty() || p.importance < 1 ||
      p.importance > 100 || p.min_level < 0 || p.min_level > 6 ||
      (p.symbol != "place" && p.symbol != "city" && p.symbol != "mountain"))
    throw std::runtime_error("Posição cartográfica inválida");
}
void validate(ChunkKey k) {
  if (k.level < 0 || k.level > max_terrain_level || k.x < 0 || k.y < 0 ||
      k.x >= (2 << k.level) || k.y >= (1 << k.level))
    throw std::runtime_error("Chave de chunk inválida");
}
double distance_m(Coordinate a, Coordinate b, std::int64_t radius) {
  validate(a);
  validate(b);
  if (radius < 1000 || radius > 1000000000)
    throw std::runtime_error("Raio inválido");
  const double dlat = (b.latitude - a.latitude) * radians;
  const double dlon = normalize_longitude(b.longitude - a.longitude) * radians;
  const double h =
      std::pow(std::sin(dlat / 2), 2) + std::cos(a.latitude * radians) *
                                            std::cos(b.latitude * radians) *
                                            std::pow(std::sin(dlon / 2), 2);
  return 2.0 * radius * std::asin(std::sqrt(std::clamp(h, 0.0, 1.0)));
}
Bounds chunk_bounds(ChunkKey key) {
  validate(key);
  const double span = 180.0 / (1 << key.level);
  return {-180 + key.x * span, 90 - (key.y + 1) * span,
          -180 + (key.x + 1) * span, 90 - key.y * span};
}
std::vector<ChunkKey> visible_chunks(Bounds b, int level) {
  validate(b);
  validate(ChunkKey{level, 0, 0});
  const double span = 180.0 / (1 << level);
  const int x0 = std::clamp(static_cast<int>(std::floor((b.west + 180) / span)),
                            0, (2 << level) - 1);
  const int x1 =
      std::clamp(static_cast<int>(std::ceil((b.east + 180) / span)) - 1, 0,
                 (2 << level) - 1);
  const int y0 = std::clamp(static_cast<int>(std::floor((90 - b.north) / span)),
                            0, (1 << level) - 1);
  const int y1 =
      std::clamp(static_cast<int>(std::ceil((90 - b.south) / span)) - 1, 0,
                 (1 << level) - 1);
  std::vector<ChunkKey> out;
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x)
      out.push_back({level, x, y});
  return out;
}
GeneratedPlanet generate(Planet planet, std::stop_token stop) {
  validate(planet);
  check_stop(stop);
  const int width = chunk_side * (2 << planet.detail), height = width / 2;
  // Explicitly bounded construction buffer (maximum 1024 x 512), not a
  // globally fixed planet resolution. Future refinements add regional chunks.
  std::vector<double> values(static_cast<std::size_t>(width) * height);
  struct Weighted {
    double value, weight;
  };
  std::vector<Weighted> ordered;
  ordered.reserve(values.size());
  double total_weight = 0;
  for (int y = 0; y < height; ++y) {
    check_stop(stop);
    const double lat = (90.0 - (y + 0.5) * 180.0 / height) * radians;
    const double weight = std::cos(lat);
    for (int x = 0; x < width; ++x) {
      const double lon = (-180.0 + (x + 0.5) * 360.0 / width) * radians;
      const double sx = std::cos(lat) * std::cos(lon),
                   sy = std::cos(lat) * std::sin(lon), sz = std::sin(lat);
      double n = 0, amplitude = 1, frequency = planet.fragmentation * 0.7 + 0.8,
             sum = 0;
      for (int octave = 0; octave < 5; ++octave) {
        n +=
            amplitude * noise(sx * frequency + 13.7, sy * frequency + 8.3,
                              sz * frequency + 2.9,
                              static_cast<std::uint64_t>(planet.seed) + octave);
        sum += amplitude;
        amplitude *= 0.48;
        frequency *= 2.03;
      }
      n /= sum;
      values[static_cast<std::size_t>(y) * width + x] = n;
      ordered.push_back({n, weight});
      total_weight += weight;
    }
  }
  check_stop(stop);
  std::sort(ordered.begin(), ordered.end(),
            [](const auto &a, const auto &b) { return a.value < b.value; });
  const double threshold_weight = total_weight * planet.water_percent / 100.0;
  double accumulated = 0, sea = 0;
  for (const auto &v : ordered) {
    accumulated += v.weight;
    if (accumulated >= threshold_weight) {
      sea = v.value;
      break;
    }
  }
  const double low = ordered.front().value, high = ordered.back().value;
  std::vector<std::int16_t> grid(values.size());
  for (std::size_t i = 0; i < grid.size(); ++i) {
    const double delta = values[i] - sea;
    const double normalized = delta >= 0
                                  ? delta / std::max(0.000001, high - sea)
                                  : delta / std::max(0.000001, sea - low);
    grid[i] = static_cast<std::int16_t>(std::lround(
        normalized * (delta >= 0 ? planet.height_m : planet.depth_m)));
  }
  GeneratedPlanet result{std::move(planet), {}};
  int w = width, h = height;
  for (int level = result.planet.detail; level >= 0; --level) {
    check_stop(stop);
    for (int ty = 0; ty < h / chunk_side; ++ty)
      for (int tx = 0; tx < w / chunk_side; ++tx) {
        TerrainChunk chunk{{level, tx, ty}, {}};
        for (int y = 0; y < chunk_side; ++y)
          for (int x = 0; x < chunk_side; ++x)
            chunk.elevation[y * chunk_side + x] =
                grid[(ty * chunk_side + y) * w + tx * chunk_side + x];
        result.chunks.push_back(std::move(chunk));
      }
    if (level == 0)
      break;
    std::vector<std::int16_t> coarse(static_cast<std::size_t>(w / 2) * (h / 2));
    for (int y = 0; y < h / 2; ++y)
      for (int x = 0; x < w / 2; ++x) {
        const auto at = static_cast<std::size_t>(2 * y) * w + 2 * x;
        coarse[y * (w / 2) + x] = static_cast<std::int16_t>(
            (static_cast<int>(grid[at]) + grid[at + 1] + grid[at + w] +
             grid[at + w + 1]) /
            4);
      }
    grid = std::move(coarse);
    w /= 2;
    h /= 2;
  }
  return result;
}

TerrainPatch preview_terrain_edit(const Planet &planet,
                                  const std::vector<TerrainChunk> &all_chunks,
                                  const TerrainBrush &brush) {
  validate(planet);
  validate(brush);
  validate_terrain_pyramid(planet, all_chunks);
  if (brush.planet_id != planet.id)
    throw std::runtime_error("Pincel pertence a outro planeta");

  std::map<ChunkKey, const TerrainChunk *> stored;
  std::size_t required = 0;
  for (int level = 0; level <= planet.detail; ++level)
    for (const auto key : visible_chunks({}, level)) {
      ++required;
      const auto found =
          std::find_if(all_chunks.begin(), all_chunks.end(),
                       [&](const auto &c) { return c.key == key; });
      if (found == all_chunks.end() || !stored.emplace(key, &*found).second)
        throw std::runtime_error(
            "Pirâmide necessária à edição está incompleta");
    }
  if (all_chunks.size() != required)
    throw std::runtime_error("Pirâmide de edição contém chunks inesperados");

  const int finest = planet.detail;
  int width = chunk_side * (2 << finest), height = width / 2;
  std::vector<std::int16_t> original(static_cast<std::size_t>(width) * height);
  for (const auto &[key, chunk] : stored) {
    if (key.level != finest)
      continue;
    for (int y = 0; y < chunk_side; ++y)
      for (int x = 0; x < chunk_side; ++x)
        original[static_cast<std::size_t>(key.y * chunk_side + y) * width +
                 key.x * chunk_side + x] = chunk->elevation[y * chunk_side + x];
  }
  auto edited = original;
  std::size_t changed = 0;
  for (int y = 0; y < height; ++y) {
    const double latitude = 90.0 - (y + 0.5) * 180.0 / height;
    for (int x = 0; x < width; ++x) {
      const double longitude = -180.0 + (x + 0.5) * 360.0 / width;
      const double angular =
          distance_m(brush.center, {longitude, latitude}, planet.radius_m) /
          planet.radius_m / radians;
      if (angular > brush.radius_degrees)
        continue;
      const double falloff = std::pow(1.0 - angular / brush.radius_degrees, 2);
      const auto at = static_cast<std::size_t>(y) * width + x;
      int next = original[at];
      if (brush.mode == TerrainEditMode::Raise ||
          brush.mode == TerrainEditMode::Lower) {
        const int direction = brush.mode == TerrainEditMode::Raise ? 1 : -1;
        next += direction * std::max(1, static_cast<int>(std::lround(
                                            brush.strength_m * falloff)));
      } else {
        int sum = 0;
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            const int sy = std::clamp(y + dy, 0, height - 1);
            const int sx = (x + dx + width) % width;
            sum += original[static_cast<std::size_t>(sy) * width + sx];
          }
        const double mix = std::min(1.0, brush.strength_m / 4000.0) * falloff;
        next = static_cast<int>(
            std::lround(original[at] + (sum / 9.0 - original[at]) * mix));
      }
      next = std::clamp(next, -planet.depth_m, planet.height_m);
      if (next != original[at]) {
        edited[at] = static_cast<std::int16_t>(next);
        ++changed;
      }
    }
  }
  if (changed == 0)
    throw std::runtime_error("O pincel não alterou nenhuma amostra de terreno");

  TerrainPatch patch{
      planet.id, brush.center, brush.radius_degrees, changed, {}};
  for (int level = finest; level >= 0; --level) {
    for (int ty = 0; ty < height / chunk_side; ++ty)
      for (int tx = 0; tx < width / chunk_side; ++tx) {
        const ChunkKey key{level, tx, ty};
        TerrainChunk after{key, {}};
        for (int y = 0; y < chunk_side; ++y)
          for (int x = 0; x < chunk_side; ++x)
            after.elevation[y * chunk_side + x] =
                edited[static_cast<std::size_t>(ty * chunk_side + y) * width +
                       tx * chunk_side + x];
        const auto &before = *stored.at(key);
        if (after != before)
          patch.chunks.push_back({key, before, std::move(after)});
      }
    if (level == 0)
      break;
    std::vector<std::int16_t> coarse(static_cast<std::size_t>(width / 2) *
                                     (height / 2));
    for (int y = 0; y < height / 2; ++y)
      for (int x = 0; x < width / 2; ++x) {
        const auto at = static_cast<std::size_t>(2 * y) * width + 2 * x;
        coarse[static_cast<std::size_t>(y) * (width / 2) + x] =
            static_cast<std::int16_t>((static_cast<int>(edited[at]) +
                                       edited[at + 1] + edited[at + width] +
                                       edited[at + width + 1]) /
                                      4);
      }
    edited = std::move(coarse);
    width /= 2;
    height /= 2;
  }
  return patch;
}

void validate_terrain_pyramid(const Planet &planet,
                              const std::vector<TerrainChunk> &chunks) {
  validate(planet);
  std::map<ChunkKey, const TerrainChunk *> indexed;
  std::size_t required = 0;
  for (int level = 0; level <= planet.detail; ++level)
    required += visible_chunks({}, level).size();
  if (chunks.size() != required)
    throw std::runtime_error("Pirâmide de terreno incompleta");
  for (const auto &chunk : chunks) {
    validate(chunk.key);
    if (chunk.key.level > planet.detail ||
        !indexed.emplace(chunk.key, &chunk).second)
      throw std::runtime_error("Pirâmide contém chunk repetido ou inesperado");
    for (const auto value : chunk.elevation)
      if (value < -planet.depth_m || value > planet.height_m)
        throw std::runtime_error("Elevação fora da escala declarada");
  }
  for (const auto &[key, coarse] : indexed) {
    if (key.level >= planet.detail)
      continue;
    for (int y = 0; y < chunk_side; ++y)
      for (int x = 0; x < chunk_side; ++x) {
        const ChunkKey child_key{key.level + 1, key.x * 2 + x / 32,
                                 key.y * 2 + y / 32};
        const auto child = indexed.find(child_key);
        if (child == indexed.end())
          throw std::runtime_error("Nível descendente de terreno ausente");
        const int at = (y % 32) * 2 * chunk_side + (x % 32) * 2;
        const auto &fine = child->second->elevation;
        const int expected =
            (static_cast<int>(fine[at]) + fine[at + 1] + fine[at + chunk_side] +
             fine[at + chunk_side + 1]) /
            4;
        if (coarse->elevation[y * chunk_side + x] != expected)
          throw std::runtime_error("Ancestral de terreno está desatualizado");
      }
  }
}

bool overlaps(const TerrainBrush &brush, const TerrainLock &lock) {
  validate(brush);
  validate(lock);
  if (brush.planet_id != lock.planet_id)
    return false;
  const double angular =
      distance_m(brush.center, lock.center, 1000000) / 1000000 / radians;
  return angular <= brush.radius_degrees + lock.radius_degrees;
}
} // namespace inde::project::geo
