#include "inde/application/project_service.hpp"
#include "inde/project/structural_node.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>

namespace {

using Clock = std::chrono::steady_clock;

template <class Operation> double milliseconds(Operation operation) {
  const auto start = Clock::now();
  operation();
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}

struct TemporaryDirectory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("inde-workspace-benchmark-" + inde::project::new_uuid());
  TemporaryDirectory() { std::filesystem::create_directories(path); }
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
};

struct Counts {
  std::size_t structural_nodes{};
  std::size_t structural_labels{};
  std::size_t entities{};
  std::size_t relations{};
  std::size_t documents{};
  std::size_t groups{};
  std::size_t points{};
  std::size_t events{};
  std::size_t presences{};
};

} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    throw std::runtime_error(
        "Uso: inde_workspace_benchmark <Projeto.inde>");
  const std::filesystem::path source = argv[1];
  if (!std::filesystem::is_directory(source))
    throw std::runtime_error("Projeto de benchmark não encontrado");

  TemporaryDirectory temporary;
  const auto copy = temporary.path / source.filename();
  std::filesystem::copy(source, copy,
                        std::filesystem::copy_options::recursive);
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);

  inde::application::ProjectService service;
  const auto first_open_ms =
      milliseconds([&] { static_cast<void>(service.open(copy)); });
  service.close();
  const auto reopen_ms =
      milliseconds([&] { static_cast<void>(service.open(copy)); });

  constexpr int iterations = 20;
  double editorial_total{};
  double planning_total{};
  double relations_total{};
  double writing_total{};
  double graphs_total{};
  Counts counts;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    editorial_total += milliseconds([&] {
      counts.structural_nodes = 0;
      counts.structural_labels = 0;
      for (const auto &work : service.catalog().works) {
        const auto nodes = service.structural_nodes_for_work(work.id);
        counts.structural_nodes += nodes.size();
        counts.structural_labels +=
            inde::project::structural_node_path_labels(nodes).size();
      }
    });

    planning_total += milliseconds([&] {
      static_cast<void>(service.narrative().entity_types());
      static_cast<void>(service.planning().time_axes());
      const auto snapshot =
          service.planning().explore_entities(service.planning_context());
      counts.entities = snapshot.matching_count;
    });

    relations_total += milliseconds([&] {
      static_cast<void>(service.narrative().relation_types());
      inde::persistence::RelationQuery query;
      query.limit = 200;
      counts.relations = service.narrative().relations(query).size();
    });

    writing_total += milliseconds([&] {
      counts.groups = service.writing().document_groups().size();
      inde::persistence::DocumentQuery query;
      query.limit = 100;
      counts.documents = service.writing().document_summaries(query).size();
      static_cast<void>(service.writing().document_count(query));
    });

    graphs_total += milliseconds([&] {
      const auto axes = service.planning().time_axes();
      if (axes.empty())
        return;
      inde::persistence::PlanningQuery point_query;
      point_query.limit = 500;
      counts.points =
          service.planning().time_points(axes.front().id, point_query).size();
      auto context = service.planning_context();
      context.fictional_axis_id = axes.front().id;
      context.fictional_time_point_id.reset();
      context.fictional_window_start_time_point_id.reset();
      context.fictional_window_end_time_point_id.reset();
      auto view_context = service.planning().temporal_view_context(context);
      view_context.limit = 500;
      const auto snapshot = service.planning().temporal_view(view_context);
      counts.events = snapshot.timeline.events.size();
      counts.presences = snapshot.timeline.presences.size();
    });
  }

  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  std::cout << "fixture=" << source.filename().string() << '\n'
            << "first_open_or_migration_ms=" << first_open_ms << '\n'
            << "reopen_ms=" << reopen_ms << '\n'
            << "iterations=" << iterations << '\n'
            << "editorial_services_mean_ms="
            << editorial_total / iterations << '\n'
            << "planning_services_mean_ms=" << planning_total / iterations
            << '\n'
            << "relations_services_mean_ms=" << relations_total / iterations
            << '\n'
            << "writing_services_mean_ms=" << writing_total / iterations
            << '\n'
            << "graphs_services_mean_ms=" << graphs_total / iterations << '\n'
            << "structural_nodes=" << counts.structural_nodes
            << " structural_labels=" << counts.structural_labels
            << " entities=" << counts.entities
            << " relations_page=" << counts.relations
            << " documents_page=" << counts.documents
            << " groups=" << counts.groups << " points=" << counts.points
            << " events=" << counts.events
            << " presences=" << counts.presences << '\n'
            << "max_rss_kib=" << usage.ru_maxrss << '\n';
}
