// Real production GTK objects; only explicitly disposable fixtures may be used.
#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/proofreading_controller.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "ui/widget_probe.hpp"

#include <chrono>
#include <iostream>

namespace {
using namespace inde::test::ui;
using Clock = std::chrono::steady_clock;
class WritingWindow final : public Gtk::Window {
public:
  WritingWindow(std::filesystem::path fixture, int &result)
      : writing_(service_), proofreading_(store_, spelling_),
        controller_(proofreading_), result_(result), fixture_(std::move(fixture)) {
    service_.open(fixture_);
    auto a = service_.writing().create_document("QA temporário E1.1 longo");
    a_id_ = a.id;
    long_text_ = "Caza  casa.\n";
    while (long_text_.size() < 1024 * 1024)
      long_text_ += "A escritora observa a cidade e reescreve a cena com atenção. "
                    "Cada parágrafo preserva contexto e ritmo.\n";
    a.content = long_text_;
    expected_words_ = inde::project::analyze_document_text(long_text_).words;
    a.formatting = {{inde::project::DocumentTextStyle::Bold, 11, 21}};
    a.anchors = {{inde::project::new_uuid(), "Âncora QA", 11, 21}};
    a = service_.writing().update_document(a);
    anchor_id_ = a.anchors.front().id;
    auto b = service_.writing().create_document("QA temporário E1.1 curto");
    b_id_ = b.id;
    b.content = "Outro Documento.";
    b = service_.writing().update_document(b);
    set_default_size(1180, 850);
    host_.set_content(writing_);
    set_child(host_);
    controller_.watch(writing_);
    writing_.refresh();
    writing_.reveal_document(a_id_);
    editor_ = find<Gtk::TextView>(writing_, [](auto &v) { return v.get_editable(); });
    require(editor_, "Editable TextView missing");
    start_ = phase_started_ = Clock::now();
    timer_ = Glib::signal_timeout().connect([this] {
      try { return step(); }
      catch (const std::exception &error) {
        std::cerr << "FAIL writing: " << error.what() << std::endl;
        cleanup(); hide(); return false;
      }
    }, 25);
  }
  ~WritingWindow() override { timer_.disconnect(); }
private:
  bool word_count(std::size_t expected) {
    return find<Gtk::Label>(writing_, [&](auto &l) {
      return l.get_text().raw() == std::to_string(expected) + " palavras";
    });
  }
  void next(int phase) { phase_ = phase; phase_started_ = Clock::now(); }
  void cleanup() {
    writing_.reset();
    service_.writing().delete_document(a_id_);
    service_.writing().delete_document(b_id_);
    service_.close();
  }
  bool step() {
    require(Clock::now() - start_ < std::chrono::seconds(65), "GUI test timeout");
    const auto buffer = editor_->get_buffer();
    if (phase_ == 0) {
      if (!word_count(expected_words_)) return true;
      auto tag = buffer->get_tag_table()->lookup("inde-grammar-error");
      if (!tag || !buffer->get_iter_at_offset(4).has_tag(tag)) return true;
      std::cout << "CHECK: asynchronous metrics and proofreading, 1 MiB" << std::endl;
      next(1);
    } else if (phase_ == 1) {
      // Rapid burst: verify immediate invalidation, then latest result only.
      const auto begin = Clock::now();
      buffer->insert(buffer->end(), " Ação 👩‍💻 café");
      std::cout << "GUI_INSERT elapsed_ms="
                << std::chrono::duration<double, std::milli>(Clock::now() - begin).count()
                << std::endl;
      require(!buffer->get_iter_at_offset(4).has_tag(
                  buffer->get_tag_table()->lookup("inde-grammar-error")),
              "Stale proofreading tag remained after edit");
      if (burst_ == 0) {
        controller_.show_current_issue();
        require(find<Gtk::Label>(writing_, [](auto &label) {
          return label.get_text().raw().find("Revisão de texto em andamento") != std::string::npos;
        }), "F7 falsely reported a completed review while analysis was pending");
      }
      if (++burst_ == 40) {
        expected_words_ = inde::project::analyze_document_text(buffer->get_text()).words;
        next(2);
      }
    } else if (phase_ == 2) {
      if (!word_count(expected_words_) || Clock::now() - phase_started_ < std::chrono::milliseconds(1200))
        return true;
      std::cout << "CHECK: Unicode burst and latest-generation counter" << std::endl;
      next(3);
    } else if (phase_ == 3) {
      // Timers schedule work; checkpoint/main commit/ACK complete asynchronously.
      if (Clock::now() - phase_started_ < std::chrono::milliseconds(1150)) return true;
      if(service_.writing().document(a_id_)->content!=buffer->get_text().raw()) {
        require(Clock::now()-phase_started_<std::chrono::seconds(5),"Idle autosave did not persist within the explicit 5 s observation limit");
        return true;
      }
      std::cout<<"GUI_AUTOSAVE_OBSERVED elapsed_ms="<<std::chrono::duration<double,std::milli>(Clock::now()-phase_started_).count()<<std::endl;
      if (++paced_ <= 16) {
        const auto begin = Clock::now();
        buffer->insert(buffer->end(), " palavra");
        std::cout << "GUI_INSERT elapsed_ms="
                  << std::chrono::duration<double, std::milli>(Clock::now() - begin).count()
                  << std::endl;
        phase_started_ = Clock::now();
      } else {
        saved_text_ = buffer->get_text().raw();
        const auto saved = service_.writing().document(a_id_);
        require(saved->anchors.size() == 1 && saved->anchors.front().id == anchor_id_ &&
                    saved->anchors.front().start_offset == 11 && saved->anchors.front().end_offset == 21 &&
                    saved->formatting.size() == 1 && saved->formatting.front().start_offset == 11 &&
                    saved->formatting.front().end_offset == 21,
                "Analysis/autosave changed formatting or anchors");
        click(writing_, "⮐");
        require(buffer->get_text().raw() != saved_text_, "Undo did not restore text");
        click(writing_, "⮑");
        require(buffer->get_text().raw() == saved_text_, "Redo lost Unicode text");
        writing_.flush_changes();
        writing_.reset();
        service_.close();
        service_.open(fixture_);
        writing_.refresh();
        writing_.reveal_document(a_id_);
        require(buffer->get_text().raw() == saved_text_, "Reopen lost manuscript text");
        // Immediately replace a queued long analysis with another document.
        writing_.reveal_document(b_id_);
        next(4);
      }
    } else if (phase_ == 4) {
      if (!word_count(2)) return true;
      require(buffer->get_text() == "Outro Documento.", "Document switch lost identity");
      writing_.reveal_document(a_id_);
      buffer->insert(buffer->end(), " encerramento pendente");
      cleanup(); // Reset with debounce/worker work outstanding, no save of test suffix.
      result_ = 0;
      std::cout << "PASS: writing async, Unicode, autosave, undo/redo, layers, "
                   "reopen, document switch and pending reset" << std::endl;
      hide(); return false;
    }
    return true;
  }
  inde::application::ProjectService service_;
  inde::ui::WritingWorkspace writing_;
  inde::persistence::JsonDictionaryStore store_;
  inde::persistence::EnchantSpellingProvider spelling_;
  inde::application::ProofreadingService proofreading_;
  inde::ui::ProofreadingController controller_;
  inde::ui::ModalOverlayHost host_;
  Gtk::TextView *editor_{};
  sigc::connection timer_;
  int &result_;
  int phase_{}, burst_{}, paced_{};
  Clock::time_point start_, phase_started_;
  std::string a_id_, b_id_, anchor_id_, long_text_, saved_text_;
  std::filesystem::path fixture_;
  std::size_t expected_words_{};
};
}
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes") {
    std::cerr << "Usage: ui_writing_smoke DISPOSABLE_FIXTURE --allow-fixture-writes\n";
    return 2;
  }
  inde::ui::configure_rendering_backend();
  int result = 1;
  auto app = Gtk::Application::create("io.github.inde.writing-smoke",
                                     Gio::Application::Flags::NON_UNIQUE);
  WritingWindow *window{};
  app->signal_activate().connect([&] {
    window = new WritingWindow(argv[1], result);
    app->add_window(*window); window->present();
  });
  app->run(1, argv);
  delete window;
  return result;
}
