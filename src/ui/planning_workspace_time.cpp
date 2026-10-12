#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

std::int64_t PlanningWorkspace::parse_ordinal(const Glib::ustring &text) {
  const auto source = text.raw();
  std::int64_t value{};
  const auto result =
      std::from_chars(source.data(), source.data() + source.size(), value);
  if (source.empty() || result.ec != std::errc{} ||
      result.ptr != source.data() + source.size())
    throw std::runtime_error("A ordem temporal precisa ser um número inteiro");
  return value;
}

void PlanningWorkspace::create_time_axis() {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Nova linha do tempo", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome da linha do tempo (obrigatório)");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  form->set_margin(16);
  append_labeled_form_field(*form, "Nome da linha do tempo", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, name, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto axis = service_.planning().create_time_axis(
                name->get_text(), description->get_text());
            refresh_axes();
            axis_combo_.set_active_id(axis.id);
            refresh_time_points();
            signal_status_message_.emit("Linha do tempo criado");
          } catch (const std::exception &error) {
            show_error("Não foi possível criar a linha do tempo", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_time_axis() {
  const std::string id = axis_combo_.get_active_id().raw();
  const auto found =
      std::find_if(axes_.begin(), axes_.end(),
                   [&](const auto &axis) { return axis.id == id; });
  auto *owner = owner_window();
  if (!owner || found == axes_.end())
    return;
  auto original = *found;
  auto *dialog =
      new OverlayDialog("Editar linha do tempo", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Nome da linha do tempo", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, original, name, description](int response) mutable {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            original.name = name->get_text();
            original.description = description->get_text();
            static_cast<void>(service_.planning().update_time_axis(original));
            refresh_axes();
            axis_combo_.set_active_id(original.id);
            signal_status_message_.emit("Linha do tempo atualizado");
          } catch (const std::exception &error) {
            show_error("Não foi possível editar a linha do tempo", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::delete_time_axis() {
  const std::string id = axis_combo_.get_active_id().raw();
  auto *owner = owner_window();
  if (!owner || id.empty())
    return;
  confirm_project_removal(*owner, service_, project::RemovalKind::TimeAxis, id, [this] {
    refresh_axes();
    signal_status_message_.emit("Linha do tempo removido");
  });
}

void PlanningWorkspace::select_time_point(std::string id) {
  selected_time_point_id_ = std::move(id);
  point_edit_button_.set_sensitive(true);
  point_remove_button_.set_sensitive(true);
  point_actions_button_.set_sensitive(true);
  signal_status_message_.emit("Ponto de tempo da história selecionado");
}

void PlanningWorkspace::create_time_point() {
  const std::string axis_id = axis_combo_.get_active_id().raw();
  if(service_.current() && !axis_id.empty() && service_.worldbuilding().binding(axis_id)) {
    create_calendar_point(); return;
  }
  auto *owner = owner_window();
  if (!owner || axis_id.empty())
    return;
  auto *dialog =
      new OverlayDialog("Novo ponto de tempo da história", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *ordinal = Gtk::make_managed<Gtk::Entry>();
  ordinal->set_placeholder_text("Ordem inteira, por exemplo 1000");
  auto *label = Gtk::make_managed<Gtk::Entry>();
  label->set_placeholder_text("Rótulo (obrigatório)");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  form->set_margin(16);
  append_labeled_form_field(*form, "Posição na linha do tempo", *ordinal,
                            "Número inteiro; por exemplo, 1000.");
  append_labeled_form_field(*form, "Nome do marco", *label,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, axis_id, ordinal, label, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto point = service_.planning().create_time_point(
                axis_id, parse_ordinal(ordinal->get_text()), label->get_text(),
                description->get_text());
            refresh_time_points();
            select_time_point(point.id);
            refresh_temporal_context();
            signal_status_message_.emit("Ponto de tempo da história criado");
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o marco na linha do tempo", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_time_point() {
  if (!selected_time_point_id_)
    return;
  const auto found = std::find_if(
      time_points_.begin(), time_points_.end(),
      [&](const auto &point) { return point.id == *selected_time_point_id_; });
  auto *owner = owner_window();
  if (!owner || found == time_points_.end())
    return;
  auto original = *found;
  auto *dialog =
      new OverlayDialog("Editar ponto de tempo da história", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *axis = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : axes_)
    axis->append(value.id, value.name);
  axis->set_active_id(original.axis_id);
  auto *ordinal = Gtk::make_managed<Gtk::Entry>();
  ordinal->set_text(std::to_string(original.ordinal));
  auto *label = Gtk::make_managed<Gtk::Entry>();
  label->set_text(original.label);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Linha do tempo", *axis);
  append_labeled_form_field(*form, "Posição na linha do tempo", *ordinal,
                            "Número inteiro.");
  append_labeled_form_field(*form, "Nome do marco", *label,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, axis, ordinal,
                                     label, description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.axis_id = axis->get_active_id().raw();
        original.ordinal = parse_ordinal(ordinal->get_text());
        original.label = label->get_text();
        original.description = description->get_text();
        static_cast<void>(service_.planning().update_time_point(original));
        refresh_axes();
        axis_combo_.set_active_id(original.axis_id);
        refresh_time_points();
        signal_status_message_.emit("Ponto de tempo da história atualizado");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o marco na linha do tempo", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_time_point() {
  if (!selected_time_point_id_)
    return;
  const auto id = *selected_time_point_id_;
  auto *owner = owner_window();
  if (!owner)
    return;
  confirm_project_removal(*owner, service_, project::RemovalKind::TimePoint, id, [this] {
    refresh_time_points(); refresh_temporal_context();
    signal_status_message_.emit("Ponto de tempo da história removido");
  });
}

} // namespace inde::ui
