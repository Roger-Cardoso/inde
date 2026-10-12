#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"

namespace inde::ui {
namespace {
Gtk::Label *text(const std::string &s) {
  auto *label = Gtk::make_managed<Gtk::Label>(s);
  label->set_xalign(0);
  label->set_wrap(true);
  label->set_max_width_chars(72);
  return label;
}
} // namespace
void NarrativeStructureWorkspace::choose_comparison() {
  if ((selection_source_.empty() && !selected_structure_id_) || !owner_window())
    return;
  const auto target = selection_source_.empty()?*selected_structure_id_:selection_source_;
  try {
    const auto snapshot = service_.builder().snapshot(target);
    auto *dialog =
        new OverlayDialog("Comparar alternativas", *owner_window(), true);
    dialog->set_secondary_text(
        "Consulta somente leitura. Itens correspondem por origem e histórico, nunca por nome. Nenhuma alternativa será ativada, mesclada ou aplicada ao texto.");
    auto *choice = Gtk::make_managed<Gtk::ComboBoxText>();
    if (!snapshot.provenance_json.empty())
      choice->append("origin", "Ponto de derivação registrado (imutável)");
    for (const auto &s :
         service_.structures().narrative_structures(selected_work_id()))
      if (s.id != target)
        choice->append(s.id, s.name + " (estado atual)");
    choice->set_active(0);
    append_labeled_form_field(*dialog->get_content_area(), "Comparar com",
                              *choice);
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Consultar diferenças", Gtk::ResponseType::ACCEPT);
    dialog->signal_response().connect(
        [this, dialog, target, choice](int response) {
          if (response != Gtk::ResponseType::ACCEPT)
            return;
          try {
            const auto selected = choice->get_active_id().raw();
            if (selected.empty())
              throw std::runtime_error(
                  "Crie uma alternativa ou escolha uma origem disponível.");
            comparison_ = selected == "origin"
                              ? service_.builder().compare_to_origin(target)
                              : service_.builder().compare(selected, target);
            comparison_source_ = selected == "origin" ? "" : selected;
            comparison_target_ = target;
            comparison_targets_.clear();comparison_target_combo_.set_visible(false);
            cancel_comparison_selection();
            comparison_page_index_ = 0;
            render_comparison();
            stack_.set_visible_child("comparison");
          } catch (const std::exception &e) {
            dialog->set_secondary_text(author::diagnostic(e.what()));
            dialog->keep_open_on_response();
          }
        });
    dialog->present();
  } catch (const std::exception &e) {
    show_error(e);
  }
}
void NarrativeStructureWorkspace::render_comparison() {
  while (auto *child = comparison_body_.get_first_child())
    comparison_body_.remove(*child);
  constexpr std::size_t page_size = 40;
  const auto pages = std::max<std::size_t>(
      1, (comparison_.differences.size() + page_size - 1) / page_size);
  comparison_page_index_ = std::min(comparison_page_index_, pages - 1);
  comparison_title_.set_text(
      comparison_.source_name + " → " + comparison_.target_name + " · " +
      std::to_string(comparison_.differences.size()) + " diferenças · página " +
      std::to_string(comparison_page_index_ + 1) + "/" + std::to_string(pages));
  comparison_body_.append(
      *text("Comparação de declarações e associações do planejamento. Não "
            "compara o corpo da prosa nem a validade das datas, não escolhe a "
            "melhor narrativa e não sincroniza as quatro ordens. Referências e "
            "Documentos compartilhados mantêm autoridade própria."));
  if (comparison_.differences.empty())
    comparison_body_.append(
        *text("Nenhuma diferença nas dimensões comparadas. Isso não é "
              "aprovação de qualidade, continuidade ou equivalência textual."));
  const auto first = comparison_page_index_ * page_size,
             last = std::min(first + page_size, comparison_.differences.size());
  for (auto i = first; i < last; ++i) {
    const auto d = comparison_.differences[i];
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    box->set_margin(8);
    box->append(*text(d.title + " · " + d.dimension));
    auto *details = Gtk::make_managed<Gtk::Expander>("Evidência da diferença");
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_min_content_height(90);
    scroll->set_max_content_height(220);
    scroll->set_propagate_natural_height(true);
    auto *view = Gtk::make_managed<Gtk::TextView>();
    view->set_editable(false);
    view->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    view->get_buffer()->set_text(d.detail);
    scroll->set_child(*view);
    details->set_child(*scroll);
    box->append(*details);
    auto *actions =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto action = [this, actions](const char *label,
                                  const std::string &structure,
                                  const std::string &unit) {
      if (structure.empty() || unit.empty())
        return;
      auto *b = Gtk::make_managed<Gtk::Button>(label);
      actions->append(*b);
      b->signal_clicked().connect([this, structure, unit] {
        selected_unit_id_ = unit;
        show_detail(structure);
        show_unit();
      });
    };
    action("Abrir parte na origem", comparison_source_, d.unit_id);
    action("Abrir parte no destino", comparison_target_, d.other_unit_id);
    box->append(*actions);
    comparison_body_.append(*box);
  }
  comparison_previous_.set_sensitive(comparison_page_index_ > 0);
  comparison_next_.set_sensitive(comparison_page_index_ + 1 < pages);
  comparison_scroll_.get_vadjustment()->set_value(0);
}
void NarrativeStructureWorkspace::choose_dependency_scope() {
  if (!selected_link_id_ || !owner_window())
    return;
  try {
    const auto expected = service_.builder().snapshot(*selected_structure_id_);
    const auto link = *selected_link_id_;
    auto *d = new OverlayDialog("A qual ordem esta regra se aplica?", *owner_window(), true);
    d->set_secondary_text(
        "Escolha a visão a que este vínculo se refere. Causalidade não obriga ordem de leitura. A declaração não reorganiza partes, datas ou manuscrito. Avisos de movimento avaliam apenas precede/depende na montagem ou fila escolhida; cronologia e editorial requerem consultas próprias.");
    auto *scope = Gtk::make_managed<Gtk::ComboBoxText>();
    for (auto [id, name] : {std::pair{"unclassified", "Não classificada"},
                            {"narrative", "Ordem do plano narrativo"},
                            {"chronology", "Cronologia dos fatos"},
                            {"writing", "Ordem de escrita"},
                            {"editorial", "Apresentação editorial"}})
      scope->append(id, name);
    scope->set_active_id("unclassified");
    for (auto &s : expected.scopes)
      if (s.link_id == link)
        scope->set_active_id(s.scope);
    append_labeled_form_field(*d->get_content_area(), "Visão do vínculo",
                              *scope);
    d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    d->add_button("Salvar autoridade", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect(
        [this, d, expected, scope, link](int response) {
          if (response != Gtk::ResponseType::ACCEPT)
            return;
          try {
            service_.builder().set_dependency_scope(expected, link,
                                                    scope->get_active_id());
            refresh_detail();
          } catch (const std::exception &e) {
            d->set_secondary_text(author::diagnostic(e.what()));
            d->keep_open_on_response();
          }
        });
    d->present();
  } catch (const std::exception &e) {
    show_error(e);
  }
}
} // namespace inde::ui
