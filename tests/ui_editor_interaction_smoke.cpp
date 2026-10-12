#include "../src/ui/rendering_policy.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <chrono>
#include <iostream>
#include <gdk/x11/gdkx.h>
namespace {
using namespace inde;
using namespace inde::test::ui;
class InteractionWindow final : public Gtk::ApplicationWindow {
public:
  InteractionWindow(const std::filesystem::path &path, int &result)
      : writing_(service_), result_(result) {
    service_.open(path);
    old_count_ = service_.narrative().entity_count();
    auto d =
        service_.writing().create_document("QA interação direta do editor");
    d.content = "Água 🙂 Marcio veio a falecer. Uma informação autoral.";
    d = service_.writing().update_document(d);
    id_ = d.id;
    baseline_ = d;
    for (const auto &t : service_.narrative().entity_types()) {
      if (t.key == "character")
        character_ = t.id;
      if (t.key == "information")
        info_type_ = t.id;
    }
    host_.set_content(writing_);
    set_child(host_);
    set_default_size(1280, 900);
    add_css_class("inde-window");
    writing_.install_actions(*this);
    writing_.refresh();
    writing_.reveal_document(id_);
    editor_ =
        find<Gtk::TextView>(writing_, [](auto &v) { return v.get_editable(); });
    require(editor_, "No editor");
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL interaction phase=" << phase_ << ": " << e.what()
                      << std::endl;
            hide();
            return false;
          }
        },
        120);
  }
  ~InteractionWindow() override { timer_.disconnect(); }

