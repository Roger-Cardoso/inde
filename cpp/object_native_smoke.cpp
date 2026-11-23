#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/main_window.hpp"
#include "main_window_smoke.hpp"
#include "object_math_native.hpp"
#include "object_native_probe.hpp"
#include "object_native_ui.hpp"
#include <deque>
#include <fstream>
#include <iostream>
void capture_window(Gtk::Widget &, const std::filesystem::path &);
Gtk::Button *find_button(Gtk::Widget &, const std::string &);
namespace {
bool capture_pending = false;
std::string capture_error;
sigc::connection capture_frame;
// Screenshots belong after GTK has laid out and painted the current frame.
// This is test instrumentation only; it does not drive application state.
void capture_native(Gtk::Widget &w, const std::filesystem::path &p) {
  if (capture_pending)
    throw std::runtime_error("Captura já pendente");
  const auto clock = w.get_frame_clock();
  if (!clock)
    throw std::runtime_error("Sem relógio de pintura para captura");
  capture_pending = true;
  capture_frame = clock->signal_paint().connect(
      [target = &w, p] {
        capture_frame.disconnect();
        try {
          std::cerr << "GTK capture: " << p.filename()
                    << " size=" << target->get_width() << "x"
                    << target->get_height() << "\n";
          capture_window(*target, p);
        } catch (const std::exception &e) {
          capture_error = e.what();
        }
        capture_pending = false;
      },
      true);
  w.queue_allocate();
  w.queue_draw();
  clock->request_phase(Gdk::FrameClock::Phase::PAINT);
}
using V = inde::persistence::json::Value;
using Probe = inde::ui::ObjectNativeProbe;
void need(bool value, const std::string &message) {
  if (!value)
    throw std::runtime_error(message);
}
Gtk::Widget *find(Gtk::Widget &w, const std::string &name) {
  if (w.get_name().raw() == name)
    return &w;
  for (auto *c = w.get_first_child(); c; c = c->get_next_sibling())
    if (auto *v = find(*c, name))
      return v;
  return nullptr;
}
template <class T> T &widget(Gtk::Widget &w, const std::string &name) {
  auto *v = dynamic_cast<T *>(find(w, name));
  if (!v)
    throw std::runtime_error("Controle nativo ausente: " + name);
  return *v;
}
void click(Gtk::Widget &w, const std::string &name) {
  auto *objects = dynamic_cast<inde::ui::ObjectDocumentEditor *>(&w);
  if (objects) {
    std::string specialty;
    if (name == "native-new-equation")
      specialty = "math";
    if (name == "native-new-table" || name == "native-table-style" ||
        name == "native-table-structure")
      specialty = "tables";
    if (name == "native-new-formula" || name == "native-formula-edit" ||
        name == "native-variables")
      specialty = "formulas";
    if (name == "native-new-image" || name == "native-layout" ||
        name == "native-remove" || name == "native-restore")
      specialty = "composition";
    if (!specialty.empty()) {
      auto *parent = w.get_parent();
      while (parent && !dynamic_cast<inde::ui::WritingWorkspace *>(parent))
        parent = parent->get_parent();
      need(parent, "Object surface outside Writing");
      g_signal_emit_by_name(
          widget<Gtk::ToggleButton>(*parent, "specialty-" + specialty).gobj(),
          "clicked");
      if (!Probe::tools_open(*objects))
        g_signal_emit_by_name(widget<Gtk::Button>(w, "native-tools").gobj(),
                              "clicked");
    }
  }
  g_signal_emit_by_name(widget<Gtk::Button>(w, name).gobj(), "clicked");
}
void field(Gtk::Widget &w, const std::string &key, const std::string &s) {
  widget<Gtk::Entry>(w, "native-field-" + key).set_text(s);
}
void choose(Gtk::Widget &w, const std::string &key, const std::string &s) {
  need(widget<Gtk::ComboBoxText>(w, "native-field-" + key).set_active_id(s),
       "Opção nativa ausente: " + s);
}
std::size_t math_widgets(Gtk::Widget &w) {
  std::size_t count = 0;
  need(std::string(G_OBJECT_TYPE_NAME(w.gobj())).find("WebKit") ==
           std::string::npos,
       "Widget WebKit presente");
  if (auto *m = dynamic_cast<inde::ui::MathPreview *>(&w)) {
    need(m->rendered(), "Equação sem desenho nativo: " + m->error());
    ++count;
  }
  for (auto *c = w.get_first_child(); c; c = c->get_next_sibling())
    count += math_widgets(*c);
  return count;
}
} // namespace
int run_object_window_smoke(const Glib::RefPtr<Gtk::Application> &app,
                            const std::filesystem::path &project,
                            const std::filesystem::path &picture) {
  std::string id, original, revised_body;
  std::vector<std::string> paragraph_ids;
  try {
    inde::application::ProjectService service;
    service.create(project, "Objetos e matemática");
    auto d = service.writing().create_document("Matemática na narrativa");
    d.content = "Uma travessia pode seguir muitas direções.\nAs relações "
                "também podem ser expressas com matemática.";
    d.paragraphs = inde::project::identify_paragraphs(d.content);
    d.visibility_profiles = {{inde::project::new_uuid(),
                              "Somente segundo parágrafo",
                              true,
                              {},
                              {{inde::project::VisibilityRuleKind::Paragraph,
                                d.paragraphs.rows[0].id}}}};
    d.formatting = {{inde::project::DocumentTextStyle::Bold, 0, 3, {}}};
    d.anchors = {{inde::project::new_uuid(), "Segundo parágrafo",
                  d.paragraphs.rows[1].start, d.paragraphs.rows[1].start + 2}};
    d = service.writing().update_document(d);
    id = d.id;
    original = inde::persistence::encode_document_snapshot(d);
    for (const auto &p : d.paragraphs.rows)
      paragraph_ids.push_back(p.id);
    service.close();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  auto png = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, false, 8, 160, 80);
  png->fill(0x4b8d78ff);
  const auto image = picture.parent_path() / "figura-teste-gtk.png";
  std::filesystem::create_directories(picture.parent_path());
  png->save(image.string(), "png");
  std::unique_ptr<inde::ui::MainWindow> main;
  inde::ui::ObjectDocumentEditor *objects = nullptr;
  std::deque<std::function<void()>> steps;
  int ticks = 0, stage = 0, result = 1, settle = 0;
  std::string table, cell, formula, equation;
  auto state = [&]() -> const V & { return Probe::state(*objects); };
  auto equation_step = [&](const std::string &name, const std::string &model) {
    steps.push_back([&, name, model] {
      click(*objects, "native-new-equation");
      field(*objects, "label", name);
      choose(*objects, "template", model);
    });
    steps.push_back([&] {
      capture_native(*main, picture.parent_path() / "gtk-editor-equacao.png");
    });
    steps.push_back([&] { click(*objects, "native-apply"); });
  };
  steps.push_back([&] {
    click(*objects, "native-new-equation");
    need(Probe::draft(*objects), "Rascunho nativo ausente");
    need(!widget<Gtk::Box>(*main, "writing-specialty-strip").get_sensitive(),
         "Barra não protegeu rascunho aberto");
    main->close();
    need(objects->get_visible() && main->get_visible(), "Fechou com rascunho");
    widget<Gtk::TextView>(*objects, "native-field-source")
        .get_buffer()
        ->set_text(R"(x = \frac{-b\pm\sqrt{b^2-4ac}}{2a})");
    for (const auto *model :
         {"fraction", "root", "sum", "integral", "matrix", "system"}) {
      choose(*objects, "template", model);
      need(math_widgets(*objects) >= 1, "Modelo matemático sem desenho Cairo");
    }
    widget<Gtk::TextView>(*objects, "native-field-source")
        .get_buffer()
        ->set_text(R"(x = \frac{-b\pm\sqrt{b^2-4ac}}{2a})");
    field(*objects, "label", "Segundo grau");
  });
  steps.push_back([&] { click(*objects, "native-apply"); });
  equation_step("Somatório", "sum");
  equation_step("Transformação", "matrix");
  steps.push_back([&] {
    need(state().at("equations").as_array().size() == 3,
         "Três equações ausentes");
    for (const auto &e : state().at("equations").as_array())
      if (e.at("label").as_string() == "Transformação")
        equation = e.at("id").as_string();
    need(!equation.empty(), "Matriz ausente");
    need(math_widgets(*objects) >= 3, "Prévia nativa ausente");
    click(*objects, "native-new-image");
    Probe::choose_image(*objects, image);
  });
  steps.push_back([&] {
    field(*objects, "alt", "Faixa verde de exemplo");
    field(*objects, "caption", "Figura nativa");
    click(*objects, "native-apply");
  });
  steps.push_back([&] {
    need(state().at("figures").as_array().size() == 1, "Figura ausente");
    click(*objects, "native-new-table");
    field(*objects, "title", "Combate");
  });
  steps.push_back([&] { click(*objects, "native-apply"); });
  steps.push_back([&] {
    const auto &t = state().at("tables").as_array().at(0);
    table = t.at("id").as_string();
    cell = t.at("cells").as_array().at(0).at("id").as_string();
    click(*objects, "native-cell-" + cell);
  });
  steps.push_back([&] { click(*objects, "native-cell-formula"); });
  steps.push_back([&] {
    click(*objects, "native-kind-value");
    field(*objects, "name", "Vida");
    field(*objects, "prefix", "Vida: ");
    field(*objects, "suffix", " pontos");
  });
  steps.push_back([&] { click(*objects, "native-preview"); });
  steps.push_back([&] { click(*objects, "native-apply"); });
  steps.push_back([&] {
    need(state().at("formula_cells").as_object().contains(cell),
         "Fórmula ausente na célula");
    formula = state().at("formula_cells").at(cell).as_string();
    click(*objects, "native-new-formula");
  });
  steps.push_back([&] { click(*objects, "native-kind-change"); });
  steps.push_back([&] { click(*objects, "native-apply"); });
  steps.push_back([&] {
    const auto &f = state().at("formulas").as_array().back();
    for (const auto &u : state().at("formula_uses").as_array())
      if (u.at("object").as_string() == f.at("id").as_string())
        Probe::reveal_object(*objects, u.at("id").as_string());
  });
  steps.push_back([&] {
    const auto &f = state().at("formulas").as_array().back();
    for (const auto &u : state().at("formula_uses").as_array())
      if (u.at("object").as_string() == f.at("id").as_string())
        Probe::open_context(*objects, u.at("id").as_string());
  });
  steps.push_back([&] {
    const auto &f = state().at("formulas").as_array().back();
    click(*objects, "native-execute-" + f.at("id").as_string());
  });
  auto value = [&] {
    for (const auto &f : state().at("formulas").as_array())
      if (f.at("id").as_string() == formula)
        return f.at("display").as_string();
    return std::string();
  };
  steps.push_back([&] {
    need(value() == "80", "Alteração remota não chegou a 80");
    click(*objects, "native-undo");
  });
  steps.push_back([&] {
    need(value() == "100", "Desfazer alteração falhou");
    click(*objects, "native-redo");
  });
  steps.push_back([&] {
    need(value() == "80", "Refazer alteração falhou");
    widget<Gtk::ComboBoxText>(*objects, "native-selection")
        .set_active_id(table);
  });
  steps.push_back([&] {
    click(*objects, "native-table-style");
    choose(*objects, "preset", "notebook");
  });
  steps.push_back([&] { click(*objects, "native-apply"); });
  steps.push_back([&] { click(*objects, "native-table-structure"); });
  steps.push_back([&] { click(*objects, "native-insert-row"); });
  steps.push_back([&] {
    need(state().at("tables").as_array()[0].at("row_ids").as_array().size() ==
             3,
         "Inserção de linha falhou");
    click(*objects, "native-table-structure");
    const auto first = state()
                           .at("tables")
                           .as_array()[0]
                           .at("row_ids")
                           .as_array()[0]
                           .as_string();
    choose(*objects, "row", first);
  });
  steps.push_back([&] { click(*objects, "native-retire-row"); });
  steps.push_back([&] { click(*objects, "native-table-structure"); });
  steps.push_back([&] { click(*objects, "native-restore-row"); });
  steps.push_back([&] { click(*objects, "native-new-formula"); });
  steps.push_back([&] {
    click(*objects, "native-kind-condition");
    field(*objects, "name", "Estado");
    choose(*objects, "entry-reference", "Vida.HP");
    widget<Gtk::Entry>(*objects, "native-rule-result-0").set_text("em combate");
    field(*objects, "otherwise", "fora do combate");
    click(*objects, "native-add-condition");
    widget<Gtk::Entry>(*objects, "native-rule-value-1").set_text("0");
    widget<Gtk::Entry>(*objects, "native-rule-result-1").set_text("caído");
    click(*objects, "native-rule-up-1");
  });
  steps.push_back([&] { click(*objects, "native-preview"); });
  steps.push_back([&] { click(*objects, "native-apply"); });
  steps.push_back([&] {
    bool found = false;
    for (const auto &f : state().at("formulas").as_array())
      if (f.at("name").as_string() == "Estado")
        found = f.at("display").as_string() == "em combate";
    need(found, "Cartão numérico não avaliou a referência");
    click(*objects, "native-new-formula");
  });
  steps.push_back([&] {
    click(*objects, "native-kind-condition");
    field(*objects, "name", "Atitude");
    choose(*objects, "type", "text");
    field(*objects, "entry-value", "hostil");
    widget<Gtk::Entry>(*objects, "native-rule-value-0").set_text("hostil");
    widget<Gtk::Entry>(*objects, "native-rule-result-0").set_text("ameaçador");
    field(*objects, "otherwise", "indecifrável");
    click(*objects, "native-add-condition");
    click(*objects, "native-rule-remove-1");
  });
  steps.push_back([&] { click(*objects, "native-apply"); });
  steps.push_back([&] {
    bool found = false;
    for (const auto &f : state().at("formulas").as_array())
      if (f.at("name").as_string() == "Atitude")
        found = f.at("display").as_string() == "ameaçador";
    need(found, "Cartão comparativo textual falhou");
  });
  steps.push_back([&] {
    widget<Gtk::ComboBoxText>(*objects, "native-selection")
        .set_active_id(equation);
  });
  steps.push_back([&] { click(*objects, "native-remove"); });
  steps.push_back([&] {
    need(state().at("equations").as_array().size() == 2,
         "Retirada de equação falhou");
    click(*objects, "native-restore");
    choose(*objects, "removed", equation);
  });
  steps.push_back([&] { click(*objects, "native-apply"); });
  steps.push_back([&] {
    widget<Gtk::ComboBoxText>(*objects, "native-selection")
        .set_active_id(equation);
  });
  steps.push_back([&] {
    click(*objects, "native-layout");
    choose(*objects, "other",
           state().at("paragraphs").as_array()[0].at("id").as_string());
  });
  steps.push_back([&] { click(*objects, "native-pair"); });
  steps.push_back([&] {
    need(state().at("rows").as_array().size() == 1, "Arranjo nativo ausente");
    click(*objects, "native-save");
  });
  steps.push_back([&] {
    need(!state().at("dirty").as_boolean(), "Documento não salvo");
    capture_native(*main, picture);
    Probe::reveal_object(*objects, equation);
  });
  steps.push_back([&] { Probe::open_context(*objects, equation); });
  steps.push_back([&] { click(*objects, "native-equation-edit-" + equation); });
  steps.push_back([&] {
    field(*objects, "label", "Equação revisada");
    click(*objects, "native-apply");
  });
  steps.push_back([&] { click(*objects, "native-align-reading"); });
  steps.push_back([&] {
    widget<Gtk::ComboBoxText>(*objects, "native-selection")
        .set_active_id(
            state().at("paragraphs").as_array()[1].at("id").as_string());
  });
  steps.push_back([&] {
    click(*objects, "native-layout");
    choose(*objects, "intent", "authorial");
    choose(*objects, "before",
           state().at("paragraphs").as_array()[0].at("id").as_string());
    click(*objects, "native-move");
  });
  steps.push_back([&] {
    need(widget<Gtk::TextView>(*main, "writing-text-editor")
             .get_buffer()
             ->get_text()
             .raw()
             .starts_with("As relações"),
         "Movimento autoral não chegou ao editor original");
    click(*objects, "native-undo");
  });
  steps.push_back([&] {
    need(widget<Gtk::TextView>(*main, "writing-text-editor")
             .get_buffer()
             ->get_text()
             .raw()
             .starts_with("Uma travessia"),
         "Desfazer movimento não chegou ao editor original");
  });
  steps.push_back([&] {
    click(*main, "specialty-text");
    auto buffer =
        widget<Gtk::TextView>(*main, "writing-text-editor").get_buffer();
    auto at = buffer->begin();
    buffer->insert(at, "Agora ");
    revised_body = buffer->get_text();
    click(*main, "specialty-math");
    need(Probe::state(*objects)
             .at("paragraphs")
             .as_array()[0]
             .at("text")
             .as_string()
             .starts_with("Agora "),
         "Composição manteve cópia antiga do texto");
    need(Probe::state(*objects).at("equations").as_array().size() == 3,
         "Troca de especialidade perdeu objetos");
    need(Probe::state(*objects)
                 .at("paragraphs")
                 .as_array()[0]
                 .at("paragraph")
                 .as_string() == paragraph_ids[0],
         "Identidade mudou ao escrever");
  });
  steps.push_back([&] {
    click(*main, "specialty-text");
    click(*main, "writing-undo");
    click(*main, "specialty-math");
    need(!state()
              .at("paragraphs")
              .as_array()[0]
              .at("text")
              .as_string()
              .starts_with("Agora "),
         "Desfazer texto não chegou à composição");
    click(*main, "specialty-text");
    click(*main, "writing-redo");
    click(*main, "specialty-math");
    need(state()
             .at("paragraphs")
             .as_array()[0]
             .at("text")
             .as_string()
             .starts_with("Agora "),
         "Refazer texto não chegou à composição");
    click(*objects, "native-undo");
  });
  steps.push_back([&] {
    need(state()
             .at("paragraphs")
             .as_array()[0]
             .at("text")
             .as_string()
             .starts_with("Agora "),
         "Desfazer objeto trouxe texto antigo");
    need(value() == "80", "Desfazer objeto alterou variável remota");
    click(*objects, "native-redo");
  });
  steps.push_back([&] {
    widget<Gtk::Entry>(*main, "writing-title")
        .set_text("Matemática na narrativa revisada");
    click(*objects, "native-save");
    need(state().at("document_title").as_string() ==
             "Matemática na narrativa revisada",
         "Salvar não incluiu metadados pendentes da Escrita");
    need(!state().at("dirty").as_boolean(),
         "Salvar deixou a composição pendente");
    widget<Gtk::Entry>(*main, "writing-title")
        .set_text("Matemática na narrativa");
    click(*objects, "native-save");
  });
  const auto click_label = [&](const std::string &caption) {
    for (auto *w : app->get_windows())
      if (auto *b = find_button(*w, caption)) {
        g_signal_emit_by_name(b->gobj(), "clicked");
        return;
      }
    throw std::runtime_error("Botão de visibilidade ausente: " + caption);
  };
  steps.push_back([&] {
    widget<Gtk::ComboBoxText>(*objects, "native-selection")
        .set_active_id(
            state().at("paragraphs").as_array()[0].at("id").as_string());
  });
  steps.push_back([&] { click(*main, "editor-visibility"); });
  steps.push_back([&] { click_label("Somente segundo parágrafo"); });
  steps.push_back([&] { click_label("Aplicar nesta área de escrita"); });
  steps.push_back([&] {
    need(!find(*objects,
               "native-object-" +
                   state().at("paragraphs").as_array()[0].at("id").as_string()),
         "Lente antiga não atualizou a composição");
    need(find(*objects,
              "native-object-" +
                  state().at("paragraphs").as_array()[1].at("id").as_string()),
         "Lente ocultou o parágrafo errado");
    need(widget<Gtk::TextView>(*main, "writing-text-editor")
             .get_buffer()
             ->get_text()
             .raw()
             .starts_with("Agora "),
         "Visibilidade apagou origem");
    click(*objects, "native-layout");
    need(!Probe::draft(*objects),
         "Objeto oculto continuou selecionado para movimentação");
    click_label("Mostrar tudo");
  });
  steps.push_back([&] {
    need(find(*objects,
              "native-object-" +
                  state().at("paragraphs").as_array()[0].at("id").as_string()),
         "Mostrar tudo não restaurou a composição");
  });
  steps.push_back([&] {
    click(*main, "specialty-composition");
    objects->show_tools(false);
    need(!Probe::tools_open(*objects), "Painel não recolheu");
    Probe::reveal_object(
        *objects, state().at("paragraphs").as_array()[0].at("id").as_string());
  });
  steps.push_back([&] {
    const auto &first = state().at("paragraphs").as_array()[0];
    Probe::open_context(*objects, first.at("id").as_string());
    need(widget<Gtk::Button>(*objects, "native-paragraph-edit-" +
                                           first.at("id").as_string())
             .get_visible(),
         "Menu contextual ausente");
  });
  steps.push_back([&] {
    capture_native(widget<Gtk::Popover>(*objects, "native-object-menu"),
                   picture.parent_path() / "gtk-menu-objeto.png");
    stage = 2;
  });
  app->signal_activate().connect([&] {
    std::cerr << "GTK probe: criando janela principal; Objetos ainda fechado\n";
    main = std::make_unique<inde::ui::MainWindow>(project);
    std::cerr
        << "GTK probe: janela principal construída; Objetos ainda fechado\n";
    app->add_window(*main);
    if (g_getenv("INDE_OBJ01_SMOKE_COMPACT")) {
      main->unmaximize();
      main->set_default_size(1190, 750);
    }
    main->present();
    Glib::signal_timeout().connect(
        [&] {
          try {
            if (++ticks > 600)
              throw std::runtime_error("Timeout GTK, etapa " +
                                       std::to_string(stage) + ", passos " +
                                       std::to_string(steps.size()));
            if (!capture_error.empty())
              throw std::runtime_error(capture_error);
            if (capture_pending)
              return true;
            if (stage == 0) {
              std::function<inde::ui::WritingWorkspace *(Gtk::Widget &)>
                  find_writing =
                      [&](Gtk::Widget &w) -> inde::ui::WritingWorkspace * {
                if (auto *p = dynamic_cast<inde::ui::WritingWorkspace *>(&w))
                  return p;
                for (auto *c = w.get_first_child(); c;
                     c = c->get_next_sibling())
                  if (auto *p = find_writing(*c))
                    return p;
                return nullptr;
              };
              main->lookup_action("show-writing")->activate();
              auto *writing = find_writing(*main);
              need(writing, "Escrita ausente");
              writing->reveal_document(id);
              if (!writing->publication_ready())
                return true;
              std::cerr << "GTK probe: abrindo Objetos C++/GTK\n";
              main->lookup_action("document-objects")->activate();
              stage = 1;
              return true;
            }
            objects = nullptr;
            std::function<void(Gtk::Widget &)> locate = [&](Gtk::Widget &w) {
              if (auto *p = dynamic_cast<inde::ui::ObjectDocumentEditor *>(&w))
                objects = p;
              for (auto *c = w.get_first_child(); c; c = c->get_next_sibling())
                locate(*c);
            };
            locate(*main);
            if (stage == 2) {
              main->lookup_action("save-project")->activate();
              auto *parent = objects->get_parent();
              while (parent &&
                     !dynamic_cast<inde::ui::WritingWorkspace *>(parent))
                parent = parent->get_parent();
              auto *writing =
                  dynamic_cast<inde::ui::WritingWorkspace *>(parent);
              need(writing, "Escrita ausente");
              click(*main, "writing-library");
              writing->reveal_document(id);
              main->lookup_action("document-objects")->activate();
              main->queue_allocate();
              main->queue_draw();
              settle = 3;
              stage = 3;
              return true;
            }
            if (!objects) {
              std::function<void(Gtk::Widget &)> dump = [&](Gtk::Widget &w) {
                if (auto *l = dynamic_cast<Gtk::Label *>(&w))
                  std::cerr << l->get_text() << "\n";
                for (auto *c = w.get_first_child(); c;
                     c = c->get_next_sibling())
                  dump(*c);
              };
              for (auto *w : app->get_windows())
                dump(*w);
              throw std::runtime_error("Editor GTK ausente, etapa " +
                                       std::to_string(stage) + ", passos " +
                                       std::to_string(steps.size()));
            }
            if (Probe::busy(*objects))
              return true;
            if (!Probe::error(*objects).empty())
              throw std::runtime_error("Operação GTK recusada: " +
                                       Probe::error(*objects));
            if (stage == 3) {
              if (settle-- > 0)
                return true;
              need(!state().at("dirty").as_boolean(),
                   "Salvar projeto não confirmou composição");
              need(state().at("equations").as_array().size() == 3,
                   "Equações perdidas na reabertura");
              need(value() == "80", "Fórmula perdida");
              need(math_widgets(*objects) >= 3, "Desenho nativo perdido");
              capture_native(*main, picture.parent_path() / "gtk-reaberto.png");
              stage = 4;
              return true;
            }
            if (stage == 4) {
              std::ofstream out(picture.parent_path() / "gtk-smoke.json");
              out << "{\"gtk_widgets\":true,\"webkit\":false,\"html\":false,"
                     "\"javascript\":false,\"native_cairo_math\":true,"
                     "\"equations\":3,\"draft_close_guard\":true,\"remote_"
                     "change_undo_redo\":true,\"table_style_structure\":true,"
                     "\"integrated_writing\":true,\"tools_collapsed\":true,"
                     "\"context_menus\":true,\"visibility_lens\":true,\"save_"
                     "reopen\":true,\"source_"
                     "document\":\""
                  << id << "\"}\n";
              for (auto *window : app->get_windows())
                need(window == main.get(), "Editor abriu outra janela");
              result = 0;
              app->quit();
              return false;
            }
            if (!steps.empty()) {
              auto step = steps.front();
              steps.pop_front();
              step();
            }
            return true;
          } catch (const std::exception &e) {
            std::cerr << "stage=" << stage << " remaining=" << steps.size()
                      << " " << e.what() << '\n';
            app->quit();
            return false;
          }
        },
        180);
  });
  app->run();
  capture_frame.disconnect();
  capture_pending = false;
  main.reset();
  if (result == 0) {
    inde::application::ProjectService check;
    check.open(project);
    const auto saved = *check.writing().document(id);
    need(saved.content == revised_body,
         "Editor original não gravou o texto compartilhado");
    need(!saved.formatting.empty(), "Formatação original perdida");
    need(saved.anchors.size() == 1 &&
             saved.anchors[0].label == "Segundo parágrafo",
         "Âncora original perdida");
    need(saved.paragraphs.rows[0].id == paragraph_ids[0],
         "Identidade original perdida");
    std::ifstream capture(project / "documents/composed" / id /
                          "origem-documento.json");
    const std::string frozen((std::istreambuf_iterator<char>(capture)), {});
    need(frozen == original, "Proveniência original alterada");
    std::cout << "Native GTK/Cairo: real widgets, no WebKit/HTML/JavaScript, "
                 "math, table, formula, draft protection, save/reopen and "
                 "owner preservation OK\n";
  }
  return result;
}
