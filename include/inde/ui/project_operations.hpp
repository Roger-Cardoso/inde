#pragma once
#include "inde/application/project_service.hpp"
#include <functional>
#include <gtkmm.h>

namespace inde::ui {
class OperationConfirmation final : public Gtk::Box {
public:
  OperationConfirmation(application::ProjectService &,
                        project::OperationPreview,
                        std::function<void()> completed,
                        std::function<void(const project::OperationPreview &, const project::RemovalUse &)> open_use = {});

private:
  void prepare();
  void confirm();
  void show_uses();
  application::ProjectService &service_;
  project::OperationPreview preview_;
  std::function<void()> completed_;
  std::function<void(const project::OperationPreview &, const project::RemovalUse &)> open_use_;
  Gtk::Label description_, effects_, message_;
  Gtk::CheckButton acknowledge_{"Conferi os efeitos desta operação"};
  Gtk::Box actions_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button refresh_{"Atualizar prévia"}, confirm_{"Confirmar operação"};
  Gtk::Box uses_{Gtk::Orientation::VERTICAL, 8}, pagination_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label usage_heading_;
  Gtk::SearchEntry usage_search_;
  Gtk::Button previous_{"Usos anteriores"}, next_{"Próximos usos"};
  std::size_t usage_offset_{};
  bool committed_{};
};
class ProjectOperationsView final : public Gtk::Box {
public:
  ProjectOperationsView(application::ProjectService &,
                        std::function<void()> changed);
  void open_operation(const std::string &id) { detail(id); }

private:
  void list();
  void detail(const std::string &);
  application::ProjectService &service_;
  std::function<void()> changed_;
  Gtk::Stack pages_;
  Gtk::Box queue_{Gtk::Orientation::VERTICAL, 8},
      detail_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label info_, empty_;
  Gtk::ListBox rows_;
  Gtk::Box pagination_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button previous_{"Anteriores"}, next_{"Próximas"},
      back_{"Voltar ao histórico"};
  std::size_t offset_{};
};
void confirm_project_removal(Gtk::Window &, application::ProjectService &,
                             project::RemovalKind, const std::string &,
                             std::function<void()> completed);
} // namespace inde::ui