private:
  project::Document saved() {
    writing_.flush_changes();
    return *service_.writing().document(id_);
  }
  void select(int a, int b) {
    auto buffer = editor_->get_buffer();
    buffer->select_range(buffer->get_iter_at_offset(b),
                         buffer->get_iter_at_offset(a));
    editor_->grab_focus();
  }
  void action(const char *name) {
    require(
        gtk_widget_activate_action(GTK_WIDGET(editor_->gobj()), name, nullptr),
        std::string("No action ") + name);
  }
  void entry(const std::string &placeholder, const std::string &value) {
    expand_options();
    auto *e = find<Gtk::Entry>(host_, [&](auto &e) {
      return GTK_IS_ENTRY(e.gobj()) && e.get_placeholder_text().raw() == placeholder;
    });
    require(e, "No entry " + placeholder);
    e->set_text(value);
  }
  void tick(const std::string &label) {
    expand_options();
    auto *b = find<Gtk::CheckButton>(
        host_, [&](auto &b) { return b.get_label().raw() == label; });
    require(b, "No checkbox " + label);
    b->set_active(true);
  }
  void expand_options() {
    if (auto *options = find<Gtk::Expander>(host_, [](auto &e) {
      return e.get_label().raw().starts_with("Opções da ");
    })) options->set_expanded(true);
  }
  void choose(const std::string &id) {
    auto *c = find<Gtk::ComboBoxText>(host_, [&](auto &c) {
      if(combo_has_id(c,id)) return true;
      return false;
    });
    require(c, "No combo choice " + id);
    require(c->set_active_id(id), "Choice failed");
  }
  void pick(const std::string &placeholder,
            const project::NarrativeEntity &entity) {
    auto *s = find<ui::IncrementalSelector>(host_, [&](auto &s) {
      return find<Gtk::SearchEntry>(s, [&](auto &e) {
        return e.get_placeholder_text().raw() == placeholder;
      });
    });
    require(s, "No picker " + placeholder);
    s->set_selected(entity.id, entity.name);
  }
  void snapshot(const std::string &name) {
    std::cout << "SNAPSHOT: " << name << std::endl;
    snapshot_time_ = std::chrono::steady_clock::now();
  }
  bool step() {
    require(std::chrono::steady_clock::now() - started_ <
                std::chrono::seconds(90),
            "Timeout");
    if (phase_ == 0) {
      if (!get_mapped())
        return true;
      require(find<Gtk::Popover>(writing_, [](auto &p) {
        return p.has_css_class("context-menu") && find<Gtk::Label>(p, [](auto &l) {
          return l.get_text() == "Adicionar elemento da história à seleção…";
        });
      }), "Localized editor context menu missing");
      select(7, 13);
      auto iter=editor_->get_buffer()->get_iter_at_offset(8);
      Gdk::Rectangle rect;editor_->get_iter_location(iter,rect);int x{},y{};
      editor_->buffer_to_window_coords(Gtk::TextWindowType::WIDGET,rect.get_x(),rect.get_y(),x,y);
      graphene_point_t origin{static_cast<float>(x+2),static_cast<float>(y+5)},point{};
      require(gtk_widget_compute_point(GTK_WIDGET(editor_->gobj()),GTK_WIDGET(gobj()),&origin,&point),"No editor coordinates");
      const auto xid=std::to_string(gdk_x11_surface_get_xid(get_surface()->gobj()));
      Glib::spawn_async({}, {"xdotool","windowfocus","--sync",xid,"mousemove","--window",xid,std::to_string(static_cast<int>(point.x)),std::to_string(static_cast<int>(point.y)),"click","3"},Glib::SpawnFlags::SEARCH_PATH);
      snapshot_time_=std::chrono::steady_clock::now();phase_=20;return true;
    }
    if(phase_==20) {
      auto *popover=find<Gtk::Widget>(writing_,[](auto &p){return GTK_IS_POPOVER(p.gobj()) && p.has_css_class("context-menu") && p.get_mapped();});
      if(!popover) {
        if(std::chrono::steady_clock::now()-snapshot_time_>std::chrono::seconds(3) && !native_debug_) {
          native_debug_=true;std::cout<<"SNAPSHOT: native-context"<<std::endl;
          const auto xid=std::to_string(gdk_x11_surface_get_xid(get_surface()->gobj()));
          Glib::spawn_async({}, {"xdotool","key","--window",xid,"shift+F10"},Glib::SpawnFlags::SEARCH_PATH);
          std::cout<<"NATIVE_DIAGNOSTIC: mouse context not detected; trying keyboard equivalent"<<std::endl;
        }
        return true;
      }
      require(find<Gtk::Label>(*popover,[](auto &l){return l.get_text().raw()=="Adicionar elemento da história à seleção…";}),"Native context item missing");
      require(editor_->get_buffer()->get_iter_at_mark(editor_->get_buffer()->get_insert()).get_offset()==13 &&
        editor_->get_buffer()->get_iter_at_mark(editor_->get_buffer()->get_selection_bound()).get_offset()==7,"Context menu lost selection");
      snapshot("native-context");phase_=21;return true;
    }
    if(phase_==21) {
      if(std::chrono::steady_clock::now()-snapshot_time_<std::chrono::milliseconds(650))return true;
      auto *popover=find<Gtk::Widget>(writing_,[](auto &p){return GTK_IS_POPOVER(p.gobj()) && p.has_css_class("context-menu") && p.get_mapped();});require(popover,"Localized popup gone");gtk_popover_popdown(GTK_POPOVER(popover->gobj()));
      select(7,13);action("inde-editor.create-entity");
      click(host_, "Cancelar");
      require(project::same_document_state(saved(), baseline_) &&
                  service_.narrative().entity_count() == old_count_,
              "Cancel created data");
      select(7, 13);
      action("inde-editor.create-entity");
      choose(character_);
      entry("Resumo / informação do Projeto",
            "Médico de Véspera — informação canônica");
      snapshot("entity-create");
      phase_ = 1;
      return true;
    }
    if (phase_ == 1) {
      if (std::chrono::steady_clock::now() - snapshot_time_ <
          std::chrono::milliseconds(650))
        return true;
      click(host_, "Criar elemento da história e vincular");
      auto d = saved();
      require(d.content == baseline_.content &&
                  d.semantics.annotations.size() == 1,
              "Create didn't preserve text/binding");
      auto id =
          d.semantics.annotations.front().fields.at("target").values.front();
      marcio_ = *service_.narrative().entity(id);
      require(marcio_.name == "Marcio" &&
                  service_.narrative().entity_count() == old_count_ + 1,
              "Wrong canonical entity");
      action("inde-editor.undo");
      require(saved().semantics.annotations.empty() &&
                  service_.narrative().entity(marcio_.id).has_value(),
              "Undo erased entity or kept binding");
      action("inde-editor.redo");
      require(saved().semantics.annotations.size() == 1, "Redo lost binding");
      select(0, 4);
      action("inde-editor.create-entity");
      tick("Criar um novo tipo de entidade nesta operação");
      entry("Nome do elemento da história", "Ordem da Aurora QA");
      entry("Nome do novo tipo", "Organização QA");
      entry("Chave do novo tipo (ex.: organizacao)",
            "qa-organizacao-" + project::new_uuid());
      click(host_, "Criar elemento da história e vincular");
      d = saved();
      require(d.semantics.annotations.size() == 2, "New type didn't bind");
      other_ = *service_.narrative().entity(
          d.semantics.annotations.back().fields.at("target").values.front());
      select(7, 13);
      action("inde-editor.create-relation");
      choose(marcio_.id);
      pick("Pesquisar outro elemento da relação", other_);
      tick("Criar um novo tipo de relação nesta operação");
      entry("Nome do novo tipo de relação", "protege");
      entry("Chave do tipo de relação (ex.: protege)",
            "qa-protege-" + project::new_uuid());
      entry("Nome no sentido inverso (ex.: é protegido por)",
            "é protegido por");
      choose("incoming");
      require(find<Gtk::Label>(
                  host_,
                  [](auto &l) {
                    return l.get_text().raw().starts_with(
                        "Prévia: Ordem da Aurora QA → protege → Marcio");
                  }),
              "Direction preview missing");
      snapshot("relation-preview");
      phase_ = 2;
      return true;
    }
    if (phase_ == 2) {
      if (std::chrono::steady_clock::now() - snapshot_time_ <
          std::chrono::milliseconds(650))
        return true;
      click(host_, "Criar relação no Projeto");
      persistence::RelationQuery q;
      q.entity_id = marcio_.id;
      auto relations = service_.narrative().relations(q);
      require(relations.size() == 1 &&
                  relations.front().source_entity_id == other_.id,
              "Wrong direction");
      info_ = service_.narrative().create_entity(
          info_type_, "Causa incerta QA",
          "A causa da morte ainda é desconhecida.");
      select(14, 28);
      action("inde-editor.materialize");
      pick("Pesquisar elemento da história ou informação do Projeto", info_);
      choose("evidence");
      entry(
          "Nota da associação (opcional)",
          "A frase é evidência textual, não fato automaticamente confirmado.");
      snapshot("materialization");
      phase_ = 3;
      return true;
    }
    if (phase_ == 3) {
      if (std::chrono::steady_clock::now() - snapshot_time_ <
          std::chrono::milliseconds(650))
        return true;
      click(host_, "Associar trecho");
      auto d = saved();
      require(d.semantics.annotations.size() == 3 &&
                  d.content == baseline_.content,
              "Materialization corrupt");
      persistence::DocumentQuery q;
      q.entity_id = info_.id;
      require(service_.writing().document_count(q) == 1, "Projection missing");
      // Main editor's Entidades panel also includes semantic associations.
      click(writing_, "Elementos da história");
      require(
          find<Gtk::Button>(
              writing_,
              [](auto &b) {
                return b.get_label().raw().find(
                           "Materialização associativa (camada semântica)") !=
                       std::string::npos;
              }),
          "Semantic entity invisible in panel");
      click(writing_, "Elementos da história");
      select(0, project::utf8_character_count(d.content));
      action("inde-editor.distribution");
      require(find<Gtk::Label>(host_,
                               [](auto &l) {
                                 return l.get_text().raw().starts_with(
                                     "Ocorrência de entidade —");
                               }),
              "Distribution missing");
      snapshot("distribution");
      phase_ = 4;
      return true;
    }
    if (phase_ == 4) {
      if (std::chrono::steady_clock::now() - snapshot_time_ <
          std::chrono::milliseconds(650))
        return true;
      click(host_, "Fechar");
      // No selection: don't invent a span; context menu gives an explanation.
      editor_->get_buffer()->place_cursor(editor_->get_buffer()->end());
      action("inde-editor.create-entity");
      require(find<Gtk::Label>(host_,
                               [](auto &l) {
                                 return l.get_text().raw().find(
                                            "Selecione um trecho não vazio") !=
                                        std::string::npos;
                               }),
              "No selection error missing");
      click(host_, "Fechar");
      select(7, 13);
      action("inde-editor.create-entity");
      choose(character_);
      auto buffer = editor_->get_buffer();
      buffer->insert(buffer->end(), " Alteração com modal aberto.");
      const auto count = service_.narrative().entity_count();
      click(host_, "Criar elemento da história e vincular");
      expected_count_ = count;
      phase_ = 5;
      return true;
    }
    if (phase_ == 5) {
      if (!host_.has_dialog())
        return true;
      require(find<Gtk::Label>(host_,
                               [](auto &l) {
                                 return l.get_text().raw().find(
                                            "Documento ou Projeto mudou") !=
                                        std::string::npos;
                               }),
              "Stale guard missing");
      require(service_.narrative().entity_count() == expected_count_,
              "Stale creation wrote entity");
      click(host_, "Fechar");
      saved();
      auto buffer = editor_->get_buffer();
      buffer->place_cursor(buffer->end());
      editor_->grab_focus();
      buffer->insert_at_cursor(" @informacao:Causa");
      phase_ = 6;
      return true;
    }
    if (phase_ == 6) {
      auto *button = find<Gtk::Button>(writing_, [](auto &b) {
        return b.get_label().raw() == "Causa incerta QA — Informação (Projeto)";
      });
      if (!button)
        return true;
      g_signal_emit_by_name(button->gobj(), "clicked");
      choose("summary-link");
      snapshot("inline-preview");
      phase_ = 7;
      return true;
    }
    if (phase_ == 7) {
      if (std::chrono::steady_clock::now() - snapshot_time_ <
          std::chrono::milliseconds(650))
        return true;
      click(host_, "Inserir escolha");
      auto d = saved();
      require(d.content.ends_with("A causa da morte ainda é desconhecida.") &&
                  d.semantics.annotations.size() == 4,
              "Inline summary association missing");
      action("inde-editor.undo");
      d = saved();
      require(d.content.ends_with("@informacao:Causa") &&
                  d.semantics.annotations.size() == 3,
              "Inline undo split operation");
      action("inde-editor.redo");
      d = saved();
      require(d.semantics.annotations.size() == 4, "Inline redo missing");
      writing_.reset();
      writing_.refresh();
      writing_.reveal_document(id_);
      require(project::same_document_state(saved(), d),
              "Reopen changed content");
      require(service_.narrative().entity_count() == old_count_ + 3,
              "Unexpected canonical entities");
      std::cout << "CREATED_DOCUMENT_ID=" << id_
                << "\nPASS: GTK context "
                   "create/type/cancel/undo/relation/materialization/"
                   "distribution/stale/inline/reopen\n";
      result_ = 0;
      hide();
      return false;
    }
    return true;
  }
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  int &result_;
  Gtk::TextView *editor_{};
  project::Document baseline_;
  project::NarrativeEntity marcio_, other_, info_;
  std::string id_, character_, info_type_;
  std::size_t old_count_{}, expected_count_{};
  int phase_{};
  bool native_debug_{};
  sigc::connection timer_;
  std::chrono::steady_clock::time_point started_{
      std::chrono::steady_clock::now()},
      snapshot_time_{};
};
} // namespace
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes" ||
      !std::getenv("INDE_CLIPBOARD_TEST_ISOLATED"))
    return 2;
  inde::ui::configure_rendering_backend();
  int result = 1;
  InteractionWindow *window{};
  auto app = Gtk::Application::create("io.github.inde.editor.interaction.qa",
                                      Gio::Application::Flags::NON_UNIQUE);
  app->signal_activate().connect([&] {
    inde::ui::install_application_theme();
    window = new InteractionWindow(argv[1], result);
    app->add_window(*window);
    window->present();
  });
  app->run(1, argv);
  delete window;
  return result;
}
