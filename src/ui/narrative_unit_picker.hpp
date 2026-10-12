#pragma once
#include "inde/application/project_service.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/incremental_selector.hpp"
#include <algorithm>
namespace inde::ui {
// Two explicit scopes plus one bounded result page. No name matching across
// structures, no implicit active-work or first-unit choice.
class NarrativeUnitPicker final : public Gtk::Box {
public:
  explicit NarrativeUnitPicker(application::ProjectService &s)
      : Gtk::Box(Gtk::Orientation::VERTICAL, 8), service_(s) {
    for (const auto &w : s.catalog().works)
      works_.append(w.id, w.title);
    bound_choice_width(works_);
    bound_choice_width(structures_);
    append_labeled_form_field(*this, "Obra", works_);
    append_labeled_form_field(*this, "Plano narrativo", structures_);
    units_ = Gtk::make_managed<IncrementalSelector>(
        "Pesquisar parte nesta estrutura",
        [this](const std::string &query, std::size_t limit) {
          std::vector<IncrementalSelection> result;
          if (!snapshot_)
            return result;
          const auto key = project::alias_key(query);
          for (const auto &u : snapshot_->units)
            if (project::alias_key(u.title).find(key) != std::string::npos) {
              result.push_back({u.id, u.title, u.purpose});
              if (result.size() >= limit)
                break;
            }
          return result;
        });
    append(*units_);
    message_.set_wrap(true);
    message_.set_xalign(0);
    append(message_);
    if (s.catalog().works.empty())
      message_.set_text(
          "Crie uma Obra em Planejar para organizar estas partes. Nenhuma Obra será escolhida automaticamente.");
    works_.signal_changed().connect([this] {
      structures_.remove_all();
      snapshot_.reset();
      units_->clear_selection();
      try {
        message_.set_text("");
        for (const auto &v : service_.structures().narrative_structures(
                 works_.get_active_id().raw()))
          structures_.append(v.id, v.name);
      } catch (const std::exception &e) {
        message_.set_text(e.what());
      }
    });
    structures_.signal_changed().connect([this] {
      snapshot_.reset();
      units_->clear_selection();
      const auto id = structures_.get_active_id().raw();
      try {
        message_.set_text("");
        if (!id.empty())
          snapshot_ = service_.builder().snapshot(id);
        units_->refresh();
      } catch (const std::exception &e) {
        message_.set_text(e.what());
      }
    });
  }
  const application::NarrativeBuilderSnapshot &snapshot() const {
    if (!snapshot_)
      throw std::runtime_error("Escolha Obra, estrutura e parte; se necessário crie-as em Planejar");
    return *snapshot_;
  }
  std::string unit() const {
    if (!units_->selected_id())
      throw std::runtime_error("Escolha uma parte da narrativa");
    return *units_->selected_id();
  }

private:
  application::ProjectService &service_;
  Gtk::ComboBoxText works_, structures_;
  Gtk::Label message_;
  IncrementalSelector *units_{};
  std::optional<application::NarrativeBuilderSnapshot> snapshot_;
};
} // namespace inde::ui
