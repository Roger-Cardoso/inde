#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/temporal_context.hpp"

#include <algorithm>

namespace inde::ui {

IncrementalSelector *PlanningWorkspace::make_time_point_selector() const {
  auto *selector = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar marco na linha do tempo",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        for (const auto &axis : axes_) {
          persistence::PlanningQuery query;
          query.search = search;
          query.limit = std::min(limit - result.size(), std::size_t{50});
          if (query.limit == 0)
            break;
          for (const auto &point :
               service_.planning().time_points(axis.id, query)) {
            result.push_back({point.id,
                              axis.name + " — " +
                                  service_.worldbuilding().point_date(point) + " — " +
                                  point.label,
                              point.description});
          }
        }
        return result;
      },
      "Pesquise ou escolha um marco na linha do tempo");
  append_time_point_calendar(*selector, service_);
  return selector;
}

IncrementalSelector *PlanningWorkspace::make_entity_selector(
    const std::optional<std::string> &type_id, bool exclude_events,
    const std::optional<std::string> &exclude_id) const {
  const auto event_type = type_id_for_key("event");
  return Gtk::make_managed<IncrementalSelector>(
      "Pesquisar elemento da história",
      [this, type_id, exclude_events, exclude_id,
       event_type](const std::string &search, std::size_t limit) {
        persistence::EntityQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        if (type_id)
          query.entity_type_ids = {*type_id};
        std::vector<IncrementalSelection> result;
        for (const auto &entity : service_.narrative().entities(query)) {
          if (exclude_id && entity.id == *exclude_id)
            continue;
          if (exclude_events && event_type &&
              entity.entity_type_id == *event_type)
            continue;
          result.push_back(
              {entity.id, entity.name, type_name(entity.entity_type_id)});
        }
        return result;
      },
      "Pesquise ou escolha um elemento da história");
}

} // namespace inde::ui
