#include "inde/experimental/blocks_lab_window.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/persistence/sqlite_writing_repository.hpp"
#include "inde/project/manifest.hpp"
#include "ui/widget_probe.hpp"
#include "ui/theme_probe.hpp"
#include "../src/ui/rendering_policy.hpp"
#include <algorithm>
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
namespace {
Gtk::ComboBoxText &combo(Gtk::Widget &root, const std::string &prefix) {
  auto *c = find<Gtk::ComboBoxText>(root, [&](auto &v) { return v.get_tooltip_text().raw().starts_with(prefix); }); require(c, "No combo " + prefix); return *c;
}
Gtk::TextView &text(Gtk::Widget &root, const std::string &prefix, bool writable = false) {
  auto *v = find<Gtk::TextView>(root, [&](auto &v) { return v.get_tooltip_text().raw().starts_with(prefix) && (!writable || v.get_editable()); });
  require(v, "No text " + prefix); return *v;
}
Gtk::Frame &frame(Gtk::Widget &widget) {
  for (auto *p = widget.get_parent(); p; p = p->get_parent()) if (auto *f = dynamic_cast<Gtk::Frame *>(p)) return *f;
  throw std::runtime_error("Editor has no frame");
}
void page(Gtk::Widget &root, const std::string &label) {
  auto *book = find<Gtk::Notebook>(root, [&](auto &n) {
    for (int i = 0; i < n.get_n_pages(); ++i) if (n.get_tab_label_text(*n.get_nth_page(i)).raw() == label) return true;
    return false;
  });
  require(book, "No page " + label);
  for (int i = 0; i < book->get_n_pages(); ++i) if (book->get_tab_label_text(*book->get_nth_page(i)).raw() == label) book->set_current_page(i);
}
void choose_label(Gtk::ComboBoxText &c, const std::string &label) {
  const auto model=c.get_model();
  if(model)for(auto it=model->children().begin();it!=model->children().end();++it) {
    Glib::ustring value;it->get_value(0,value);
    if(value.raw()==label) {c.set_active(it);return;}
  }
  throw std::runtime_error("Option missing " + label);
}
class Window final : public experimental::BlocksLabWindow {
public:
  Window(const std::filesystem::path &copy, int &result) : BlocksLabWindow(copy), copy_(copy), result_(result) {
    title_ = find<Gtk::Entry>(*this, [](auto &v) { return v.get_placeholder_text().raw().starts_with("Nome da montagem"); });
    name_ = find<Gtk::Entry>(*this, [](auto &v) { return v.get_placeholder_text().raw().starts_with("Nome para bloco"); }); require(title_ && name_, "No names");
    timer_ = Glib::signal_timeout().connect([this] {
      try { return step(); } catch (const std::exception &e) { std::cerr << "FAIL workshop phase=" << phase_ << ": " << e.what() << '\n'; hide(); return false; }
    }, 150);
  }
  ~Window() override { timer_.disconnect(); }
private:
  std::filesystem::path copy_; int &result_; int phase_{}, ticks_{};
  sigc::connection timer_; Gtk::Entry *title_{}, *name_{};
  std::string a_, b_, before_split_, expected_, output_title_;
  std::string read() { return text(*this, "Leitura renderizada").get_buffer()->get_text().raw(); }
  void name(const std::string &v) { name_->set_text(v); }
  void select(int i) {
    page(*this, "Usos dos blocos");
    auto *list = find<Gtk::ListBox>(*this, [](auto &v) { return v.get_row_at_index(0) &&
      find<Gtk::Label>(*v.get_row_at_index(0), [](auto &l) { return l.get_text().raw().starts_with("1. "); }); });
    require(list && list->get_row_at_index(i), "Occurrence row missing"); list->select_row(*list->get_row_at_index(i));
  }
  void apply(const std::string &label) {
    auto &editor = text(*this, "Edição do bloco: " + label, true); click(frame(editor), "Aplicar versão"); click(*this, "Confirmar operação");
  }
  bool step() {
    if (!get_mapped()) return true;
    if (phase_ == 0) {
      auto *background = find<Gtk::Widget>(*this, [](auto &w) {
        return std::string(gtk_widget_get_css_name(w.gobj())) == "stack" &&
            w.get_parent() && GTK_IS_NOTEBOOK(w.get_parent()->gobj());
      });
      require(background, "Notebook content node missing"); require_surface_background(*background);
      name("Chegada"); click(*this, "Usar Documento inteiro"); a_ = read(); require(!a_.empty(), "No first source");
      auto &editor = text(*this, "Edição do bloco: Chegada", true); editor.get_buffer()->set_text("Lia mudou.\n");
      require(read() == a_, "Draft leaked into reading");
      page(*this, "Origens"); combo(*this, "Documento de origem").set_active(1); name("Carta");
      b_ = text(*this, "Origem somente leitura").get_buffer()->get_text().raw(); click(*this, "Usar Documento inteiro");
      require(read() == a_ + b_, "Changing source reset montage");
      require(editor.get_buffer()->get_text().raw() == "Lia mudou.\n", "Draft lost switching sources");
      require(count(*this, [](auto &w) { auto *v = dynamic_cast<Gtk::Frame *>(&w); return v && v->get_visible(); }) == 1,
              "Workshop exposes simultaneous editing panels");
      for (const auto &label : {"Divisões, versões e alternativas", "Camadas de leitura"}) {
        auto *options = find<Gtk::Expander>(*this, [&](auto &e) { return e.get_label() == label; });
        require(options && !options->get_expanded(), "Workshop options should initially be collapsed");
        options->set_expanded(true);
      }
      select(0); click(*this, "Reutilizar uso do bloco"); require(read() == a_ + b_ + a_, "No linked reuse");
      apply("Chegada"); require(read() == "Lia mudou.\n" + b_ + "Lia mudou.\n", "Linked uses not updated");
      combo(*this, "Versão do bloco").set_active(0); click(*this, "Manter versão escolhida");
      require(read() == "Lia mudou.\n" + b_ + a_, "Old version not pinned");
      text(*this, "Edição do bloco: Chegada", true).get_buffer()->set_text("Lia retornou.\n"); apply("Chegada");
      require(read() == "Lia retornou.\n" + b_ + a_, "Pinned version overwritten");
      combo(*this, "Versão do bloco").set_active(0); click(*this, "Abrir versão");
      require(count(*this, [](auto &w) { auto *v = dynamic_cast<Gtk::TextView *>(&w); return v && v->get_tooltip_text().raw().starts_with("Edição do bloco: Chegada"); }) >= 2, "No simultaneous historical editor");
      auto *historical = find<Gtk::TextView>(*this, [](auto &v) { return v.get_tooltip_text().raw().starts_with("Edição do bloco: Chegada") && !v.get_editable(); });
      require(historical, "No historical panel"); const auto before_close = read(); click(frame(*historical), "Fechar painel"); click(*this, "Confirmar operação");
      require(read() == before_close, "Closing panel changed montage");
      page(*this, "Origens"); combo(*this, "Documento de origem").set_active(0); name("Chave");
      auto &origin = text(*this, "Origem somente leitura"); const auto body = origin.get_buffer()->get_text().raw();
      const auto first = body.find('\n') + 1, last = body.find('\n', first) + 1;
      const auto start = project::utf8_character_count(body.substr(0, first)), end = project::utf8_character_count(body.substr(0, last));
      origin.get_buffer()->select_range(origin.get_buffer()->get_iter_at_offset(start), origin.get_buffer()->get_iter_at_offset(end));
      click(*this, "Extrair seleção"); before_split_ = read();
      auto &selected = text(*this, "Edição do bloco: Chave", true); selected.get_buffer()->select_range(selected.get_buffer()->begin(), selected.get_buffer()->get_iter_at_offset(14));
      name("Plataforma"); click(*this, "Separar seleção em bloco"); click(*this, "Confirmar operação"); require(read() == before_split_, "Author split changed rendered text");
      name("Plataforma alternativa"); click(*this, "Criar alternativa");
      auto &alt = text(*this, "Edição do bloco: Plataforma alternativa", true); alt.get_buffer()->set_text("Na velha plataforma,"); apply("Plataforma alternativa");
      require(read().find("Na velha plataforma,") != std::string::npos, "Alternative editor not selected");
      choose_label(combo(*this, "Alternativas de leitura"), "Usar Base em todas as famílias"); require(read().find("Na velha plataforma,") == std::string::npos, "Base lost");
      choose_label(combo(*this, "Alternativas de leitura"), "Plataforma alternativa");
      name("Plataforma final"); click(*this, "Transformar em bloco reutilizável"); click(*this, "Confirmar operação"); require(read().find("Na velha plataforma,") != std::string::npos, "Promotion not in reading");
      name("Revelação"); click(*this, "Criar camada"); click(*this, "Incluir/retirar da camada");
      click(*this, "Mostrar/ocultar camada"); require(read().find("Na velha plataforma,") == std::string::npos, "Hidden layer shown");
      click(*this, "Mostrar/ocultar camada"); click(*this, "Ocultar/mostrar uso do bloco"); require(read().find("Na velha plataforma,") == std::string::npos, "Hidden occurrence shown");
      click(*this, "Ocultar/mostrar uso do bloco");
      click(*this, "Converter em referência"); click(*this, "Confirmar operação"); require(read().find("Na velha plataforma,") == std::string::npos, "Reference transcluded");
      click(*this, "Acompanhar versão atual"); require(read().find("Na velha plataforma,") != std::string::npos, "Reference did not return to text");
      page(*this, "Posições e origens dos blocos"); std::cout << "SNAPSHOT: workshop-map\n" << std::flush; phase_ = 1; return true;
    }
    if (phase_ == 1) {
      if (++ticks_ < 8) return true;
      select(1); const auto before = read(); click(*this, "Mover acima"); require(read() != before, "Order unchanged");
      click(*this, "Desfazer montagem"); require(read() == before, "Undo order failed");
      click(*this, "Retirar da montagem"); click(*this, "Cancelar"); require(read() == before, "Cancel removed occurrence");
      click(*this, "Retirar da montagem"); click(*this, "Confirmar operação"); require(read() != before, "Occurrence not removed");
      click(*this, "Desfazer montagem"); require(read() == before, "Undo removal lost source");
      // Load a persisted origin where present; combine without resetting.
      page(*this, "Origens"); auto &origins = combo(*this, "Documento de origem");
      const auto model = origins.get_model();
      if (model && model->children().size() > 2) { origins.set_active(2); name("Documento da cópia"); click(*this, "Usar Documento inteiro"); }
      expected_ = read(); std::string large; for (int i = 0; i < 20000; ++i) large += "Água memória caminho história mundo.\n";
      text(*this, "Edição do bloco: Carta", true).get_buffer()->set_text(large);
      require(read() == expected_, "Large unconfirmed draft leaked into reading");
      click(*this, "Preparar novo Documento");
      auto *publish = find<Gtk::Button>(*this, [](auto &v) { return v.get_label() == "Criar Documento na cópia"; });
      require(publish && !publish->get_sensitive(), "Large pending draft allowed publication");
      page(*this, "Leitura"); std::cout << "SNAPSHOT: workshop-large-draft\n" << std::flush; phase_ = 10; ticks_ = 0; return true;
    }
    if (phase_ == 10) {
      if (++ticks_ < 8) return true;
      auto &draft = text(*this, "Edição do bloco: Carta", true); require(draft.get_buffer()->get_char_count() > 500000, "Large draft lost before paint");
      click(frame(draft), "Descartar rascunho"); click(*this, "Confirmar operação"); require(read() == expected_, "Discarding draft changed montage");
      output_title_ = "Montagem QA — " + project::new_uuid(); title_->set_text(output_title_);
      std::cout << "SNAPSHOT: workshop-reading\n" << std::flush; phase_ = 2; ticks_ = 0; return true;
    }
    if (phase_ == 2) {
      if (++ticks_ < 8) return true;
      click(*this, "Preparar novo Documento");
      std::cout << "SNAPSHOT: workshop-output\n" << std::flush; phase_ = 3; ticks_ = 0; return true;
    }
    if (phase_ == 3) {
      if (++ticks_ < 8) return true;
      auto *check = find<Gtk::CheckButton>(*this, [](auto &v) { return v.get_label().raw().starts_with("Li os avisos"); }); require(check, "No conservation acknowledgement");
      auto *publish = find<Gtk::Button>(*this, [](auto &v) { return v.get_label() == "Criar Documento na cópia"; }); require(publish && !publish->get_sensitive(), "Publish enabled without consent");
      check->set_active(true); click(*this, "Criar Documento na cópia"); click(*this, "Confirmar operação");
      persistence::SqliteWritingRepository repo; persistence::DocumentQuery q; q.search = output_title_;
      auto docs = repo.documents(copy_, q); require(docs.size() == 1 && docs[0].content == expected_, "Created document roundtrip differs");
      require(!docs[0].formatting.empty(), "Composed styles missing");
      std::cout << "CREATED_DOCUMENT_ID=" << docs[0].id << '\n';
      std::cout << "PASS: author-selected cuts, multi-source montage, simultaneous versions/drafts, linked/pinned, named promoted alternatives, layers/reference/order/undo, explicit document create/reopen\n" << std::flush;
      result_ = 0; hide(); return false;
    }
    return false;
  }
};
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  int result = 1;
  try {
    ui::configure_rendering_backend();
    auto app = Gtk::Application::create("io.github.inde.workshop.qa", Gio::Application::Flags::NON_UNIQUE);
    app->signal_startup().connect([] { ui::install_application_theme(); });
    app->make_window_and_run<Window>(1, argv, std::filesystem::path(argv[1]), std::ref(result));
  } catch (const std::exception &e) { std::cerr << "FAIL workshop: " << e.what() << '\n'; }
  return result;
}
