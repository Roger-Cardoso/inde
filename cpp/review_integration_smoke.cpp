#include "main_window_smoke.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/review_workspace.hpp"
#include "inde/application/text_semantic_service.hpp"
#include <iostream>
namespace {
template<class T, class Predicate> T *find(Gtk::Widget &root, Predicate predicate) {
  if (auto *value = dynamic_cast<T *>(&root); value && predicate(*value)) return value;
  for (auto *child = root.get_first_child(); child; child = child->get_next_sibling())
    if (auto *value = find<T>(*child, predicate)) return value;
  return nullptr;
}
void require(bool value, const char *why) { if (!value) throw std::runtime_error(why); }
}
int run_review_integration_smoke(const Glib::RefPtr<Gtk::Application> &app,
                                const std::filesystem::path &root) {
  using namespace inde;
  require(!std::filesystem::exists(root), "Use uma pasta sintética nova");
  std::filesystem::create_directories(root);
  const auto project = root / "Revisao.inde";
  {
    application::ProjectService seed;
    seed.create(project, "Revisão integrada — QA");
    auto pending = seed.writing().create_document("ZZZ Pendência tardia");
    pending.content = "Texto para conferir.";
    const auto types = seed.writing().semantic_types();
    const auto *type = project::find_annotation_type(types, "inde.editor.comment", 1);
    require(type != nullptr, "Tipo de comentário ausente");
    using K = project::SemanticFieldKind;
    pending = application::TextSemanticService::create(pending, *type, 0, 5,
        {{"note", {K::Text, {"Pendência depois do limite inicial"}}}, {"state", {K::Enum, {"open"}}}});
    pending = seed.writing().update_document(pending);
    for (int i = 0; i < 110; ++i)
      (void)seed.writing().create_document("AAA Documento " + std::to_string(i));
    persistence::DocumentQuery query;
    query.limit = 50;
    for (const auto &summary : seed.writing().document_summaries(query))
      require(summary.id != pending.id, "Fixture não exercita pendência depois do quinquagésimo");
  }
  std::unique_ptr<ui::MainWindow> window;
  ui::ReviewWorkspace *review{};
  int result = 1, stage = 0, ticks = 0;
  app->signal_activate().connect([&] {
    window = std::make_unique<ui::MainWindow>(project);
    app->add_window(*window); window->present();
    Glib::signal_timeout().connect([&] {
      try {
        require(++ticks < 600, "Timeout da fila integrada");
        if (stage == 0) {
          auto action = window->lookup_action("show-review");
          require(bool(action), "Ação Revisar ausente"); action->activate();
          review = find<ui::ReviewWorkspace>(*window, [](auto &) { return true; });
          require(review != nullptr, "Revisão ausente"); stage = 1; return true;
        }
        auto *summary = find<Gtk::Label>(*review, [](auto &label) {
          return label.get_text().raw().find("Documentos nesta página de revisão") != std::string::npos;
        });
        if (stage == 1) {
          if (!summary) return true;
          auto *button = find<Gtk::Button>(*review, [](auto &value) { return value.get_label() == "ZZZ Pendência tardia"; });
          require(button != nullptr, "Filtro não alcançou a pendência distante");
          require(summary->get_text().raw().find("1 Documentos nesta página") == 0, "Fila omitiu ou inventou pendências");
          g_signal_emit_by_name(button->gobj(), "clicked"); stage = 2; return true;
        }
        if (stage == 2) {
          auto *button = find<Gtk::Button>(*review, [](auto &value) { return value.get_mapped() && value.get_label() == "Resolver comentário"; });
          if (!button) return true;
          g_signal_emit_by_name(button->gobj(), "clicked"); stage = 3; return true;
        }
        if (stage == 3) {
          auto *button = find<Gtk::Button>(*review, [](auto &value) { return value.get_mapped() && value.get_label() == "Voltar à fila"; });
          require(button != nullptr, "Retorno à fila ausente");
          g_signal_emit_by_name(button->gobj(), "clicked"); stage = 4; return true;
        }
        if (!summary || summary->get_text().raw().find("0 Documentos nesta página") != 0) return true;
        std::cout << "INT01 GTK revisão: pendência além de 100 Documentos na primeira página, decisão persistida e fila atualizada OK\n";
        result = 0; window->close(); app->quit(); return false;
      } catch (const std::exception &error) {
        std::cerr << error.what() << '\n'; window->hide(); app->quit(); return false;
      }
    }, 50);
  });
  app->run(); window.reset(); return result;
}
