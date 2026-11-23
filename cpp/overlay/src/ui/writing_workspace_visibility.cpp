#include "inde/ui/text_coordinates.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/project.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <algorithm>
namespace inde::ui {
namespace {
std::string style_name(project::DocumentTextStyle s) {
  using S = project::DocumentTextStyle;
  switch (s) {
  case S::Bold:
    return "Negrito";
  case S::Italic:
    return "Itálico";
  case S::Underline:
    return "Sublinhado";
  case S::Strikethrough:
    return "Tachado";
  case S::Heading:
    return "Título";
  case S::Subheading:
    return "Subtítulo";
  case S::Quote:
    return "Citação";
  case S::Heading2:
    return "Título 2";
  case S::Heading3:
    return "Título 3";
  case S::Heading4:
    return "Título 4";
  case S::Heading5:
    return "Título 5";
  case S::Heading6:
    return "Título 6";
  case S::AlignLeft:
    return "Alinhado à esquerda";
  case S::AlignCenter:
    return "Centralizado";
  case S::AlignRight:
    return "Alinhado à direita";
  case S::AlignJustify:
    return "Justificado";
  case S::FontFamily:
    return "Família da fonte";
  case S::FontSize:
    return "Tamanho da fonte";
  case S::Foreground:
    return "Cor do texto";
  }
  return {};
}
std::string
rule_name(const project::VisibilityRule &r, const project::Document &d,
          const std::vector<project::AnnotationTypeDefinition> &types) {
  using K = project::VisibilityRuleKind;
  if (r.kind == K::Paragraph) {
    for (std::size_t i = 0; i < d.paragraphs.rows.size(); ++i)
      if (d.paragraphs.rows[i].id == r.key)
        return "Parágrafo " + std::to_string(i + 1);
    return "Parágrafo indisponível/retirado";
  }
  if (r.kind == K::Resma) {
    for (const auto &x : d.resmas)
      if (x.id == r.key)
        return x.label;
    return "Resma indisponível";
  }
  if (r.kind == K::Anchor) {
    for (const auto &x : d.anchors)
      if (x.id == r.key)
        return x.label;
    return "Trecho marcado indisponível";
  }
  if (r.kind == K::Annotation) {
    for (const auto &x : d.semantics.annotations)
      if (x.id == r.key) {
        for (const auto &a : d.anchors)
          if (a.id == x.anchor_id)
            return a.label + " · " + x.status;
        return "Marcação documental · " + x.status;
      }
    return "Marcação indisponível";
  }
  if (r.kind == K::SemanticType) {
    for (const auto &x : types)
      if (x.key == r.key)
        return x.label;
    return "Tipo sem definição disponível";
  }
  return style_name(project::document_text_style_from_string(r.key));
}
auto profile(project::Document &d, const std::string &id) {
  auto i =
      std::find_if(d.visibility_profiles.begin(), d.visibility_profiles.end(),
                   [&](const auto &p) { return p.id == id; });
  if (i == d.visibility_profiles.end())
    throw std::runtime_error("Perfil não está mais no Documento");
  return i;
}
Gtk::Box *scroll_rows(OverlayDialog &d) {
  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll->set_min_content_height(180);
  scroll->set_max_content_height(340);
  auto *rows = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
  scroll->set_child(*rows);
  d.get_content_area()->append(*scroll);
  return rows;
}
} // namespace
void WritingWorkspace::build_visibility_ui() {
  auto *hide = Gtk::make_managed<Gtk::Button>(
      "Experimentar leitura sem os blocos escolhidos");
  hide->set_tooltip_text("Cria um novo perfil de trabalho sem os blocos da "
                         "seleção/cursor. Não apaga nem troca folhas; pode "
                         "retirar os critérios para mostrá-los de novo.");
  hide->signal_clicked().connect([this] {
    document_popover_.popdown();
    hide_selected_reading_blocks();
  });
  document_menu_.append(*hide);
  auto *button = Gtk::make_managed<Gtk::Button>("Visibilidade e lentes…");
  button->signal_clicked().connect([this] {
    document_popover_.popdown();
    show_visibility_profiles();
  });
  document_menu_.append(*button);
  hidden_text_tag_ = editor_.get_buffer()->create_tag("inde-view-hidden");
  hidden_text_tag_->property_invisible() = true;
  // GTK's insert/delete API revalidates the caller's mutable iterators. The
  // C++ signal wrapper exposes copies, so interception here must keep that
  // native contract after a canonical transaction changes the same buffer.
  using Owner = std::pair<WritingWorkspace *, std::weak_ptr<int>>;
  const auto release = +[](gpointer p, GClosure *) { delete static_cast<Owner *>(p); };
  auto buffer = editor_.get_buffer();
  g_signal_connect_data(buffer->gobj(), "insert-text", G_CALLBACK(+[](GtkTextBuffer *b, GtkTextIter *at, gchar *s, gint n, gpointer p) {
    auto &guard=*static_cast<Owner *>(p); if(guard.second.expired())return;
    auto &w=*guard.first;
    if(w.loading_editor_ || w.restoring_undo_)return;
    if(w.intercept_visibility_edit(gtk_text_iter_get_offset(at),gtk_text_iter_get_offset(at),std::string(s,n))) {
      gtk_text_buffer_get_iter_at_mark(b,at,gtk_text_buffer_get_insert(b));
      g_signal_stop_emission_by_name(b,"insert-text");
    }
  }), new Owner{this,clipboard_lifetime_}, release, GConnectFlags(0));
  g_signal_connect_data(buffer->gobj(), "delete-range", G_CALLBACK(+[](GtkTextBuffer *b, GtkTextIter *a, GtkTextIter *z, gpointer p) {
    auto &guard=*static_cast<Owner *>(p); if(guard.second.expired())return;
    auto &w=*guard.first;
    if(w.loading_editor_ || w.restoring_undo_)return;
    if(w.intercept_visibility_edit(gtk_text_iter_get_offset(a),gtk_text_iter_get_offset(z),"")) {
      gtk_text_buffer_get_iter_at_mark(b,a,gtk_text_buffer_get_insert(b));*z=*a;
      g_signal_stop_emission_by_name(b,"delete-range");
    }
  }), new Owner{this,clipboard_lifetime_}, release, GConnectFlags(0));
}
void WritingWorkspace::hide_selected_reading_blocks() {
  if (!current_document_)
    return;
  try {
    flush_changes();
    auto next = capture_edit_state();
    const auto blocks = visible_reading_blocks();
    if (blocks.empty()) {
      show_document_paragraphs();
      return;
    }
    const auto a = std::min(next.selection.cursor, next.selection.bound),
               b = std::max(next.selection.cursor, next.selection.bound);
    project::VisibilityProfile p{
        project::new_uuid(),
        "Leitura experimental " +
            std::to_string(next.document.visibility_profiles.size() + 1),
        true,
        {},
        {}};
    for (const auto &block : blocks) {
      const bool chosen = a == b ? (block.start <= a && a <= block.end)
                                 : (block.start < b && a < block.end);
      if (chosen)
        for (auto i = block.first; i <= block.last; ++i)
          p.exclude.push_back({project::VisibilityRuleKind::Paragraph,
                               next.document.paragraphs.rows[i].id});
    }
    if (p.exclude.empty())
      throw std::runtime_error("Coloque o cursor em um bloco ou selecione os "
                               "blocos que deseja ocultar");
    next.document.visibility_profiles.push_back(p);
    project::validate(next.document);
    apply_edit_transaction(std::move(next),
                           "Criar leitura sem os blocos escolhidos");
    open_visibility_lens(p.id);
  } catch (const std::exception &e) {
    show_error("Não foi possível experimentar a leitura", e);
  }
}
void WritingWorkspace::open_visibility_lens(std::string id) {
  try {
    flush_changes();
    require_completed_composition();
    (void)application::project_visibility(capture_editor_state(),id);
    active_visibility_id_=std::move(id);
    invalidate_clipboard();
    refresh_editor_visibility();
    schedule_find();
    focus_common_sheet();
  } catch (const std::exception &e) {
    show_error("Não foi possível abrir a lente", e);
  }
}
void WritingWorkspace::show_visibility_profiles() {
  auto *owner = owner_window();
  if (!owner || !current_document_)
    return;
  try {
    flush_changes();
    const auto observed = capture_edit_state();
    auto *d =
        new OverlayDialog("Visibilidade e lentes de trabalho", *owner, true);
    d->set_secondary_text(
        "Perfis mudam somente o que se vê. Ocultar não apaga, não troca a "
        "folha da resma e não escolhe a saída editorial. O Documento integral "
        "continua disponível.");
    auto *rows = scroll_rows(*d);
    for (const auto &p : observed.document.visibility_profiles) {
      auto *button = Gtk::make_managed<Gtk::Button>(p.name);
      button->signal_clicked().connect(sigc::track_obj(
          [this, d, id = p.id] {
            d->hide();
            Glib::signal_idle().connect_once(sigc::track_obj(
                [this, d, id] {
                  delete d;
                  show_visibility_profile(id);
                },
                *this));
          },
          *this));
      rows->append(*button);
    }
    if (observed.document.visibility_profiles.empty())
      rows->append(*Gtk::make_managed<Gtk::Label>(
          "Nenhum perfil. Comece com uma leitura completa ou mostre só os "
          "tipos de trecho escolhidos."));
    d->add_button("Fechar", Gtk::ResponseType::CLOSE);
    d->add_button("Novo perfil", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect(sigc::track_obj(
        [this](int r) {
          if (r == Gtk::ResponseType::ACCEPT)
            Glib::signal_idle().connect_once(
                sigc::track_obj([this] { edit_visibility_profile(); }, *this));
        },
        *this));
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível consultar os perfis", e);
  }
}
void WritingWorkspace::edit_visibility_profile(std::string id) {
  auto *owner = owner_window();
  if (!owner || !current_document_)
    return;
  try {
    flush_changes();
    auto observed = capture_edit_state();
    project::VisibilityProfile p;
    if (id.empty())
      p = {project::new_uuid(), "Leitura experimental", true, {}, {}};
    else
      p = *profile(observed.document, id);
    auto *d = new OverlayDialog(id.empty() ? "Novo perfil de trabalho"
                                           : "Nome e base do perfil",
                                *owner, true);
    auto *name = Gtk::make_managed<Gtk::Entry>();
    name->set_text(p.name);
    append_labeled_form_field(*d->get_content_area(), "Nome do perfil", *name);
    auto *all =
        Gtk::make_managed<Gtk::CheckButton>("Começar com o Documento inteiro");
    all->set_active(p.include_all);
    d->get_content_area()->append(*all);
    d->set_secondary_text(
        "Com base integral, os critérios ocultam trechos. Sem ela, a lente "
        "mostra a união dos critérios de inclusão, menos as exclusões. Nenhum "
        "conteúdo é removido.");
    d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    d->add_button("Guardar perfil", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect(sigc::track_obj(
        [this, d, observed, p, id, name, all](int r) mutable {
          if (r != Gtk::ResponseType::ACCEPT)
            return;
          try {
            auto next = capture_edit_state();
            if (!project::same_document_state(next.document, observed.document,
                                              false))
              throw std::runtime_error("A origem mudou; reabra o perfil");
            p.name = name->get_text();
            p.include_all = all->get_active();
            if (id.empty())
              next.document.visibility_profiles.push_back(p);
            else
              *profile(next.document, id) = p;
            project::validate(next.document);
            apply_edit_transaction(std::move(next),
                                   "Guardar perfil de trabalho");
            Glib::signal_idle().connect_once(sigc::track_obj(
                [this, id = p.id] { show_visibility_profile(id); }, *this));
          } catch (const std::exception &e) {
            d->keep_open_on_response();
            signal_status_message_.emit(e.what());
          }
        },
        *this));
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível guardar o perfil", e);
  }
}
void WritingWorkspace::show_visibility_profile(std::string id,
                                               std::size_t offset) {
  auto *owner = owner_window();
  if (!owner || !current_document_)
    return;
  try {
    flush_changes();
    auto observed = capture_edit_state();
    const auto p = *profile(observed.document, id);
    const auto preview = application::project_visibility(observed.document, id);
    auto *d = new OverlayDialog(p.name, *owner, true);
    d->set_secondary_text(
        std::string(p.include_all
                        ? "Base: Documento integral. "
                        : "Base: somente a união dos critérios mostrados. ") +
        std::to_string(project::utf8_character_count(preview.text)) +
        " caracteres exibidos; " + std::to_string(preview.diagnostics.size()) +
        " critério(s) sem trecho disponível/confiável. O texto omitido "
        "continua na origem.");
    auto *rows = scroll_rows(*d);
    std::vector<std::pair<bool, project::VisibilityRule>> rules;
    for (const auto &r : p.include)
      rules.emplace_back(true, r);
    for (const auto &r : p.exclude)
      rules.emplace_back(false, r);
    offset =
        std::min(offset / 50 * 50,
                 rules.empty() ? std::size_t{0} : (rules.size() - 1) / 50 * 50);
    const auto types = service_.writing().semantic_types();
    for (auto i = offset; i < std::min(offset + 50, rules.size()); ++i) {
      const auto [include, rule] = rules[i];
      auto *button = Gtk::make_managed<Gtk::Button>(
          std::string(include ? "Mostrar · " : "Ocultar · ") +
          rule_name(rule, observed.document, types) + " — retirar critério");
      button->set_tooltip_text(project::display_name(rule.kind) + " · " +
                               rule.key);
      button->signal_clicked().connect(sigc::track_obj(
          [this, d, id, include, rule, offset] {
            try {
              auto next = capture_edit_state();
              auto p = profile(next.document, id);
              auto &v = include ? p->include : p->exclude;
              std::erase(v, rule);
              apply_edit_transaction(std::move(next),
                                     "Retirar critério de visibilidade");
              d->hide();
              Glib::signal_idle().connect_once(sigc::track_obj(
                  [this, d, id, offset] {
                    delete d;
                    show_visibility_profile(id, offset);
                  },
                  *this));
            } catch (const std::exception &e) {
              signal_status_message_.emit(e.what());
            }
          },
          *this));
      rows->append(*button);
    }
    if (rules.empty())
      rows->append(*Gtk::make_managed<Gtk::Label>(
          "Adicione os trechos que deseja mostrar ou ocultar."));
    auto *bar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    for (bool include : {true, false}) {
      auto *button = Gtk::make_managed<Gtk::Button>(include ? "Mostrar apenas…"
                                                            : "Ocultar…");
      button->signal_clicked().connect(sigc::track_obj(
          [this, d, id, include] {
            d->hide();
            Glib::signal_idle().connect_once(sigc::track_obj(
                [this, d, id, include] {
                  delete d;
                  pick_visibility_rule(id, include);
                },
                *this));
          },
          *this));
      bar->append(*button);
    }
    auto *rename = Gtk::make_managed<Gtk::Button>("Nome e base…");
    rename->signal_clicked().connect(sigc::track_obj(
        [this, d, id] {
          d->hide();
          Glib::signal_idle().connect_once(sigc::track_obj(
              [this, d, id] {
                delete d;
                edit_visibility_profile(id);
              },
              *this));
        },
        *this));
    bar->append(*rename);
    d->get_content_area()->append(*bar);
    if (rules.size() > 50)
      for (int step : {-1, 1}) {
        auto *b = Gtk::make_managed<Gtk::Button>(
            step < 0 ? "Critérios anteriores" : "Próximos critérios");
        b->set_sensitive(step < 0 ? offset > 0 : offset + 50 < rules.size());
        b->signal_clicked().connect(sigc::track_obj(
            [this, d, id, offset, step] {
              d->hide();
              Glib::signal_idle().connect_once(sigc::track_obj(
                  [this, d, id, offset, step] {
                    delete d;
                    show_visibility_profile(id, step < 0 ? offset - 50
                                                         : offset + 50);
                  },
                  *this));
            },
            *this));
        d->get_content_area()->append(*b);
      }
    d->add_button("Fechar", Gtk::ResponseType::CLOSE);
    d->add_button("Excluir só o perfil", Gtk::ResponseType::REJECT);
    d->add_button("Aplicar nesta área de escrita", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect(sigc::track_obj(
        [this, d, id](int r) {
          if (r == Gtk::ResponseType::ACCEPT)
            Glib::signal_idle().connect_once(sigc::track_obj(
                [this, id] { open_visibility_lens(id); }, *this));
          if (r == Gtk::ResponseType::REJECT)
            try {
              auto next = capture_edit_state();
              std::erase_if(next.document.visibility_profiles,
                            [&](const auto &p) { return p.id == id; });
              apply_edit_transaction(std::move(next),
                                     "Excluir somente perfil de trabalho");
            } catch (const std::exception &e) {
              d->keep_open_on_response();
              signal_status_message_.emit(e.what());
            }
        },
        *this));
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível consultar o perfil", e);
  }
}
void WritingWorkspace::pick_visibility_rule(std::string id, bool include,
                                            project::VisibilityRuleKind kind,
                                            std::size_t offset) {
  auto *owner = owner_window();
  if (!owner || !current_document_)
    return;
  try {
    flush_changes();
    const auto observed = capture_edit_state();
    auto *d = new OverlayDialog(include ? "Mostrar apenas os trechos escolhidos"
                                        : "Ocultar trechos sem apagar",
                                *owner, true);
    d->set_secondary_text(
        "Escolher uma inclusão troca a base para somente os critérios "
        "mostrados. Exclusões sempre prevalecem. Marcações semânticas "
        "arquivadas ou sem ancoragem confiável não fornecem texto.");
    auto *category = Gtk::make_managed<Gtk::ComboBoxText>();
    for (auto k : {project::VisibilityRuleKind::Paragraph,
                   project::VisibilityRuleKind::Resma,
                   project::VisibilityRuleKind::Anchor,
                   project::VisibilityRuleKind::SemanticType,
                   project::VisibilityRuleKind::Annotation,
                   project::VisibilityRuleKind::Style})
      category->append(project::to_string(k), project::display_name(k));
    category->set_active_id(project::to_string(kind));
    append_labeled_form_field(*d->get_content_area(), "Camada ou marcação",
                              *category);
    category->signal_changed().connect(sigc::track_obj(
        [this, d, id, include, category] {
          const auto k =
              project::visibility_rule_kind(category->get_active_id());
          d->hide();
          Glib::signal_idle().connect_once(sigc::track_obj(
              [this, d, id, include, k] {
                delete d;
                pick_visibility_rule(id, include, k, 0);
              },
              *this));
        },
        *this));
    auto *rows = scroll_rows(*d);
    std::size_t total = 0;
    auto add = [&](const std::string &key, const std::string &label) {
      const auto i = total++;
      if (i < offset || i >= offset + 50)
        return;
      auto *button = Gtk::make_managed<Gtk::Button>(label);
      button->set_tooltip_text(key);
      button->signal_clicked().connect(sigc::track_obj(
          [this, d, id, include, kind, key] {
            try {
              auto next = capture_edit_state();
              auto p = profile(next.document, id);
              auto &v = include ? p->include : p->exclude;
              const project::VisibilityRule r{kind, key};
              if (std::find(v.begin(), v.end(), r) == v.end())
                v.push_back(r);
              if (include)
                p->include_all = false;
              project::validate(next.document);
              apply_edit_transaction(std::move(next),
                                     "Ajustar visibilidade sem apagar texto");
              d->hide();
              Glib::signal_idle().connect_once(sigc::track_obj(
                  [this, d, id] {
                    delete d;
                    show_visibility_profile(id);
                  },
                  *this));
            } catch (const std::exception &e) {
              signal_status_message_.emit(e.what());
            }
          },
          *this));
      rows->append(*button);
    };
    using K = project::VisibilityRuleKind;
    const auto &doc = observed.document;
    if (kind == K::Paragraph)
      for (std::size_t i = 0; i < doc.paragraphs.rows.size(); ++i) {
        const auto &p = doc.paragraphs.rows[i];
        const auto excerpt =
            total >= offset && total < offset + 50
                ? editor_.get_buffer()
                      ->get_text(
                          editor_.get_buffer()->get_iter_at_offset(gtk_text_offset(p.start)),
                          editor_.get_buffer()->get_iter_at_offset(gtk_text_offset(
                              std::min(p.end, p.start + 52))))
                      .raw()
                : "";
        add(p.id, "Parágrafo " + std::to_string(i + 1) + " · " + excerpt);
      }
    else if (kind == K::Resma)
      for (const auto &r : doc.resmas)
        add(r.id, r.label);
    else if (kind == K::Anchor)
      for (const auto &a : doc.anchors)
        add(a.id, a.label);
    else if (kind == K::Annotation)
      for (const auto &a : doc.semantics.annotations)
        add(a.id, a.type_key + " · " + a.status);
    else if (kind == K::SemanticType)
      for (const auto &t : service_.writing().semantic_types())
        add(t.key, t.label + " · " + t.key);
    else if (kind == K::Style)
      for (auto s : {project::DocumentTextStyle::Bold,
                     project::DocumentTextStyle::Italic,
                     project::DocumentTextStyle::Underline,
                     project::DocumentTextStyle::Strikethrough,
                     project::DocumentTextStyle::Heading,
                     project::DocumentTextStyle::Subheading,
                     project::DocumentTextStyle::Quote,
                     project::DocumentTextStyle::Heading2,
                     project::DocumentTextStyle::Heading3,
                     project::DocumentTextStyle::Heading4,
                     project::DocumentTextStyle::Heading5,
                     project::DocumentTextStyle::Heading6,
                     project::DocumentTextStyle::AlignLeft,
                     project::DocumentTextStyle::AlignCenter,
                     project::DocumentTextStyle::AlignRight,
                     project::DocumentTextStyle::AlignJustify,
                     project::DocumentTextStyle::FontFamily,
                     project::DocumentTextStyle::FontSize,
                     project::DocumentTextStyle::Foreground})
        add(project::to_string(s), style_name(s));
    if (!total)
      rows->append(*Gtk::make_managed<Gtk::Label>(
          "Nenhum item nesta camada. Parágrafos legados precisam primeiro ser "
          "identificados no Documento integral."));
    if (total > 50)
      for (int step : {-1, 1}) {
        auto *b = Gtk::make_managed<Gtk::Button>(step < 0 ? "Página anterior"
                                                          : "Próxima página");
        b->set_sensitive(step < 0 ? offset > 0 : offset + 50 < total);
        b->signal_clicked().connect(sigc::track_obj(
            [this, d, id, include, kind, offset, step] {
              d->hide();
              Glib::signal_idle().connect_once(sigc::track_obj(
                  [this, d, id, include, kind, offset, step] {
                    delete d;
                    pick_visibility_rule(id, include, kind,
                                         step < 0 ? offset - 50 : offset + 50);
                  },
                  *this));
            },
            *this));
        d->get_content_area()->append(*b);
      }
    d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível escolher o critério", e);
  }
}
} // namespace inde::ui
