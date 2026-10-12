// Uses production UI objects and explicitly disposable project copies only.
#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "ui/widget_probe.hpp"
#include <chrono>
#include <iostream>
#include <gdk/x11/gdkx.h>

namespace {
using namespace inde::test::ui;
using Clock = std::chrono::steady_clock;
class EditingWindow final : public Gtk::ApplicationWindow {
public:
  EditingWindow(std::filesystem::path fixture, int &result) : writing_(service_), result_(result), fixture_(std::move(fixture)) {
    service_.open(fixture_);
    auto a = service_.writing().create_document("QA grande corte edição");
    id_ = a.id;
    a.content = "Água água AGUA águaX\n🙂 água fim.";
    original_ = a.content;
    anchor_ = inde::project::new_uuid();
    a.formatting = {{inde::project::DocumentTextStyle::Bold, 0, 4},
                    {inde::project::DocumentTextStyle::Italic, 5, 9}};
    a.anchors = {{anchor_, "Primeira água", 0, 4}};
    const auto types = service_.narrative().entity_types();
    require(!types.empty(), "Fixture without entity types");
    entity_ = service_.narrative().create_entity(types.front().id, "QA referência do grande corte").id;
    entity_reference_ = inde::project::new_uuid();
    a.entity_references = {{entity_reference_, entity_, anchor_, "Referência autoral preservada"}};
    a.perspective = "Perspectiva autoral";
    a = service_.writing().update_document(a);
    auto b = service_.writing().create_document("QA grande corte troca");
    other_ = b.id; b.content = "Outro Documento sem ocorrência."; b = service_.writing().update_document(b);
    text_reference_ = service_.writing().add_text_reference(other_, id_, anchor_, "Uso externo da âncora").id;
    set_title("INDE QA grande corte"); set_default_size(1180, 850);
    host_.set_content(writing_); set_child(host_);
    writing_.install_actions(*this); writing_.refresh(); writing_.reveal_document(id_);
    editor_ = find<Gtk::TextView>(writing_, [](auto &v) { return v.get_editable(); });
    query_ = find<Gtk::Entry>(writing_, [](auto &e) { return GTK_IS_ENTRY(e.gobj()) && e.get_placeholder_text() == "Localizar no Documento"; });
    replacement_ = find<Gtk::Entry>(writing_, [](auto &e) { return GTK_IS_ENTRY(e.gobj()) && e.get_placeholder_text() == "Substituir por (vazio apaga)"; });
    require(editor_ && query_ && replacement_, "Missing editor/search widgets");
    started_ = Clock::now();
    timer_ = Glib::signal_timeout().connect([this] {
      try { return step(); }
      catch (const std::exception &e) { std::cerr << "FAIL editing: " << e.what() << std::endl; cleanup(); hide(); return false; }
    }, 30);
  }
  ~EditingWindow() override { timer_.disconnect(); }
private:
  void cleanup() {
    writing_.reset();
    service_.writing().delete_document(other_); // Remove outgoing anchor use before target.
    service_.writing().delete_document(id_);
    service_.narrative().delete_entity(entity_);
    service_.close();
  }
  bool results(const std::string &suffix) {
    return find<Gtk::Label>(writing_, [&](auto &l) { return l.get_text().raw().find(suffix) != std::string::npos; });
  }
  void key(const std::string &shortcut) {
    auto surface = get_surface(); require(GDK_IS_X11_SURFACE(surface->gobj()), "X11 keyboard test required");
    const auto xid = std::to_string(gdk_x11_surface_get_xid(surface->gobj()));
    Glib::spawn_async({}, {"xdotool", "windowfocus", "--sync", xid, "key", "--window", xid, shortcut}, Glib::SpawnFlags::SEARCH_PATH);
  }
  bool step() {
    require(Clock::now() - started_ < std::chrono::seconds(35), "Search GUI timeout");
    auto buffer = editor_->get_buffer();
    if (phase_ == 0) {
      if (!get_mapped()) return true;
      editor_->grab_focus(); key("ctrl+h"); phase_ = 10;
    } else if (phase_ == 10) {
      if (!replacement_->get_mapped()) return true;
      query_->set_text("água"); replacement_->set_text("rio");
      auto *whole = find<Gtk::CheckButton>(writing_, [](auto &b) { return b.get_label() == "Palavra inteira"; });
      require(whole, "Whole-word option missing"); whole->set_active(true);
      phase_ = 1;
    } else if (phase_ == 1) {
      if (!results("de 3 ocorrências")) return true;
      if (keyboard_phase_ == 0) {
        keyboard_cursor_ = buffer->get_iter_at_mark(buffer->get_insert()).get_offset();
        query_->grab_focus(); key("F3"); keyboard_phase_ = 1; return true;
      }
      if (keyboard_phase_ == 1) {
        if (buffer->get_iter_at_mark(buffer->get_insert()).get_offset() == keyboard_cursor_) return true;
        key("shift+F3"); keyboard_phase_ = 2; return true;
      }
      if (keyboard_phase_ == 2 && buffer->get_iter_at_mark(buffer->get_insert()).get_offset() != keyboard_cursor_) return true;
      click(writing_, "Próximo"); click(writing_, "Anterior");
      const auto cursor = buffer->get_iter_at_mark(buffer->get_insert()).get_offset();
      const auto bound = buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset();
      click(writing_, "Substituir tudo");
      require(buffer->get_text() == "rio rio AGUA águaX\n🙂 rio fim.", "Case/whole-word replacement incorrect");
      click(writing_, "⮐");
      require(buffer->get_text().raw() == original_, "Replace-all must undo in one step");
      require(buffer->get_iter_at_mark(buffer->get_insert()).get_offset() == cursor &&
              buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset() == bound, "Undo lost directed selection");
      click(writing_, "⮑"); writing_.flush_changes();
      const auto saved = service_.writing().document(id_);
      require(saved->anchors[0].id == anchor_ && saved->anchors[0].end_offset == 3, "Anchor identity/geometry changed");
      require(saved->formatting.size() == 2 && saved->formatting[0].end_offset == 3 && saved->formatting[1].start_offset == 4, "Formatting replacement lost");
      require(saved->perspective == "Perspectiva autoral", "Metadata lost");
      require(saved->entity_references.size() == 1 && saved->entity_references[0].id == entity_reference_ &&
              saved->entity_references[0].entity_id == entity_ && saved->entity_references[0].anchor_id == anchor_ &&
              saved->entity_references[0].notes == "Referência autoral preservada", "Explicit entity reference changed");
      const auto uses = service_.writing().text_references({id_, true, anchor_, 1, 0});
      require(uses.total == 1 && uses.items[0].reference.id == text_reference_, "Incoming cross-document reference changed");
      writing_.reset(); service_.close(); service_.open(fixture_); writing_.refresh(); writing_.reveal_document(id_);
      require(buffer->get_text().raw() == saved->content, "Reopen lost replacement");
      activate_action("win.writing-replace"); query_->set_text("rio"); replacement_->set_text(""); phase_ = 2;
      std::cout << "CHECK: Unicode, navigation, compound undo/redo, selection, layers, metadata, persistence\n" << std::flush;
    } else if (phase_ == 2) {
      if (!results("de 3 ocorrências")) return true;
      const auto before = buffer->get_text(); click(writing_, "Substituir");
      require(buffer->get_text() != before, "Replace one did not delete"); click(writing_, "⮐");
      require(buffer->get_text() == before, "Single replacement undo failed");
      replacement_->set_text("rio"); phase_ = 3;
    } else if (phase_ == 3) {
      if (!results("de 3 ocorrências")) return true;
      click(writing_, "Substituir tudo");
      require(results("não altera"), "No-op replacement not explained");
      auto *redo = find<Gtk::Button>(writing_, [](auto &b) { return b.get_label() == "⮑"; });
      require(redo && redo->get_sensitive(), "No-op destroyed redo history");
      click(writing_, "⮑"); click(writing_, "⮐");
      buffer->insert(buffer->end(), " digitado");
      require(!redo->get_sensitive(), "Typing did not invalidate redo immediately");
      query_->set_text("rio"); replacement_->set_text("mar"); phase_ = 4;
    } else if (phase_ == 4) {
      if (!results("de 3 ocorrências")) return true;
      click(writing_, "Substituir tudo"); click(writing_, "⮐");
      require(buffer->get_text().raw().ends_with(" digitado"), "Compound operation merged preceding typing");
      click(writing_, "⮐"); require(!buffer->get_text().raw().ends_with(" digitado"), "Preceding typing not separate");
      query_->set_text("rio"); writing_.reveal_document(other_); phase_ = 5;
    } else if (phase_ == 5) {
      if (!results("Nenhuma ocorrência")) return true;
      require(buffer->get_text() == "Outro Documento sem ocorrência.", "Stale result affected another document");
      query_->grab_focus(); key("Escape"); phase_ = 6;
    } else if (phase_ == 6) {
      if (query_->get_mapped()) return true;
      key("ctrl+f"); phase_ = 7;
    } else if (phase_ == 7) {
      if (!query_->get_mapped()) return true;
      require(!replacement_->get_mapped(), "Ctrl+F did not hide replacement row");
      query_->set_text("Outro"); writing_.reset(); cleanup(); result_ = 0;
      std::cout << "PASS: editing, no-op, immediate redo invalidation, typing isolation, stale switch/reset\n" << std::flush;
      std::cout << "PASS: X11 shortcuts Ctrl+H, F3, Shift+F3, Escape, Ctrl+F on own test window\n" << std::flush;
      hide(); return false;
    }
    return true;
  }
  inde::application::ProjectService service_;
  inde::ui::WritingWorkspace writing_;
  inde::ui::ModalOverlayHost host_;
  Gtk::TextView *editor_{}; Gtk::Entry *query_{}, *replacement_{};
  sigc::connection timer_; int &result_; int phase_{}, keyboard_phase_{}, keyboard_cursor_{};
  std::filesystem::path fixture_; Clock::time_point started_;
  std::string id_, other_, anchor_, original_, entity_, entity_reference_, text_reference_;
};
}
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes") return 2;
  inde::ui::configure_rendering_backend(); int result = 1;
  auto app = Gtk::Application::create("io.github.inde.editing-smoke", Gio::Application::Flags::NON_UNIQUE);
  EditingWindow *window{};
  app->signal_activate().connect([&] { window = new EditingWindow(argv[1], result); app->add_window(*window); window->present(); });
  app->run(1, argv); delete window; return result;
}
