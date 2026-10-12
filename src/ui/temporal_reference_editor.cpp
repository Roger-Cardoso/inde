#include "inde/ui/temporal_reference_editor.hpp"
#include "inde/ui/accessibility.hpp"
#include "temporal_form_helpers.hpp"
#include <algorithm>
namespace inde::ui {
TemporalReferenceEditor::TemporalReferenceEditor(
    const std::vector<project::FictionalCalendar> &calendars)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 6), calendars_(calendars) {
  constrain_temporal_combo(system_);
  name_.set_placeholder_text("Nome da nova referência");
  append_labeled_form_field(*this, "Nome da data ou período", name_);
  system_.append("", "Escolha o calendário de origem");
  for (const auto &c : calendars_)
    system_.append(c.id, c.name);
  system_.set_active(0);
  append_labeled_form_field(*this, "Calendário da data", system_);
  if (calendars_.empty()) {
    auto *help =
        Gtk::make_managed<Gtk::Label>("Crie um calendário e relógio em Planejar → Tempo para escolher o calendário de origem.");
    help->set_wrap(true);
    help->set_max_width_chars(56);
    help->set_xalign(0);
    append(*help);
  }
  selector_.set_child(picker_);
  picker_.set_visible(false);
  append(selector_);
  for (const auto &[key, caption] :
       std::vector<std::pair<std::string, std::string>>{
           {"exact", "Data informada"},
           {"between", "Entre as datas, incluindo início e fim"},
           {"before", "Antes"},
           {"after", "Depois"},
           {"approximate", "Aproximadamente"},
           {"season", "Estação / época"}})
    precision_.append(key, caption);
  precision_.set_active_id("exact");
  append_labeled_form_field(details_, "Quanto sabemos sobre a data", precision_);
  uncertainty_.set_placeholder_text("Explicação da incerteza ou estação");
  append_labeled_form_field(details_, "Explicação", uncertainty_);
  advanced_.set_child(details_);
  append(advanced_);
  system_.signal_changed().connect([this] {
    picker_.set_visible(!system_.get_active_id().empty());
    if (!system_.get_active_id().empty()) {
      picker_.set_calendar(calendar());
      selector_.set_expanded(true);
      selector_.set_label("Escolher data ou intervalo…");
    }
  });
  precision_.signal_changed().connect([this] {
    picker_.set_range(precision_.get_active_id() == "between");
    summarize();
  });
  uncertainty_.signal_changed().connect(
      sigc::mem_fun(*this, &TemporalReferenceEditor::summarize));
  picker_.signal_changed().connect(
      sigc::mem_fun(*this, &TemporalReferenceEditor::summarize));
}
void TemporalReferenceEditor::summarize() {
  try {
    auto r = draft("preview");
    if (r.label.empty())
      r.label = "Prévia";
    selector_.set_label(project::format_temporal_reference(calendar(), r) +
                        " — alterar…");
  } catch (const std::exception &) {
    selector_.set_label("Escolher data ou intervalo…");
  }
  if (auto *label = dynamic_cast<Gtk::Label *>(selector_.get_label_widget())) {
    label->set_wrap(true);
    label->set_max_width_chars(56);
    label->set_lines(2);
    label->set_ellipsize(Pango::EllipsizeMode::END);
    label->set_xalign(0);
  }
}
project::FictionalCalendar TemporalReferenceEditor::calendar() const {
  for (const auto &c : calendars_)
    if (c.id == system_.get_active_id().raw())
      return c;
  throw std::runtime_error("Escolha um calendário e relógio");
}
project::TemporalReference
TemporalReferenceEditor::draft(const std::string &axis) const {
  const auto c = calendar();
  const auto b = picker_.boundary();
  project::TemporalReference r;
  r.axis_id = axis;
  r.calendar_id = c.id;
  r.label = name_.get_text();
  r.year = b.year;
  r.month = b.month;
  r.day = b.day;
  r.clock = b.clock;
  r.qualifier = precision_.get_active_id();
  r.uncertainty = uncertainty_.get_text();
  if (r.qualifier == "between")
    r.upper = picker_.boundary(true);
  return r;
}
} // namespace inde::ui
