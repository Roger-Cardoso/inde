// Opt-in process pair: producer is SIGKILLed by the isolated test runner.
#include "../src/ui/rendering_policy.hpp"
#include "inde/application/document_recovery_service.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
namespace {
using namespace inde;
using namespace inde::test::ui;
using Clock = std::chrono::steady_clock;
const std::string body = "Base Á🙂 com âncora";
const std::string unsaved = body + " — escrita NÃO salva no banco principal";
class RecoveryWindow final : public Gtk::ApplicationWindow {
public:
  RecoveryWindow(std::filesystem::path fixture, std::string mode, int &result)
      : writing_(service_), fixture_(std::move(fixture)),
        mode_(std::move(mode)), result_(result) {
    service_.open(fixture_);
    if (mode_ == "producer") {
      auto d = service_.writing().create_document("QA recuperação GTK");
      id_ = d.id;
      d.content = body;
      d.formatting = {{project::DocumentTextStyle::Bold, 0, 4}};
      anchor_ = project::new_uuid();
      d.anchors = {{anchor_, "Autoral", 1, 3}};
      const auto types = service_.narrative().entity_types();
      require(!types.empty(), "No entity types");
      entity_ = service_.narrative()
                    .create_entity(types[0].id, "QA recuperação entidade")
                    .id;
      binding_ = project::new_uuid();
      d.entity_references = {{binding_, entity_, anchor_, "Vínculo explícito"}};
      d.perspective = "Autoral — 🙂";
      d.word_goal = 123;
      d = service_.writing().update_document(d);
      incoming_ =
          service_.writing().create_document("QA referência de entrada").id;
      reference_ =
          service_.writing()
              .add_text_reference(incoming_, id_, anchor_, "Entrada preservada")
              .id;
      std::ofstream seed(fixture_ / "qa-recovery-seed.txt");
      seed << id_ << '\n'
           << anchor_ << '\n'
           << entity_ << '\n'
           << binding_ << '\n'
           << incoming_ << '\n'
           << reference_ << '\n';
      seed.close();
      require(sqlite3_open((fixture_ / "data/project.sqlite3").c_str(),
                           &lock_) == SQLITE_OK,
              "No lock connection");
    } else {
      std::ifstream seed(fixture_ / "qa-recovery-seed.txt");
      seed >> id_ >> anchor_ >> entity_ >> binding_ >> incoming_ >> reference_;
      require(seed.good() || seed.eof(), "No producer metadata");
      require(service_.writing().document(id_)->content == body,
              "Automatic mutation on reopen");
      const auto page = store_.pending(*service_.current());
      const auto own = std::find_if(page.items.begin(), page.items.end(),
                                    [&](const auto &r) { return r.document_id == id_; });
      require(own != page.items.end(), "Expected our inactive GTK session");
      require(std::count_if(page.items.begin(), page.items.end(),
                            [&](const auto &r) { return r.document_id == id_; }) == 1,
              "Expected exactly one inactive session for the test Document");
      session_ = own->session_id;
    }
    set_default_size(1180, 850);
    host_.set_content(writing_);
    set_child(host_);
    writing_.install_actions(*this);
    writing_.refresh();
    if (mode_ == "producer")
      writing_.reveal_document(id_);
    started_ = Clock::now();
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL recovery GTK: " << e.what() << std::endl;
            hide();
            return false;
          }
        },
        30);
  }
  ~RecoveryWindow() override {
    timer_.disconnect();
    if (lock_)
      sqlite3_close(lock_);
  }

