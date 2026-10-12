// Opt-in, production GTK objects, disposable fixtures, private Xvfb clipboard.
#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "ui/widget_probe.hpp"
#include <sqlite3.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
namespace {
using namespace inde::test::ui;
using Clock = std::chrono::steady_clock;
class ScaleWindow final : public Gtk::ApplicationWindow {
public:
  ScaleWindow(std::filesystem::path fixture, int &result) : writing_(service_), result_(result) {
    service_.open(fixture);
    auto a = service_.writing().create_document("QA escala origem"); id_ = a.id;
    a.content = "Aá🙂BCD"; original_ = a.content;
    a.formatting = {{inde::project::DocumentTextStyle::Bold, 0, 4}, {inde::project::DocumentTextStyle::Italic, 3, 6}};
    anchor_ = inde::project::new_uuid(); a.anchors = {{anchor_, "Autoral", 1, 4}};
    const auto types = service_.narrative().entity_types(); require(!types.empty(), "No entity types");
    entity_ = service_.narrative().create_entity(types[0].id, "QA escala entidade").id;
    a.entity_references = {{inde::project::new_uuid(), entity_, anchor_, "Não copiar vínculo"}};
    a = service_.writing().update_document(a);
    auto b = service_.writing().create_document("QA escala destino"); other_ = b.id;
    b.content = "xy"; b = service_.writing().update_document(b);
    set_default_size(1180, 850); host_.set_content(writing_); set_child(host_);
    writing_.install_actions(*this); writing_.refresh(); writing_.reveal_document(id_);
    editor_ = find<Gtk::TextView>(writing_, [](auto &v) { return v.get_editable(); }); require(editor_, "No editor");
    writing_.signal_status_message().connect([this](const std::string &text) { status_ = text; });
    require(sqlite3_open((fixture / "data/project.sqlite3").c_str(), &lock_) == SQLITE_OK, "No test lock connection");
    started_ = tick_ = Clock::now();
    timer_ = Glib::signal_timeout().connect([this] {
      try { return step(); } catch (const std::exception &e) {
        std::cerr << "FAIL scale: " << e.what() << std::endl; cleanup(); hide(); return false;
      }
    }, 30);
  }
  ~ScaleWindow() override { timer_.disconnect(); if (lock_) sqlite3_close(lock_); }
private:
  void cleanup() {
    sqlite3_exec(lock_, "ROLLBACK", nullptr, nullptr, nullptr);
    writing_.reset(); service_.writing().delete_document(other_); service_.writing().delete_document(id_);
    service_.narrative().delete_entity(entity_); service_.close();
  }
  bool label(const std::string &text) {
    return find<Gtk::Label>(writing_, [&](auto &l) { return l.get_text().raw().find(text) != std::string::npos; });
  }
  void select(int a, int b) { auto buffer = editor_->get_buffer(); buffer->select_range(buffer->get_iter_at_offset(b), buffer->get_iter_at_offset(a)); }
  void clipboard(const std::string &action) {
    auto *menu = find<Gtk::MenuButton>(writing_, [](auto &m) { return m.get_label() == "Copiar/Colar"; }); require(menu, "No clipboard menu");
    menu->popup(); auto *popover = menu->get_popover(); require(popover, "No clipboard popover");
    auto *button = find<Gtk::Button>(*popover, [&](auto &b) { return find<Gtk::Label>(b, [&](auto &l) { return l.get_text().raw() == action; }); });
    require(button, "No clipboard action: " + action); g_signal_emit_by_name(button->gobj(), "clicked"); menu->popdown();
  }
  bool step() {
    require(Clock::now() - started_ < std::chrono::seconds(40), "Scale timeout");
    auto buffer = editor_->get_buffer();
    if (phase_ == 0) {
      if (!get_mapped()) return true;
      select(1, 5); clipboard("Copiar com estilos");
      require(editor_->get_clipboard()->get_formats()->contain_mime_type("application/x-inde-text-fragment-v1+json"), "Styled MIME missing");
      writing_.reveal_document(other_); select(1, 1);
      g_signal_emit_by_name(editor_->gobj(), "paste-clipboard"); phase_ = 1;
    } else if (phase_ == 1) {
      if (buffer->get_text() == "xy") return true;
      require(buffer->get_text() == "xá🙂BCy", "Native paste must import only text"); writing_.flush_changes();
      auto d = service_.writing().document(other_); require(d->formatting.empty() && d->anchors.empty() && d->entity_references.empty(), "Plain paste imported origin layers/bindings");
      click(writing_, "⮐"); require(buffer->get_text() == "xy", "Plain paste undo not atomic");
      click(writing_, "⮑"); require(buffer->get_text() == "xá🙂BCy", "Paste redo broken"); click(writing_, "⮐");
      select(1, 1); clipboard("Colar com estilos"); phase_ = 2;
    } else if (phase_ == 2) {
      if (buffer->get_text() == "xy") return true;
      writing_.flush_changes(); auto d = service_.writing().document(other_);
      require(d->formatting.size() == 2 && d->anchors.empty() && d->entity_references.empty(), "Styled paste contract broken");
      select(1, 5); g_signal_emit_by_name(editor_->gobj(), "cut-clipboard");
      require(buffer->get_text() == "xy", "Native cut did not remove selection");
      require(!editor_->get_clipboard()->get_formats()->contain_mime_type("application/x-inde-text-fragment-v1+json"), "Default cut exports styles");
      click(writing_, "⮐"); require(buffer->get_text() == "xá🙂BCy", "Cut not one-step undo");
      writing_.flush_changes(); editor_->get_clipboard()->set_text("STALE"); select(1, 1);
      g_signal_emit_by_name(editor_->gobj(), "paste-clipboard"); select(0, 0); tick_ = Clock::now(); phase_ = 3;
    } else if (phase_ == 3) {
      if (Clock::now() - tick_ < std::chrono::milliseconds(200)) return true;
      require(buffer->get_text() == "xá🙂BCy" && status_.find("cancelada") != std::string::npos, "Moved-selection paste was not discarded/explained");
      select(1, 1); g_signal_emit_by_name(editor_->gobj(), "paste-clipboard"); writing_.reveal_document(id_); tick_ = Clock::now(); phase_ = 4;
    } else if (phase_ == 4) {
      if (Clock::now() - tick_ < std::chrono::milliseconds(200)) return true;
      require(buffer->get_text().raw() == original_, "Late paste reached another document");
      const auto source = service_.writing().document(id_); require(source->anchors[0].id == anchor_ && source->entity_references.size() == 1, "Origin references changed");
      require(sqlite3_exec(lock_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK, "Failed to hold SQLite writer");
      buffer->insert(buffer->end(), " one"); phase_ = 5;
      std::cout << "CHECK: private-display clipboard, native default paste/cut, styles opt-in, no imported bindings, undo/redo, selection/context cancellation\n" << std::flush;
    } else if (phase_ == 5) {
      if (!label("Salvamento em andamento")) return true;
      const auto start = Clock::now(); buffer->insert(buffer->end(), " two");
      require(Clock::now() - start < std::chrono::milliseconds(300), "Typing blocked on SQLite autosave");
      require(buffer->get_text().raw() == original_ + " one two", "Concurrent edit lost"); tick_ = Clock::now(); phase_ = 6;
    } else if (phase_ == 6) {
      if (Clock::now() - tick_ < std::chrono::milliseconds(1100)) return true;
      require(sqlite3_exec(lock_, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK, "Failed to release writer"); phase_ = 7;
    } else if (phase_ == 7) {
      if (!label("Salvo") || label("Salvamento em andamento")) return true;
      require(service_.writing().document(id_)->content == original_ + " one two", "Autosave lost latest queued text");
      buffer->insert(buffer->end(), " barrier"); writing_.reveal_document(other_);
      require(service_.writing().document(id_)->content == original_ + " one two barrier", "Switch did not flush pending edits");
      writing_.reveal_document(id_); auto external = *service_.writing().document(id_); external.content += " EXTERNAL"; external = service_.writing().update_document(external);
      buffer->insert(buffer->end(), " LOCAL"); phase_ = 8;
      std::cout << "CHECK: typing while DB writer blocked, newest pending snapshot persisted, synchronous navigation barrier\n" << std::flush;
    } else if (phase_ == 8) {
      if (!label("Falha ao salvar")) return true;
      require(buffer->get_text().raw().ends_with(" LOCAL"), "Conflict discarded local text");
      require(service_.writing().document(id_)->content.ends_with(" EXTERNAL"), "Conflict overwrote external text");
      bool caught{}; try { writing_.flush_changes(); } catch (const std::exception &) { caught = true; }
      require(caught && buffer->get_text().raw().ends_with(" LOCAL"), "Explicit failed-save barrier silently allowed close");
      std::cout << "PASS: editor scale GTK; external conflict nonmodal, local text retained, failed-close/save barrier\n" << std::flush;
      cleanup(); result_ = 0; hide(); return false;
    }
    return true;
  }
  inde::application::ProjectService service_; inde::ui::WritingWorkspace writing_; inde::ui::ModalOverlayHost host_;
  Gtk::TextView *editor_{}; sqlite3 *lock_{}; sigc::connection timer_;
  int &result_; int phase_{}; std::string id_, other_, entity_, anchor_, original_, status_; Clock::time_point started_, tick_;
};
}
int main(int argc, char **argv) {
  // This executable must never claim ownership of the user's clipboard.
  const char *isolated = std::getenv("INDE_CLIPBOARD_TEST_ISOLATED");
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes" || !isolated || std::string(isolated) != "1") return 2;
  inde::ui::configure_rendering_backend(); int result = 1;
  auto app = Gtk::Application::create("io.github.inde.scale-smoke", Gio::Application::Flags::NON_UNIQUE);
  ScaleWindow *window{};
  app->signal_activate().connect([&] { window = new ScaleWindow(argv[1], result); app->add_window(*window); window->present(); });
  app->run(1, argv); delete window; return result;
}
