#include "../src/ui/editor_color_picker.hpp"
#include "../src/ui/rendering_policy.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include "ui/theme_probe.hpp"
#include <chrono>
#include <iostream>
namespace {
using namespace inde;
using namespace inde::test::ui;
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &fixture, int &result)
      : writing_(service_), result_(result) {
    add_css_class("inde-window");
    service_.open(fixture);
    auto d = service_.writing().create_document("QA RGB visual");
    d.content = "Água 🙂 e palavras autorais.";
    id_ = service_.writing().update_document(d).id;
    host_.set_content(writing_);
    set_child(host_);
    set_default_size(1280, 900);
    writing_.install_actions(*this);
    writing_.signal_status_message().connect([](const std::string &s) {
      std::cout << "STATUS: " << s << std::endl;
    });
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
            std::cerr << "FAIL color phase=" << phase_ << ": " << e.what()
                      << std::endl;
            hide();
            return false;
          }
        },
        100);
  }
  ~Window() override { timer_.disconnect(); }

private:
  ui::EditorColorPicker &picker() {
    auto *p = find<ui::EditorColorPicker>(host_, [](auto &) { return true; });
    require(p, "No visual picker");
    return *p;
  }
  Gtk::Entry &hex() {
    auto *e = find<Gtk::Entry>(picker(), [](auto &e) {
      return e.get_placeholder_text() == "Cor #RRGGBB";
    });
    require(e, "No hex");
    return *e;
  }
  void rgb(int r, int g, int b) {
    for (auto [label, value] :
         {std::pair{"R — Vermelho", r}, {"G — Verde", g}, {"B — Azul", b}}) {
      auto *s = find<Gtk::SpinButton>(
          picker(), [&](auto &v) { return v.get_tooltip_text() == label; });
      require(s, "No channel");
      s->set_value(value);
    }
  }
  project::Document saved() {
    writing_.flush_changes();
    return *service_.writing().document(id_);
  }
  bool color(const std::string &value) {
    auto d = saved();
    return std::any_of(d.formatting.begin(), d.formatting.end(), [&](auto &s) {
      return s.style == project::DocumentTextStyle::Foreground &&
             s.value == value;
    });
  }
  void select() {
    auto b = editor_->get_buffer();
    b->select_range(b->begin(), b->get_iter_at_offset(6));
  }
  bool step() {
    if (phase_ == 0) {
      if (!get_mapped())
        return true;
      select();
      click(writing_, "A");
      rgb(17, 34, 51);
      require(picker().color_hex() == "#112233", "RGB synchronization");
      require(count(picker(),
                    [](auto &w) {
                      return dynamic_cast<Gtk::DrawingArea *>(&w) != nullptr;
                    }) >= 2,
              "No visual HSV surface");
      for (auto [label, value] :
           {std::pair{"Tonalidade — H", 240}, std::pair{"Saturação — S", 100},
            std::pair{"Luminosidade — V", 100}}) {
        auto *scale = find<Gtk::Scale>(
            picker(), [&](auto &v) { return v.get_tooltip_text() == label; });
        require(scale, "HSV keyboard control absent");
        scale->set_value(value);
      }
      require(picker().color_hex() == "#0000ff",
              "HSV conversion did not produce blue");
      auto scale = [&](const std::string &label) -> Gtk::Scale & {
        auto *control = find<Gtk::Scale>(picker(), [&](auto &v) {
          return v.get_tooltip_text().raw() == label;
        });
        require(control, "HSV control missing");
        return *control;
      };
      scale("Luminosidade — V").set_value(0);
      require(picker().color_hex() == "#000000" &&
                  scale("Saturação — S").get_value() == 100,
              "Black lost the user's saturation");
      scale("Luminosidade — V").set_value(100);
      require(picker().color_hex() == "#0000ff", "Brightness lost hue");
      scale("Saturação — S").set_value(0);
      require(picker().color_hex() == "#ffffff" &&
                  scale("Tonalidade — H").get_value() == 240,
              "White lost hue");
      scale("Tonalidade — H").set_value(360);
      scale("Saturação — S").set_value(100);
      require(picker().color_hex() == "#ff0000", "Hue wrap not red");
      rgb(136, 170, 255);
      require(picker().color_hex() == "#88aaff" &&
                  hex().get_text() == "#88aaff",
              "HSV synchronization");
      hex().set_text("#123");
      require(!picker().color_hex(), "Invalid hex accepted");
      click(host_, "Aplicar cor");
      require(saved().formatting.empty(), "Invalid color mutated text");
      click(writing_, "A");
      rgb(17, 34, 51);
      require(picker().color_hex() == "#112233",
              "RGB did not recover from invalid hex");
      std::cout << "SNAPSHOT: color-hsv\n" << std::flush;
      shot_ = std::chrono::steady_clock::now();
      ++phase_;
    } else if (phase_ == 1) {
      if (std::chrono::steady_clock::now() - shot_ < std::chrono::seconds(2))
        return true;
      auto *preview = find<Gtk::DrawingArea>(picker(), [](auto &area) {
        return area.get_content_height() == 54;
      });
      require(preview && preview->get_mapped(), "Color preview not allocated");
      require_surface_background(*preview, "inde_paper");
      click(host_, "Mostrar paleta / ajustar tonalidade");
      std::cout << "SNAPSHOT: color-palette\n" << std::flush;
      shot_ = std::chrono::steady_clock::now();
      ++phase_;
    } else if (phase_ == 2) {
      if (std::chrono::steady_clock::now() - shot_ < std::chrono::seconds(2))
        return true;
      click(host_, "Cancelar");
      require(saved().formatting.empty(), "Cancel mutated text");
      select();
      click(writing_, "A");
      hex().set_text("#88aaff");
      click(host_, "Aplicar cor");
      require(color("#88aaff"), "Color not applied");
      click(writing_, "⮐");
      require(saved().formatting.empty(), "Color undo");
      click(writing_, "⮑");
      require(color("#88aaff"), "Color redo");
      select();
      click(writing_, "A");
      require(picker().color_hex() == "#88aaff", "Existing color not selected");
      hex().set_text("#eeaa77");
      editor_->get_buffer()->place_cursor(editor_->get_buffer()->end());
      editor_->get_buffer()->insert_at_cursor(" mudança externa QA");
      click(host_, "Aplicar cor");
      require(color("#88aaff") && !color("#eeaa77"), "Stale revision applied");
      auto before = saved();
      writing_.reset();
      writing_.refresh();
      writing_.reveal_document(id_);
      require(project::same_document_state(saved(), before),
              "Reopen color changed");
      writing_.reset();
      service_.writing().delete_document(id_);
      service_.close();
      result_ = 0;
      std::cout << "PASS: current GTK/Cairo HSV/palette, RGB/hex sync, "
                   "invalid/recovery, cancel, apply, undo/redo, stale "
                   "selection, reopen and teardown\n"
                << std::flush;
      hide();
      return false;
    }
    return true;
  }
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  Gtk::TextView *editor_{};
  sigc::connection timer_;
  int &result_;
  int phase_{};
  std::string id_;
  std::chrono::steady_clock::time_point shot_;
};
} // namespace
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes" ||
      !std::getenv("INDE_CLIPBOARD_TEST_ISOLATED"))
    return 2;
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.colorqa",
                                      Gio::Application::Flags::NON_UNIQUE);
  int result = 1;
  Window *w{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    w = new Window(argv[1], result);
    app->add_window(*w);
    w->present();
  });
  app->run(1, argv);
  delete w;
  return result;
}
