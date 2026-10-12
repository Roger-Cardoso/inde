#include "../src/ui/rendering_policy.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <chrono>
#include <iostream>
namespace {
using namespace inde::test::ui;
using Clock = std::chrono::steady_clock;
class SemanticWindow final : public Gtk::ApplicationWindow {
public:
  SemanticWindow(std::filesystem::path fixture, int &result)
      : writing_(service_), result_(result) {
    service_.open(fixture);
    auto d = service_.writing().create_document("QA camadas semânticas GTK");
    d.content = "Água 🙂 disse: olá. Pensou: amanhã.";
    id_ = d.id;
    original_ = d.content;
    service_.writing().update_document(d);
    auto types = service_.narrative().entity_types();
    auto t = std::find_if(types.begin(), types.end(),
                          [](auto &t) { return t.key == "character"; });
    require(t != types.end(), "Character type absent");
    entity_ = service_.narrative()
                  .create_entity(t->id, "QA falante do kernel GTK")
                  .id;
    host_.set_content(writing_);
    set_child(host_);
    add_css_class("inde-window");
    set_default_size(1280, 900);
    writing_.install_actions(*this);
    writing_.refresh();
    writing_.reveal_document(id_);
    editor_ =
        find<Gtk::TextView>(writing_, [](auto &e) { return e.get_editable(); });
    require(editor_, "No editor");
    started_ = Clock::now();
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL semantics GTK: " << e.what() << std::endl;
            hide();
            return false;
          }
        },
        50);
  }
  ~SemanticWindow() override { timer_.disconnect(); }