private:
  bool label(const std::string &s) {
    return find<Gtk::Label>(host_, [&](auto &l) {
      return l.get_text().raw().find(s) != std::string::npos;
    });
  }
  Gtk::TextView &editor() {
    auto *v =
        find<Gtk::TextView>(writing_, [](auto &t) { return t.get_editable(); });
    require(v, "No editor");
    return *v;
  }
  void examine() {
    auto *button = find<Gtk::Button>(writing_, [](auto &b) {
      return b.get_label().raw().starts_with("Recuperação");
    });
    require(button, "No recovery button");
    g_signal_emit_by_name(button->gobj(), "clicked");
    require(host_.has_dialog(), "Recovery list not modal");
    const auto page = store_.pending(*service_.current());
    const auto own = std::find_if(page.items.begin(), page.items.end(),
                                  [&](const auto &r) { return r.session_id == session_; });
    require(own != page.items.end(), "Our checkpoint is absent from the list");
    const auto title = own->title + " — r" + std::to_string(own->revision) +
                       " — " + own->updated_at;
    auto *examine = find<Gtk::Button>(host_, [&](auto &b) {
      return b.get_label() == "Examinar ponto de recuperação" && b.get_parent() &&
             find<Gtk::Label>(*b.get_parent(), [&](auto &l) {
               return l.get_text().raw() == title;
             });
    });
    require(examine, "Our checkpoint row is absent");
    g_signal_emit_by_name(examine->gobj(), "clicked");
    require(host_.has_dialog() && label("Examinar ponto de recuperação de escrita"),
            "Preview absent");
  }
  void seed_checkpoint() {
    auto baseline = *service_.writing().document(id_);
    auto draft = baseline;
    draft.content += " checkpoint";
    session_ = project::new_uuid();
    project::RecoveryRecord r{service_.current()->manifest().project_id,
                              session_,
                              1,
                              0,
                              {draft, 7, 2},
                              baseline,
                              "pending",
                              {},
                              {}};
    {
      persistence::RecoveryLease lease(*service_.current(), session_);
      store_.checkpoint(*service_.current(), r);
    }
    writing_.refresh();
  }
  bool step() {
    require(Clock::now() - started_ < std::chrono::seconds(35),
            "Recovery GTK timeout");
    if (!get_mapped())
      return true;
    if (mode_ == "producer") {
      if (phase_ == 0) {
        require(sqlite3_exec(lock_, "BEGIN IMMEDIATE", nullptr, nullptr,
                             nullptr) == SQLITE_OK,
                "No main writer lock");
        auto buffer = editor().get_buffer();
        buffer->insert(buffer->end(),
                       " — escrita NÃO salva no banco principal");
        buffer->select_range(buffer->get_iter_at_offset(9),
                             buffer->get_iter_at_offset(2));
        phase_ = 1;
      } else if (phase_ == 1 && find<Gtk::Label>(writing_, [](auto &l) {
                   const auto text = l.get_text().raw();
                   return text.starts_with("Ponto de recuperação r") &&
                          text.ends_with(" confirmado");
                 })) {
        require(service_.writing().document(id_)->content == body,
                "Main DB was saved before interruption");
        const auto pending = store_.pending(*service_.current());
        require(std::none_of(pending.items.begin(), pending.items.end(),
                             [&](const auto &r) { return r.document_id == id_; }),
                "Our active session offered for recovery");
        std::cout << "READY: GTK checkpoint durable; main writer blocked; "
                     "active lease hidden\n"
                  << std::flush;
        phase_ = 2;
      }
      return true; // Runner interrupts this exact process with SIGKILL.
    }
    if (phase_ == 0) {
      require(label("aguardando sua decisão"),
              "No explanation on initial library");
      examine();
      require(service_.writing().document(id_)->content == body,
              "Preview mutated original");
      click(host_, "Cancelar");
      require(store_.load(*service_.current(), session_)->status == "pending",
              "Cancel consumed evidence");
      examine();
      click(host_, "Restaurar original");
      phase_ = 1;
    } else if (phase_ == 1) {
      auto restored = *service_.writing().document(id_);
      auto buffer = editor().get_buffer();
      require(restored.content == unsaved &&
                  buffer->get_text().raw() == unsaved,
              "GTK restore mismatch");
      require(buffer->get_iter_at_mark(buffer->get_insert()).get_offset() ==
                      9 &&
                  buffer->get_iter_at_mark(buffer->get_selection_bound())
                          .get_offset() == 2,
              "Directed Unicode selection mismatch");
      require(restored.formatting.size() == 1 &&
                  restored.anchors[0].id == anchor_ &&
                  restored.entity_references[0].id == binding_ &&
                  restored.perspective == "Autoral — 🙂" &&
                  restored.word_goal == 123,
              "Authorial state changed");
      require(service_.writing()
                      .text_references({id_, true, anchor_, 1, 0})
                      .items[0]
                      .reference.id == reference_,
              "Incoming identity lost");
      require(store_.load(*service_.current(), session_)->status == "restored",
              "Original decision not retained");
      std::cout << "CHECK: GTK inactive notice, preview/cancel nonmutating, "
                   "explicit restoration, "
                   "text/styles/selection/authorial/incoming identities\n"
                << std::flush;
      writing_.reset();
      seed_checkpoint();
      auto external = *service_.writing().document(id_);
      external.content += " EXTERNAL";
      static_cast<void>(service_.writing().update_document(external));
      examine();
      require(!find<Gtk::Button>(host_,
                                 [](auto &b) {
                                   return b.get_label() == "Restaurar original";
                                 }),
              "Conflict offered overwrite");
      click(host_, "Criar cópia de resgate");
      phase_ = 2;
    } else if (phase_ == 2) {
      const auto snapshot = *store_.load(*service_.current(), session_);
      require(snapshot.status == "copy-created",
              "Copy resolution not retained");
      require(service_.writing().document(id_)->content.ends_with(" EXTERNAL"),
              "Copy overwrote conflicting original");
      const auto docs = service_.writing().documents({});
      for (const auto &d : docs)
        if (d.title.starts_with("Resgate — QA recuperação GTK"))
          copy_ = d.id;
      require(!copy_.empty(), "No rescue in library");
      const auto copy = *service_.writing().document(copy_);
      require(copy.content == snapshot.state.document.content &&
                  copy.formatting.size() == 1 && copy.anchors.empty() &&
                  copy.entity_references.empty(),
              "Copy imported authorial bindings");
      writing_.reset();
      seed_checkpoint();
      examine();
      click(host_, "Descartar ponto de recuperação");
      require(host_.has_dialog() && label("Arquivar ponto de recuperação?"),
              "Discard skipped second confirmation");
      click(host_, "Cancelar");
      require(store_.load(*service_.current(), session_)->status == "pending",
              "Discard cancel consumed evidence");
      examine();
      click(host_, "Descartar ponto de recuperação");
      click(host_, "Confirmar descarte");
      phase_ = 3;
    } else if (phase_ == 3) {
      require(store_.load(*service_.current(), session_)->status ==
                      "discarded" &&
                  !store_.load(*service_.current(), session_)
                       ->state.document.content.empty(),
              "Discard deleted checkpoint payload");
      require(service_.writing().document(id_)->content.ends_with(" EXTERNAL"),
              "Discard mutated original");
      writing_.reset();
      service_.writing().delete_document(copy_);
      service_.writing().delete_document(incoming_);
      service_.writing().delete_document(id_);
      service_.narrative().delete_entity(entity_);
      service_.close();
      std::cout << "PASS: recovery GTK real process interruption, "
                   "conflict-safe explicit copy, discard/cancel/confirmation, "
                   "retained evidence and seed cleanup\n"
                << std::flush;
      result_ = 0;
      hide();
      return false;
    }
    return true;
  }
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  persistence::DocumentRecoveryStore store_;
  std::filesystem::path fixture_;
  std::string mode_, id_, anchor_, entity_, binding_, incoming_, reference_,
      session_, copy_;
  int &result_;
  int phase_{};
  sqlite3 *lock_{};
  sigc::connection timer_;
  Clock::time_point started_;
};
} // namespace
int main(int argc, char **argv) {
  const char *isolated = std::getenv("INDE_CLIPBOARD_TEST_ISOLATED");
  if (argc != 4 || std::string(argv[3]) != "--allow-fixture-writes" ||
      !isolated || std::string(isolated) != "1" ||
      (std::string(argv[2]) != "producer" &&
       std::string(argv[2]) != "consumer"))
    return 2;
  ui::configure_rendering_backend();
  int result = 1;
  auto app = Gtk::Application::create("io.github.inde.recovery-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  RecoveryWindow *window{};
  app->signal_activate().connect([&] {
    window = new RecoveryWindow(argv[1], argv[2], result);
    app->add_window(*window);
    window->present();
  });
  app->run(1, argv);
  delete window;
  return result;
}
