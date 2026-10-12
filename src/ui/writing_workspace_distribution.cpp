#include "editor_interaction_ui_support.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <iomanip>
#include <set>
#include <sstream>
namespace inde::ui {
using namespace interaction_ui;
void WritingWorkspace::show_semantic_distribution() {
  if (!owner_window())
    return;
  try {
    auto c = interaction_context(false);
    const bool selected = c.range.start != c.range.end;
    if (!selected)
      c.range = {0, project::utf8_character_count(c.state.document.content)};
    const auto types = service_.writing().semantic_types();
    const auto report =
        application::semantic_distribution(c.state.document, types, c.range);
    auto *dialog = new OverlayDialog("Distribuição das marcações do texto",
                                     *owner_window(), true);
    dialog->set_secondary_text(
        "Análise das marcações declaradas pelo autor, incluindo o rascunho "
        "atual. Não é classificação automática. Percentuais usam caracteres "
        "Unicode com espaços, não bytes; categorias podem se sobrepor.");
    dialog->add_button("Fechar", Gtk::ResponseType::CANCEL);
    auto &box = *dialog->get_content_area();
    label(box, std::string("Onde se aplica: ") +
                   (selected ? "seleção atual" : "Documento inteiro") + " · " +
                   std::to_string(report.characters) + " caracteres")
        ->add_css_class("title-3");
    label(box, "Cobertura das marcações semânticas: " +
                   std::to_string(report.covered) + " · Sem marcação: " +
                   std::to_string(report.characters - report.covered) +
                   " · Sobreposição de categorias: " +
                   std::to_string(report.overlapping));
    if (report.covered == 0)
      label(box, "Ainda não há marcações do texto localizadas neste área da consulta. Selecione trechos e use Marcar Seleção (Narração, Descrição, Diálogo, Pensamento…). O texto sem marcação não é automaticamente classificado como narração.");
    for (const auto &row : report.rows) {
      auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
      card->add_css_class("tool-section");
      std::ostringstream percent;
      percent << std::fixed << std::setprecision(1)
              << (report.characters ? 100.0 * row.characters / report.characters
                                    : 0.0);
      label(*card, row.label + " — " + std::to_string(row.characters) +
                       " caracteres · " + percent.str() + "% · " +
                       std::to_string(row.markings) + " marcações");
      auto *bar = Gtk::make_managed<Gtk::ProgressBar>();
      bar->set_fraction(report.characters
                            ? static_cast<double>(row.characters) /
                                  report.characters
                            : 0);
      card->append(*bar);
      if (row.unresolved || row.incomplete || row.global)
        label(*card, "Pendências: " + std::to_string(row.unresolved) +
                         " trechos marcados não resolvidas (fora da cobertura; contagem no Documento inteiro) · " +
                         std::to_string(row.incomplete) +
                         " marcações com campos incompletos · " +
                         std::to_string(row.global) +
                         " declarações globais (sem cobertura artificial)");
      box.append(*card);
    }
    std::set<std::string> canonical_relations;
    std::size_t queried{};
    bool limited = report.entity_ids.size() > 32;
    auto *entities = Gtk::make_managed<Gtk::Expander>(
        "Elementos da história e relações encontradas neste área da consulta");
    auto *details = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    const auto entity_types = service_.narrative().entity_types();
    for (const auto &id : report.entity_ids) {
      if (queried++ >= 32)
        break;
      if (auto e = service_.narrative().entity(id))
        label(*details, e->name + " — " +
                            type_name(entity_types, e->entity_type_id) + "\n" +
                            clipped(e->summary));
      persistence::RelationQuery q;
      q.entity_id = id;
      q.limit = 200;
      const auto relations = service_.narrative().relations(q);
      limited |= relations.size() == 200;
      for (const auto &r : relations)
        canonical_relations.insert(r.id);
    }
    label(box, "Elementos da história distintas no área da consulta: " +
                   std::to_string(report.entity_ids.size()) +
                   " · Relações entre marcações incidentes ao área da consulta: " +
                   std::to_string(report.annotation_relations));
    label(box,
          "Relações registradas no mundo da história ligadas aos elementos da história: " +
              std::string(limited ? "pelo menos " : "") +
              std::to_string(canonical_relations.size()) +
              ". Essas relações pertencem ao Projeto: não recebem percentual "
              "de texto nem são confundidas com relações entre marcações.");
    if (limited)
      label(*details,
            "Consulta limitada a 32 elementos da história e 200 relações por elemento da história. Use Planejamento para explorar o conjunto completo.");
    entities->set_child(*details);
    box.append(*entities);
    const std::weak_ptr<int> life = clipboard_lifetime_;
    if (selected) {
      auto *whole =
          Gtk::make_managed<Gtk::Button>("Analisar Documento inteiro");
      box.append(*whole);
      whole->signal_clicked().connect([this, life, dialog, c] {
        dialog->hide();
        Glib::signal_idle().connect_once([this, life, c, dialog] {
          delete dialog;
          if (life.expired())
            return;
          try {
            require_interaction_context(c);
            editor_.get_buffer()->place_cursor(
                editor_.get_buffer()->get_iter_at_offset(
                    c.state.selection.cursor));
            show_semantic_distribution();
          } catch (const std::exception &e) {
            show_error("Análise desatualizada", e);
          }
        });
      });
    }
    auto *layers = Gtk::make_managed<Gtk::Button>(
        "Revisar marcações nas Marcações do texto");
    box.append(*layers);
    layers->signal_clicked().connect([this, life, dialog, c] {
      dialog->hide();
      Glib::signal_idle().connect_once([this, life, c, dialog] {
        delete dialog;
        if (life.expired())
          return;
        try {
          require_interaction_context(c);
          show_semantic_layers();
        } catch (const std::exception &e) {
          show_error("Análise desatualizada", e);
        }
      });
    });
    dialog->signal_response().connect([dialog](int) { dialog->hide(); });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível analisar o texto", e);
  }
}
} // namespace inde::ui