private:
  void choose(const std::string &id) {
    auto *c = find<Gtk::ComboBoxText>(host_, [&](auto &c) {
      for (auto *p = c.get_parent(); p; p = p->get_parent())
        if (p->has_css_class("modal-body"))
          return c.set_active_id(id);
      return false;
    });
    require(c, "No combo choice: " + id);
  }
  void select(int a, int b) {
    auto buffer = editor_->get_buffer();
    buffer->select_range(buffer->get_iter_at_offset(a),
                         buffer->get_iter_at_offset(b));
  }
  inde::project::Document saved() {
    writing_.flush_changes();
    auto d = service_.writing().document(id_);
    require(d.has_value(), "Document absent");
    return *d;
  }
  void begin(const std::string &type, int a, int b) {
    select(a, b);
    click(writing_, "Marcar Seleção");
    choose(type);
    click(host_, "Continuar");
  }
  Gtk::Widget &explorer() {
    auto &stack = pages(writing_, "semantic-layers");
    require(stack.get_visible_child_name() == "semantic-layers",
            "Explorer is not a dedicated page");
    return *stack.get_child_by_name("semantic-layers");
  }
  void open_annotation(const std::string &id) {
    auto *card = find<Gtk::Button>(explorer(), [&](auto &b) {
      return b.has_css_class("entity-card") &&
             b.get_tooltip_text().raw().find(id) != std::string::npos;
    });
    require(card, "No card for annotation: " + id);
    g_signal_emit_by_name(card->gobj(), "clicked");
  }
  std::size_t card_count() {
    return count(explorer(),
                 [](auto &w) { return w.has_css_class("entity-card"); });
  }
  Gtk::Widget &filters() {
    auto *expander = find<Gtk::Expander>(explorer(), [](auto &e) {
      return e.get_label() == "Pesquisa e filtros das camadas";
    });
    require(expander, "Filter expander absent");
    expander->set_expanded(true);
    return *expander->get_child();
  }
  void filter_checks() {
    click(writing_, "Marcações do texto");
    require(!find<Gtk::Button>(explorer(),
                               [](auto &b) {
                                 return b.get_label() == "Marcar Seleção" ||
                                        b.get_label() == "Busca semântica" ||
                                        b.get_label() ==
                                            "Pesquisar usos no Projeto";
                               }),
            "Actions were not removed from the explorer");
    require(card_count() == 3, "Explorer lost overlapping annotations");
    auto *role = find<Gtk::ComboBoxText>(filters(), [](auto &c) {
      return c.set_active_id("inde.semantic.thought");
    });
    require(role, "Role filter absent");
    click(explorer(), "Buscar camadas");
    require(card_count() == 1, "Role filter failed");
    open_annotation(annotated_.semantics.annotations[1].id);
    click(host_, "Voltar às camadas");
    require(card_count() == 1, "Detail return lost filters");
    click(explorer(), "Limpar filtros");
    auto *issues = find<Gtk::CheckButton>(filters(), [](auto &c) {
      return c.get_label() == "Somente com pendências";
    });
    require(issues, "Diagnostic filter absent");
    issues->set_active(true);
    click(explorer(), "Buscar camadas");
    require(card_count() == 1, "Diagnostic filtering failed");
    click(explorer(), "Limpar filtros");
    auto *search =
        find<Gtk::SearchEntry>(filters(), [](auto &) { return true; });
    require(search, "Layer search absent");
    search->set_text("ÁGUA");
    click(explorer(), "Buscar camadas");
    require(card_count() == 2, "Unicode casefold search failed");
    search = find<Gtk::SearchEntry>(filters(), [](auto &) { return true; });
    search->set_text("question");
    click(explorer(), "Buscar camadas");
    require(card_count() == 1, "Original enum key not searchable");
    search = find<Gtk::SearchEntry>(filters(), [](auto &) { return true; });
    search->set_text("PERGUNTA");
    click(explorer(), "Buscar camadas");
    require(card_count() == 1, "Translated enum label not searchable");
    search = find<Gtk::SearchEntry>(filters(), [](auto &) { return true; });
    search->set_text(entity_);
    click(explorer(), "Buscar camadas");
    require(card_count() == 1, "Canonical entity identity not searchable");
    search = find<Gtk::SearchEntry>(filters(), [](auto &) { return true; });
    search->set_text("not-a-semantic-layer-QA");
    click(explorer(), "Buscar camadas");
    require(card_count() == 0, "No-result search failed");
    require(find<Gtk::Label>(explorer(),
                             [](auto &l) {
                               return l.get_text() ==
                                      "Nenhuma camada corresponde aos filtros";
                             }),
            "No-result explanation absent");
    click(explorer(), "Limpar filtros");
    require(card_count() == 3, "Clear filters failed");
    select(7, 17);
    click(explorer(), "Voltar ao texto");
    auto selection =
        editor_->get_buffer()
            ->get_iter_at_mark(editor_->get_buffer()->get_selection_bound())
            .get_offset();
    require(selection == 17, "Explorer changed the editor selection");
  }
  bool step() {
    require(Clock::now() - started_ < std::chrono::seconds(35),
            "Semantic GTK timeout");
    auto buffer = editor_->get_buffer();
    if (phase_ == 0) {
      if (!get_mapped())
        return true;
      select(0, 4);
      click(writing_, "Marcações do texto");
      require(card_count() == 0, "Empty explorer unexpected cards");
      require(
          find<Gtk::Label>(explorer(),
                           [](auto &l) {
                             return l.get_text() ==
                                    "Este Documento ainda não tem marcações";
                           }),
          "Empty document unexplained");
      click(explorer(), "Voltar ao texto");
      begin("inde.semantic.narration", 0, 4);
      choose("action");
      click(host_, "Salvar anotação");
      annotated_ = saved();
      require(annotated_.semantics.annotations.size() == 1,
              "UI creation failed");
      annotation_ = annotated_.semantics.annotations.front().id;
      phase_ = 1;
    } else if (phase_ == 1) {
      begin("inde.semantic.thought", 0, 6);
      click(host_, "Salvar anotação");
      auto d = saved();
      require(d.semantics.annotations.size() == 2 &&
                  d.semantics.annotations.back().status == "incomplete",
              "Required field must stay incomplete");
      begin("inde.semantic.dialogue", 7, 17);
      auto *search = find<Gtk::SearchEntry>(host_, [](auto &s) {
        return s.get_placeholder_text() == "Pesquisar entidade para Falante";
      });
      require(search, "Explicit character picker missing");
      search->set_text("QA falante do kernel GTK");
      phase_ = 2;
    } else if (phase_ == 2) {
      auto *search = find<Gtk::SearchEntry>(host_, [](auto &s) {
        return s.get_placeholder_text() == "Pesquisar entidade para Falante";
      });
      require(search, "Speaker search disappeared");
      auto *picker =
          dynamic_cast<inde::ui::IncrementalSelector *>(search->get_parent());
      require(picker, "Speaker selector absent");
      auto *selected = find<Gtk::Button>(*picker, [](auto &b) {
        return find<Gtk::Label>(b, [](auto &l) {
                 return l.get_text() == "QA falante do kernel GTK";
               }) != nullptr;
      });
      if (!selected)
        return true;
      g_signal_emit_by_name(selected->gobj(), "clicked");
      require(picker->selected_id() == std::optional<std::string>(entity_),
              "Clicked speaker selector did not choose explicit canonical ID");
      click(host_, "Adicionar escolha — Falante");
      choose("spoken");
      auto *act = find<Gtk::CheckButton>(
          host_, [](auto &c) { return c.get_label() == "Pergunta"; });
      require(act, "Typed multiple enum missing");
      act->set_active(true);
      click(host_, "Salvar anotação");
      annotated_ = saved();
      require(annotated_.semantics.annotations.size() == 3,
              "Overlapping roles lost");
      auto &a = annotated_.semantics.annotations.back();
      require(a.fields.at("speaker").values ==
                  std::vector<std::string>{entity_},
              "Speaker binding not explicit");
      require(a.fields.at("acts").values ==
                  std::vector<std::string>{"question"},
              "Typed act incorrect");
      filter_checks();
      auto *lens = find<Gtk::ToggleButton>(writing_, [](auto &b) {
        return b.get_label() == "Realçar marcações";
      });
      require(lens, "No lenses");
      lens->set_active(true);
      auto lens_doc = saved();
      require(inde::project::same_document_state(annotated_, lens_doc),
              "Lens contaminated persistent formatting/history");
      require(!buffer->get_iter_at_offset(1).get_tags().empty(),
              "Lens not visible");
      for (auto &tag : buffer->get_iter_at_offset(1).get_tags()) {
        const std::string name = tag->property_name().get_value();
        if (name.starts_with("inde-semantic-")) {
          require(tag->property_background_set().get_value() &&
                      !tag->property_foreground_set().get_value(),
                  "Semantic lens must paint background but preserve glyph colour");
          const auto alpha = tag->property_background_rgba().get_value().get_alpha();
          require(alpha >= .20 && alpha <= .35,
                  "Semantic background too opaque or invisible");
        }
      }
      buffer->place_cursor(buffer->get_iter_at_offset(1));
      auto *tooltip = GTK_TOOLTIP(g_object_new(GTK_TYPE_TOOLTIP, nullptr));
      gboolean shown = false;
      g_signal_emit_by_name(editor_->gobj(), "query-tooltip", 0, 0, TRUE, tooltip, &shown);
      require(shown, "Resolved semantic range has no contextual tooltip");
      g_object_unref(tooltip);
      std::cout << "SNAPSHOT: lens" << std::endl;
      buffer->place_cursor(buffer->get_iter_at_offset(17));
      typing_ = Clock::now();
      phase_ = 21;
    } else if (phase_ == 21) {
      if (Clock::now() - typing_ < std::chrono::milliseconds(1200))
        return true;
      auto *lens = find<Gtk::ToggleButton>(writing_, [](auto &b) {
        return b.get_label() == "Realçar marcações";
      });
      lens->set_active(false);
      click(writing_, "⮐");
      auto undone = saved();
      require(undone.semantics.annotations.size() == 2 &&
                  undone.content == original_,
              "Semantic command not one undo step");
      click(writing_, "⮑");
      require(saved().semantics.annotations.size() == 3,
              "Redo omitted semantic metadata");
      buffer->insert(buffer->get_iter_at_offset(2), "X");
      typing_ = Clock::now();
      phase_ = 3;
    } else if (phase_ == 3) {
      if (Clock::now() - typing_ < std::chrono::milliseconds(850))
        return true;
      auto edited = saved();
      require(edited.content != original_, "Typing missing");
      require(edited.semantics.anchors.front().state ==
                  inde::project::AnchorResolution::Resolved,
              "Partial insertion orphaned annotation");
      click(writing_, "⮐");
      require(inde::project::same_document_state(annotated_, saved(), false),
              "Typing undo did not restore semantic evidence");
      click(writing_, "⮑");
      auto a = buffer->get_iter_at_offset(0), b = buffer->get_iter_at_offset(5);
      buffer->erase(a, b);
      typing_ = Clock::now();
      phase_ = 4;
    } else if (phase_ == 4) {
      if (Clock::now() - typing_ < std::chrono::milliseconds(850))
        return true;
      auto deleted = saved();
      require(deleted.semantics.anchors.front().state ==
                  inde::project::AnchorResolution::Orphaned,
              "Deleted range reassociated silently");
      require(deleted.semantics.annotations.front().id == annotation_,
              "Orphan UUID changed");
      select(0, 4);
      click(writing_, "Marcações do texto");
      open_annotation(annotation_);
      auto *notice = find<Gtk::Label>(host_, [](auto &l) {
        return l.get_text().raw().find("O trecho foi removido") !=
               std::string::npos;
      });
      require(notice, "Orphan unexplained");
      click(host_, "Reancorar na seleção atual");
      auto fixed = saved();
      require(fixed.semantics.anchors.front().state ==
                      inde::project::AnchorResolution::Resolved &&
                  fixed.semantics.annotations.front().id == annotation_,
              "Manual repair failed");
      click(explorer(), "Voltar ao texto");
      click(writing_, "⮐");
      require(saved().semantics.anchors.front().state ==
                  inde::project::AnchorResolution::Orphaned,
              "Repair not undoable");
      click(writing_, "⮑");
      auto before = saved();
      writing_.reset();
      writing_.refresh();
      writing_.reveal_document(id_);
      require(inde::project::same_document_state(before, saved()),
              "Reopen lost typed annotations");
      require(!find<Gtk::Button>(writing_, [](auto &b) { return b.get_label()=="Busca semântica"; }),
              "Redundant editor search button still exists");
      click(writing_, "Marcações do texto");
      auto *project_scope = find<Gtk::ComboBoxText>(explorer(), [](auto &c) { return c.get_active_id()=="document"; });
      require(project_scope, "Scope selector missing"); project_scope->set_active_id("project");
      while (Glib::MainContext::get_default()->iteration(false)) {}
      auto *uses = find<Gtk::Label>(host_, [](auto &l) {
        return l.get_text().raw().find("Consulta de anotações persistidas") !=
               std::string::npos;
      });
      require(uses, "Uses query UI missing");
      require(pages(writing_, "semantic-layers").get_visible_child_name() == "semantic-layers",
              "Semantic search still uses a separate modal");
      auto *scope = find<Gtk::ComboBoxText>(explorer(), [](auto &c) { return c.get_active_id() == "project"; });
      require(scope, "Merged explorer scope missing");
      scope->set_active_id("document");
      while (Glib::MainContext::get_default()->iteration(false)) {}
      click(explorer(), "Voltar ao texto");
      // More than one page, using only public domain/application services.
      auto pagination =
          service_.writing().create_document("QA paginação semântica");
      pagination.content = "Água 🙂 descrição sobreposta.";
      for (int i = 0; i < 29; ++i)
        pagination = inde::application::TextSemanticService::create(
            pagination, inde::project::builtin_annotation_types().front(), 0, 4,
            {});
      pagination = service_.writing().update_document(pagination);
      writing_.reveal_document(pagination.id);
      click(writing_, "Marcações do texto");
      require(card_count() == 24, "First page is not bounded to 24 cards");
      click(explorer(), "Próxima página de camadas");
      require(card_count() == 5, "Second page failed");
      open_annotation(pagination.semantics.annotations.back().id);
      click(host_, "Voltar às camadas");
      require(card_count() == 5, "Detail return reset page");
      click(explorer(), "Página anterior de camadas");
      require(card_count() == 24, "Previous page failed");
      writing_.reveal_document(id_);
      click(writing_, "Marcações do texto");
      require(card_count() == 3, "Document switch retained stale filters/page");
      std::cout << "SNAPSHOT: explorer" << std::endl;
      pagination_id_ = pagination.id;
      typing_ = Clock::now();
      phase_ = 41;
    } else if (phase_ == 41) {
      if (Clock::now() - typing_ < std::chrono::milliseconds(1200))
        return true;
      writing_.reset();
      service_.writing().delete_document(pagination_id_);
      service_.writing().delete_document(id_);
      service_.narrative().delete_entity(entity_);
      service_.close();
      result_ = 0;
      std::cout
          << "PASS: production GTK semantic creation, typed multiple fields, "
             "explicit entity choice, overlap, incomplete diagnostics, "
             "visual-only lenses, compound undo/redo, Unicode typing, orphan "
             "preservation, explained manual reanchor, reopen, direct toolbar "
             "actions, "
             "Unicode/role/diagnostic filters, empty states, 24-card "
             "pagination and uses\n"
          << std::flush;
      hide();
      return false;
    }
    return true;
  }
  inde::application::ProjectService service_;
  inde::ui::WritingWorkspace writing_;
  inde::ui::ModalOverlayHost host_;
  Gtk::TextView *editor_{};
  sigc::connection timer_;
  int &result_;
  int phase_{};
  Clock::time_point started_, typing_;
  std::string id_, entity_, annotation_, original_, pagination_id_;
  inde::project::Document annotated_;
};
} // namespace
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes")
    return 2;
  inde::ui::configure_rendering_backend();
  int result = 1;
  auto app = Gtk::Application::create("io.github.inde.semantic-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  SemanticWindow *window{};
  app->signal_activate().connect([&] {
    inde::ui::install_application_theme();
    window = new SemanticWindow(argv[1], result);
    app->add_window(*window);
    window->present();
  });
  app->run(1, argv);
  delete window;
  return result;
}
