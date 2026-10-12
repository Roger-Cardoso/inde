#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/composition_page.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include <algorithm>
#include <cmath>
namespace inde::ui {
namespace {
void label(Gtk::Label &v) {
  v.set_wrap(true);
  v.set_xalign(0);
  v.set_max_width_chars(90);
}
void area(Gtk::TextView &v, Gtk::ScrolledWindow &scroll,
          bool editable = false) {
  v.set_editable(editable);
  v.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  v.add_css_class("writing-editor");
  v.set_left_margin(20);
  v.set_right_margin(20);
  v.set_top_margin(12);
  v.set_bottom_margin(12);
  scroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll.set_child(v);
  scroll.set_vexpand(true);
  scroll.set_hexpand(true);
}
void clear(Gtk::ListBox &box) {
  while (auto *c = box.get_first_child())
    box.remove(*c);
}
std::string mode(const experimental::TextPlacement &p) {
  switch (p.mode) {
  case experimental::PlacementMode::Live:
    return "acompanha o bloco";
  case experimental::PlacementMode::Pinned:
    return "versão fixada";
  case experimental::PlacementMode::Forked:
    return "bloco independente";
  case experimental::PlacementMode::ReferenceOnly:
    return "referência, sem texto";
  }
  return {};
}
} // namespace
CompositionPage::CompositionPage(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), service_(service) {
  set_hexpand(true);
  set_vexpand(true);
  title_.add_css_class("title-2");
  title_.set_hexpand(true);
  title_.set_xalign(0);
  title_.set_ellipsize(Pango::EllipsizeMode::END);
  header_.append(back_);
  header_.append(title_);
  header_.append(all_);
  header_.append(more_);
  header_.add_css_class("toolbar-surface");
  append(header_);
  pages_.set_hhomogeneous(false);
  pages_.set_vhomogeneous(false);
  pages_.set_vexpand(true);
  append(pages_);
  for (auto *l : {&message_, &hint_, &edit_info_, &reading_info_, &output_info_,
                  &origin_info_})
    label(*l);
  append(message_);
  name_.set_placeholder_text("Nome da nova Montagem de texto");
  name_.set_max_length(128);
  set_accessible_label(name_, "Nome da Montagem de texto");
  auto *new_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  name_.set_hexpand(true);
  new_row->append(name_);
  new_row->append(create_);
  queue_.append(*new_row);
  hint_.set_text(
      "Monte versões e trechos sem alterar os Documentos de origem. A leitura "
      "é derivada; rascunhos não aplicados ficam fora dela.");
  queue_.append(hint_);
  compositions_.set_selection_mode(Gtk::SelectionMode::NONE);
  queue_scroll_.set_child(compositions_);
  queue_scroll_.set_vexpand(true);
  queue_.append(queue_scroll_);
  auto *pagination =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  pagination->append(previous_);
  pagination->append(next_);
  queue_.append(*pagination);
  auto *order_actions =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  order_actions->append(add_);
  order_actions->append(read_);
  order_.append(*order_actions);
  occurrences_.set_selection_mode(Gtk::SelectionMode::NONE);
  order_scroll_.set_child(occurrences_);
  order_scroll_.set_vexpand(true);
  order_.append(order_scroll_);
  edit_.append(edit_back_);
  edit_info_scroll_.set_policy(Gtk::PolicyType::NEVER,
                               Gtk::PolicyType::AUTOMATIC);
  edit_info_scroll_.set_max_content_height(130);
  edit_info_scroll_.set_propagate_natural_height(true);
  edit_info_scroll_.set_child(edit_info_);
  edit_.append(edit_info_scroll_);
  bound_choice_width(versions_);
  edit_.append(versions_);
  area(editor_, edit_scroll_, true);
  auto position_keys = Gtk::EventControllerKey::create();
  position_keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  position_keys->signal_key_pressed().connect(
      [this](guint, guint, Gdk::ModifierType) {
        position_restore_.disconnect();
        return false;
      },
      false);
  editor_.add_controller(position_keys);
  auto position_click = Gtk::GestureClick::create();
  position_click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  position_click->signal_pressed().connect(
      [this](int, double, double) { position_restore_.disconnect(); });
  editor_.add_controller(position_click);
  auto position_scroll = Gtk::EventControllerScroll::create();
  position_scroll->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
  position_scroll->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  position_scroll->signal_scroll().connect(
      [this](double, double) {
        position_restore_.disconnect();
        return false;
      },
      false);
  editor_.add_controller(position_scroll);
  editor_.set_editable(false);
  edit_.append(edit_scroll_);
  set_accessible_label(editor_, "Rascunho do bloco");
  auto *edit_actions =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  edit_actions->append(apply_);
  edit_actions->append(discard_);
  edit_.append(*edit_actions);
  reading_.append(reading_back_);
  reading_.append(reading_info_);
  reading_.append(reading_profile_);
  area(reading_text_, reading_scroll_);
  reading_.append(reading_scroll_);
  set_accessible_label(reading_text_, "Leitura da Montagem de texto");
  import_.append(import_back_);
  sources_ = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar Documento de origem",
      [this](const std::string &search, std::size_t limit) {
        persistence::DocumentQuery q;
        q.search = search;
        q.limit = limit;
        std::vector<IncrementalSelection> out;
        if (service_.current())
          for (const auto &d : service_.writing().document_summaries(q))
            out.push_back({d.id, d.title,
                           "Origem será preservada, sem vínculo de edição"});
        return out;
      });
  import_.append(*sources_);
  area(import_text_, import_scroll_);
  import_.append(import_scroll_);
  set_accessible_label(import_text_, "Origem para importar");
  auto *imports = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  imports->append(import_whole_);
  imports->append(import_selection_);
  import_.append(*imports);
  output_.append(output_back_);
  output_.append(output_info_);
  area(output_text_, output_scroll_);
  output_.append(output_scroll_);
  set_accessible_label(output_text_, "Leitura preparada para saída");
  label(output_losses_);
  output_losses_.set_selectable(true);
  output_details_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                    Gtk::PolicyType::AUTOMATIC);
  output_details_scroll_.set_min_content_height(90);
  output_details_scroll_.set_max_content_height(140);
  output_details_scroll_.set_propagate_natural_height(true);
  output_details_scroll_.set_child(output_losses_);
  output_details_.set_child(output_details_scroll_);
  output_.append(output_details_);
  output_.append(acknowledged_);
  auto *outputs = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  outputs->append(materialize_);
  outputs->append(export_);
  output_.append(*outputs);
  origin_.append(origin_back_);
  origin_.append(origin_info_);
  area(origin_text_, origin_scroll_);
  origin_.append(origin_scroll_);
  set_accessible_label(origin_text_, "Origem congelada do bloco");
  for (auto [page, key] :
       std::initializer_list<std::pair<Gtk::Box *, const char *>>{
           {&queue_, "queue"},
           {&order_, "order"},
           {&edit_, "edit"},
           {&reading_, "reading"},
           {&import_, "import"},
           {&output_, "output"},
           {&origin_, "origin"}})
    pages_.add(*page, key);
  configure_overflow_button(
      more_,
      {{"Preparar saída independente…",
        [this] { attempt([&] { prepare(); }); }},
       {"Planejamento deste uso do bloco…",
        [this] { narrative_uses_signal_.emit(); }},
       {"Mover uso do bloco acima", [this] { attempt([&] { move(-1); }); }},
       {"Mover uso do bloco abaixo", [this] { attempt([&] { move(1); }); }},
       {"Reutilizar uso do bloco",
        [this] { attempt([&] { occurrence_command("repeat"); }); }},
       {"Manter esta versão neste uso",
        [this] { attempt([&] { occurrence_command("pin"); }); }},
       {"Acompanhar a versão em uso do bloco",
        [this] { attempt([&] { occurrence_command("follow"); }); }},
       {"Criar cópia independente para este uso",
        [this] { attempt([&] { occurrence_command("fork"); }); }},
       {"Referência sem texto",
        [this] { attempt([&] { occurrence_command("reference"); }); }},
       {"Ocultar / mostrar uso do bloco",
        [this] { attempt([&] { occurrence_command("hide"); }); }},
       {"Separar seleção / dividir no cursor",
        [this] { attempt([&] { split(); }); }},
       {"Criar alternativa de leitura…",
        [this] { attempt([&] { classify(true); }); }},
       {"Incluir em nova camada de leitura…",
        [this] { attempt([&] { classify(false); }); }},
       {"Ver origem congelada", [this] { attempt([&] { show_origin(); }); }},
       {"Retirar este uso da montagem",
        [this] { attempt([&] { occurrence_command("remove"); }); }},
       {"Recarregar do Projeto…", [this] { reload(); }}},
      "Operações da Montagem de texto / uso do bloco atual");
  back_.signal_clicked().connect([this] {
    attempt([&] {
      flush();
      back_signal_.emit();
    });
  });
  all_.signal_clicked().connect([this] {
    attempt([&] {
      flush();
      current_.reset();
      refresh();
    });
  });
  create_.signal_clicked().connect([this] {
    attempt([&] {
      current_ = service_.compositions().create(name_.get_text());
      name_.set_text("");
      show_order();
    });
  });
  previous_.signal_clicked().connect([this] {
    if (offset_ >= 30)
      offset_ -= 30;
    refresh();
  });
  next_.signal_clicked().connect([this] {
    offset_ += 30;
    refresh();
  });
  add_.signal_clicked().connect([this] { attempt([&] { show_import(); }); });
  read_.signal_clicked().connect([this] { attempt([&] { show_reading(); }); });
  for (auto *button : {&edit_back_, &reading_back_, &import_back_,
                       &output_back_, &origin_back_})
    button->signal_clicked().connect([this] {
      attempt([&] {
        flush();
        show_order();
      });
    });
  edited_ = editor_.get_buffer()->signal_changed().connect([this] {
    if (loading_ || !current_ || !editor_.get_editable())
      return;
    dirty_ = true;
    prepared_.reset();
    acknowledged_.set_active(false);
    message_.set_text("Rascunho em memória; aguardando salvamento. A leitura ainda usa a versão em uso na montagem.");
    debounce_.disconnect();
    debounce_ = Glib::signal_timeout().connect(
        [this] {
          attempt([&] { flush(); });
          return false;
        },
        800);
  });
  g_signal_connect(
      editor_.gobj(), "preedit-changed",
      G_CALLBACK(+[](GtkTextView *, const gchar *text, gpointer data) {
        static_cast<CompositionPage *>(data)->ime_ = text && *text;
      }),
      this);
  version_changed_ = versions_.signal_changed().connect([this] {
    if (!loading_)
      attempt([&] {
        position_restore_.disconnect();
        flush();
        display_version();
      });
  });
  apply_.signal_clicked().connect([this] {
    attempt([&] {confirm_draft(true);});
  });
  discard_.signal_clicked().connect([this] {
    attempt([&] {confirm_draft(false);});
  });
  sources_->signal_selection_changed().connect([this] {
    attempt([&] {
      source_.reset();
      if (auto id = sources_->selected_id())
        source_ = service_.writing().document(*id);
      import_text_.get_buffer()->set_text(source_ ? source_->content : "");
    });
  });
  import_whole_.signal_clicked().connect(
      [this] { attempt([&] { import_source(true); }); });
  import_selection_.signal_clicked().connect(
      [this] { attempt([&] { import_source(false); }); });
  acknowledged_.signal_toggled().connect([this] {
    const bool allow = prepared_ && acknowledged_.get_active();
    materialize_.set_sensitive(allow);
    export_.set_sensitive(allow);
  });
  materialize_.signal_clicked().connect([this] {
    attempt([&] {
      if (!prepared_)
        throw std::runtime_error("Prepare novamente a saída");
      project::Document d;
      try {
        d = service_.compositions().materialize(*prepared_,
                                                acknowledged_.get_active());
      } catch (...) {
        prepared_.reset();
        acknowledged_.set_active(false);
        throw;
      }
      prepared_.reset();
      acknowledged_.set_active(false);
      message_.set_text("Documento independente criado: " + d.title);
      open_signal_.emit(d.id);
    });
  });
  export_.signal_clicked().connect(
      [this] { attempt([&] { export_reading(); }); });
  pages_.set_visible_child("queue");
  all_.set_visible(false);
  more_.set_visible(false);
}
CompositionPage::~CompositionPage() {
  position_restore_.disconnect();
  debounce_.disconnect();
  edited_.disconnect();
  version_changed_.disconnect();
  g_signal_handlers_disconnect_by_data(editor_.gobj(), this);
}
ViewState CompositionPage::capture_view() {
  const auto buffer = editor_.get_buffer();
  return ViewState::Object{
      {"composition",
       view_id(current_ ? std::optional(current_->value.blocks.composition_id())
                        : std::nullopt)},
      {"occurrence", view_id(occurrence_.empty() ? std::nullopt
                                                 : std::optional(occurrence_))},
      {"version",
       view_id(versions_.get_active_id().empty()
                   ? std::nullopt
                   : std::optional(versions_.get_active_id().raw()))},
      {"page", pages_.get_visible_child_name().raw()},
      {"scroll", scroll_state(edit_scroll_.get_vadjustment()->get_value())},
      {"offset", static_cast<std::int64_t>(offset_)},
      {"cursor",
       static_cast<std::int64_t>(
           buffer->get_iter_at_mark(buffer->get_insert()).get_offset())},
      {"bound", static_cast<std::int64_t>(
                    buffer->get_iter_at_mark(buffer->get_selection_bound())
                        .get_offset())}};
}
void CompositionPage::restore_view(const ViewState &v) {
  position_restore_.disconnect();
  const auto id = view_id(v.at("composition"));
  const auto occurrence = view_id(v.at("occurrence"));
  const auto version = view_id(v.at("version"));
  const auto page = v.at("page").as_string();
  const auto cursor = view_offset(v.at("cursor")),
             bound = view_offset(v.at("bound"));
  const auto offset = view_offset(v.at("offset"));
  if (offset > 1000000 || offset % 30 ||
      (page != "queue" && page != "order" && page != "edit" &&
       page != "reading" && page != "import" && page != "origin" &&
       page != "output"))
    throw std::runtime_error("Contexto de Montagem de texto inválido");
  flush();
  offset_ = offset;
  prepared_.reset();
  acknowledged_.set_active(false);
  if (!id) {
    refresh();
    return;
  }
  open(*id);
  if (occurrence &&
      std::any_of(current_->value.blocks.placements().begin(),
                  current_->value.blocks.placements().end(),
                  [&](const auto &p) { return p.id == *occurrence; }))
    occurrence_ = *occurrence;
  if (page == "reading")
    show_reading();
  else if (page == "edit" && !occurrence_.empty()) {
    show_editor(occurrence_);
    const auto &revisions = current_->value.blocks.block(block_).revisions;
    if (version && std::any_of(revisions.begin(), revisions.end(),
                               [&](const auto &r) { return r.id == *version; }))
      versions_.set_active_id(*version);
    const auto buffer = editor_.get_buffer();
    const auto size = static_cast<std::size_t>(buffer->get_char_count());
    buffer->select_range(
        buffer->get_iter_at_offset(static_cast<int>(std::min(cursor, size))),
        buffer->get_iter_at_offset(static_cast<int>(std::min(bound, size))));
    const auto scroll =
        v.as_object().contains("scroll") ? view_scroll(v.at("scroll")) : 0;
    const auto revision = current_->revision;
    position_restore_ = Glib::signal_timeout().connect(
        sigc::track_object(
            [this, id = *id, occurrence = occurrence_, cursor, bound, scroll,
             revision, attempts = 60, restored = false, stable = 0,
             upper = -1.0]() mutable {
              if (--attempts <= 0 || !current_ ||
                  current_->value.blocks.composition_id() != id ||
                  occurrence_ != occurrence || current_->revision != revision ||
                  pages_.get_visible_child_name() != "edit" || dirty_ || ime_)
                return false;
              if (!editor_.get_mapped())
                return true;
              if (!editor_.has_focus()) {
                editor_.grab_focus();
                return true;
              }
              const auto adjustment = edit_scroll_.get_vadjustment();
              if (!restored) {
                const auto b = editor_.get_buffer();
                const auto count =
                    static_cast<std::size_t>(b->get_char_count());
                b->select_range(b->get_iter_at_offset(
                                    static_cast<int>(std::min(cursor, count))),
                                b->get_iter_at_offset(
                                    static_cast<int>(std::min(bound, count))));
                restored = true;
              }
              const auto target =
                  std::clamp(scroll, adjustment->get_lower(),
                             std::max(adjustment->get_lower(),
                                      adjustment->get_upper() -
                                          adjustment->get_page_size()));
              const auto before = adjustment->get_value();
              adjustment->set_value(target);
              if (upper == adjustment->get_upper() &&
                  std::abs(before - target) < 0.5)
                ++stable;
              else
                stable = 0;
              upper = adjustment->get_upper();
              return stable < 3;
            },
            *this),
        50);
  }
  // Import previews and output approvals are never restored from preferences.
  // They require fresh source selection/preparation and explicit review.
}
void CompositionPage::attempt(const std::function<void()> &action) {
  try {
    action();
  } catch (const std::exception &e) {
    message_.set_text(author::diagnostic(e.what()));
  }
}
void CompositionPage::reset() {
  position_restore_.disconnect();
  debounce_.disconnect();
  current_.reset();
  source_.reset();
  prepared_.reset();
  dirty_ = ime_ = false;
  editor_.set_editable(false);
  block_.clear();
  occurrence_.clear();
  offset_ = 0;
  pages_.set_visible_child("queue");
  clear(compositions_);
  clear(occurrences_);
  message_.set_text("");
}
void CompositionPage::refresh() {
  attempt([&] {
    flush();
    current_.reset();
    title_.set_text("Montagens de texto");
    all_.set_visible(false);
    more_.set_visible(false);
    clear(compositions_);
    const auto rows = service_.compositions().list(offset_);
    for (const auto &row : rows) {
      auto *b = Gtk::make_managed<Gtk::Button>();
      auto *l = Gtk::make_managed<Gtk::Label>(row.name + "\nRevisão " +
                                              std::to_string(row.revision));
      label(*l);
      b->set_child(*l);
      set_accessible_label(*b, row.name);
      b->signal_clicked().connect(
          [this, id = row.id] { attempt([&] { open(id); }); });
      compositions_.append(*b);
    }
    previous_.set_sensitive(offset_ > 0);
    next_.set_sensitive(rows.size() == 30);
    pages_.set_visible_child("queue");
    message_.set_text(rows.empty() ? "Nenhuma Montagem de texto nesta página. Crie uma e importe um Documento ou seleção."
                                   : "");
  });
}
void CompositionPage::open(const std::string &id) {
  position_restore_.disconnect();
  flush();
  current_ = service_.compositions().load(id);
  show_order();
}
void CompositionPage::flush() {
  debounce_.disconnect();
  if (ime_)
    throw std::runtime_error(
        "Conclua a entrada do teclado antes de sair do bloco");
  if (!dirty_)
    return;
  if (!current_ || block_.empty())
    throw std::runtime_error("Rascunho sem contexto; não foi descartado");
  auto candidate = *current_;
  candidate.value.drafts[block_] = {expected_,
                                    editor_.get_buffer()->get_text().raw()};
  current_ = service_.compositions().save(candidate, true);
  dirty_ = false;
  message_.set_text("Rascunho salvo no Projeto, ainda não aplicado à leitura.");
  apply_.set_sensitive(true);
  discard_.set_sensitive(true);
}
void CompositionPage::mutate(
    const std::function<void(project::Composition &)> &action) {
  flush();
  if (!current_)
    throw std::runtime_error("Abra uma Montagem de texto");
  auto candidate = *current_;
  const auto previous_occurrence = occurrence_;
  try {
    action(candidate.value);
    current_ = service_.compositions().save(candidate);
  } catch (...) {
    occurrence_ = previous_occurrence;
    throw;
  }
  prepared_.reset();
  acknowledged_.set_active(false);
  message_.set_text("Montagem de texto salva no Projeto.");
  show_order();
}
void CompositionPage::show_order() {
  if (!current_)
    throw std::runtime_error("Abra uma Montagem de texto");
  title_.set_text(current_->value.name);
  all_.set_visible(true);
  more_.set_visible(true);
  clear(occurrences_);
  const auto &lab = current_->value.blocks;
  if (!occurrence_.empty() &&
      std::none_of(lab.placements().begin(), lab.placements().end(),
                   [&](auto &p) { return p.id == occurrence_; }))
    occurrence_.clear();
  std::size_t n{};
  for (const auto &p : lab.placements()) {
    auto *b = Gtk::make_managed<Gtk::Button>();
    auto *l = Gtk::make_managed<Gtk::Label>(
        std::to_string(++n) + ". " + lab.block(p.block_id).label + "\n" +
        mode(p) + (p.hidden ? " · oculta" : "") +
        (p.layer ? " · camada: " + lab.layers().at(*p.layer) : "") +
        (p.variant ? " · alternativa: " + lab.variants().at(*p.variant) : ""));
    label(*l);
    b->set_child(*l);
    set_accessible_label(*b, "Editar uso do bloco " + std::to_string(n));
    b->signal_clicked().connect(
        [this, id = p.id] { attempt([&] { show_editor(id); }); });
    occurrences_.append(*b);
  }
  pages_.set_visible_child("order");
  message_.set_text("Montagem de texto salva no Projeto.");
  if (lab.placements().empty())
    message_.set_text("Importe um Documento ou uma seleção para começar. "
                      "Nenhuma origem será modificada.");
  else if (!current_->value.drafts.empty())
    message_.set_text(std::to_string(current_->value.drafts.size()) +
                      " rascunho(s) salvo(s), ainda não aplicados. Abra o "
                      "bloco para recuperar.");
}
void CompositionPage::show_editor(const std::string &id) {
  position_restore_.disconnect();
  flush();
  occurrence_ = id;
  const auto &lab = current_->value.blocks;
  const auto &p = lab.occurrence(id);
  block_ = p.block_id;
  loading_ = true;
  versions_.remove_all();
  for (const auto &r : lab.block(block_).revisions)
    versions_.append(r.id, (r.id == lab.block(block_).current_revision
                                ? "Atual — "
                                : "Anterior — ") +
                               r.id.substr(0, 8));
  versions_.set_active_id(lab.revision(p).id);
  loading_ = false;
  display_version();
  pages_.set_visible_child("edit");
}
void CompositionPage::reveal_occurrence(const std::string &composition,
                                        const std::string &occurrence) {
  const auto current = service_.compositions().load(composition);
  (void)current.value.blocks.occurrence(occurrence);
  flush();
  open(composition);
  show_editor(occurrence);
  editor_.grab_focus();
}
void CompositionPage::display_version() {
  const auto &lab = current_->value.blocks;
  const auto &p = lab.occurrence(occurrence_);
  const auto revision = versions_.get_active_id().raw();
  const auto &r = lab.version(block_, revision);
  const bool writable = revision == lab.block(block_).current_revision &&
                        p.mode != experimental::PlacementMode::Pinned &&
                        p.mode != experimental::PlacementMode::ReferenceOnly;
  expected_ = revision;
  const auto d = current_->value.drafts.find(block_);
  const bool recovered = writable && d != current_->value.drafts.end();
  loading_ = true;
  editor_.get_buffer()->set_text(recovered ? d->second.text : r.text);
  editor_.set_editable(writable);
  loading_ = false;
  if (recovered)
    expected_ = d->second.expected_revision;
  apply_.set_sensitive(recovered);
  discard_.set_sensitive(recovered);
  std::string info = lab.block(block_).label + " — " + mode(p) + "\n";
  info += writable ? "Edição textual conserva as camadas pelo motor do INDE. A "
                     "leitura só muda ao aplicar a versão."
                   : "Esta versão é somente leitura. Use Acompanhar versão "
                     "atual ou Tornar independente para editar.";
  const auto impact = lab.impact(block_);
  info += "\nAplicar afeta " + std::to_string(impact.changed_live.size()) +
          " uso do bloco(s); conserva " +
          std::to_string(impact.preserved_pinned.size()) + " fixada(s).";
  if (recovered)
    info += "\nRascunho recuperado do Projeto; escolha Aplicar ou Descartar.";
  for (const auto &warning : r.layers.warnings)
    info += "\n• " + warning;
  edit_info_.set_text(info);
  message_.set_text(recovered
                        ? "Rascunho recuperado, ainda não aplicado à leitura."
                        : "Versão aplicada do bloco apresentada; origem "
                          "independente preservada.");
}
void CompositionPage::show_reading() {
  flush();
  const auto &c = current_->value;
  reading_text_.get_buffer()->set_text(c.blocks.project(c.profile).text);
  std::vector<ContextMenuAction> choices;
  for (const auto &[id, name] : c.blocks.variants())
    choices.push_back(
        {"Escolher: " + name, [this, id] {
           attempt([&] {
             mutate([&](auto &v) {
               if (v.blocks.variant_groups().contains(id))
                 v.profile.selected_variants[v.blocks.variant_groups().at(id)] =
                     id;
               else
                 v.profile.variant = id;
             });
             show_reading();
           });
         }});
  for (const auto &[id, name] : c.blocks.layers())
    choices.push_back({"Mostrar / ocultar: " + name, [this, id] {
                         attempt([&] {
                           mutate([&](auto &v) {
                             if (v.profile.visible_layers.contains(id))
                               v.profile.visible_layers.erase(id);
                             else
                               v.profile.visible_layers.insert(id);
                           });
                           show_reading();
                         });
                       }});
  configure_overflow_button(reading_profile_, std::move(choices),
                            "Alternativas e camadas desta leitura");
  reading_profile_.set_label("Alternativas e camadas");
  reading_profile_.set_visible(!c.blocks.variants().empty() ||
                               !c.blocks.layers().empty());
  reading_info_.set_text(
      "Somente leitura — versões em uso na montagem. " +
      std::to_string(c.drafts.size()) +
      " rascunho(s) salvo(s), mas ainda não usado(s), ficam fora desta leitura. "
      "Estilos e camadas são conservados nos dados da montagem e no texto "
      "gerado; esta prévia mostra somente o texto.");
  pages_.set_visible_child("reading");
  message_.set_text(
      "Prévia da montagem; esta página não altera o texto dos blocos.");
}
void CompositionPage::show_import() {
  flush();
  source_.reset();
  sources_->clear_selection();
  sources_->refresh();
  import_text_.get_buffer()->set_text("");
  pages_.set_visible_child("import");
}
void CompositionPage::import_source(bool whole) {
  if (!source_ || !current_)
    throw std::runtime_error("Escolha um Documento de origem");
  // Import is based on the exact source shown, not an unseen later reload.
  std::size_t a{}, b = project::utf8_character_count(source_->content);
  Gtk::TextIter first, last;
  if (!whole) {
    if (!import_text_.get_buffer()->get_selection_bounds(first, last))
      throw std::runtime_error("Selecione o trecho na origem");
    a = first.get_offset();
    b = last.get_offset();
  }
  mutate([&](auto &c) {
    occurrence_ = service_.compositions().import(c, *source_, a, b);
  });
}
void CompositionPage::occurrence_command(const std::string &command) {
  if (!current_ || occurrence_.empty())
    throw std::runtime_error("Abra um uso do bloco primeiro");
  if (command == "remove") {
    flush();
    const auto uses = service_.builder().text_uses(
        "", current_->value.blocks.composition_id(), occurrence_);
    if (!uses.empty()) {
      auto *owner = dynamic_cast<Gtk::Window *>(get_root());
      if (!owner)
        throw std::runtime_error("Janela indisponível; uso do bloco preservada");
      const auto expected = *current_;
      const auto occurrence = occurrence_;
      auto *dialog =
          new OverlayDialog("Retirar uso do bloco associado", *owner, true);
      dialog->set_secondary_text(
          "Este uso do bloco desenvolve " + std::to_string(uses.size()) +
          " associações narrativas. Retirá-la preserva o bloco e seu "
          "histórico, mas deixa alvos pendentes para substituir ou "
          "desvincular. Nada é retirado sem confirmação.");
      auto entries =
          std::make_shared<const std::vector<project::RemovalUse>>(uses);
      auto *where = Gtk::make_managed<IncrementalSelector>(
          "Onde este uso do bloco é usado",
          [entries](const std::string &query, std::size_t limit) {
            std::vector<IncrementalSelection> out;
            const auto key = project::alias_key(query);
            for (const auto &v : *entries)
              if (project::alias_key(v.name + " " + v.path).find(key) !=
                  std::string::npos) {
                out.push_back({v.record_id, v.name, v.path});
                if (out.size() >= limit)
                  break;
              }
            return out;
          });
      dialog->get_content_area()->append(*where);
      dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
      dialog->add_button("Ver planejamento", 1);
      dialog->add_button("Retirar e manter pendências",
                         Gtk::ResponseType::ACCEPT);
      dialog->signal_response().connect([this, dialog, expected,
                                         occurrence](int response) {
        if (response != 1 && response != Gtk::ResponseType::ACCEPT)
          return;
        try {
          if (!current_ || current_->sha256 != expected.sha256 ||
              current_->revision != expected.revision ||
              occurrence_ != occurrence)
            throw std::runtime_error("O uso do bloco mudou; confira novamente");
          if (response == 1)
            narrative_uses_signal_.emit();
          else
            mutate([&](auto &value) {
              value.blocks.remove_placement(occurrence);
            });
        } catch (const std::exception &e) {
          dialog->set_secondary_text(author::diagnostic(e.what()));
          dialog->keep_open_on_response();
        }
      });
      dialog->present();
      return;
    }
  }
  mutate([&](auto &c) {
    auto &lab = c.blocks;
    const auto p = lab.occurrence(occurrence_);
    const auto rev = lab.revision(p).id;
    if (command == "pin")
      lab.pin(p.id, rev);
    else if (command == "follow")
      lab.follow_current(p.id);
    else if (command == "fork")
      lab.fork(p.id, rev);
    else if (command == "reference")
      lab.reference_only(p.id);
    else if (command == "hide")
      lab.set_hidden(p.id, !p.hidden);
    else if (command == "repeat") {
      occurrence_ = lab.place(p.block_id, p.mode, p.pinned_revision);
      lab.classify(occurrence_, p.variant, p.layer);
      lab.set_hidden(occurrence_, p.hidden);
    } else if (command == "remove")
      lab.remove_placement(p.id);
  });
}
void CompositionPage::move(int delta) {
  if (!current_ || occurrence_.empty())
    throw std::runtime_error("Abra um uso do bloco primeiro");
  mutate([&](auto &c) {
    std::vector<std::string> order;
    for (const auto &p : c.blocks.placements())
      order.push_back(p.id);
    auto i = std::find(order.begin(), order.end(), occurrence_);
    const int position = std::distance(order.begin(), i),
              target = position + delta;
    if (i == order.end() || target < 0 ||
        target >= static_cast<int>(order.size()))
      throw std::runtime_error("O uso do bloco já está no limite da ordem");
    std::swap(order[position], order[target]);
    c.blocks.reorder(order);
  });
}
void CompositionPage::split() {
  if (pages_.get_visible_child_name() != "edit" || !editor_.get_editable())
    throw std::runtime_error("Abra uma versão editável e selecione o corte");
  flush();
  if (current_->value.drafts.contains(block_))
    throw std::runtime_error("Aplique ou descarte o rascunho antes de cortar");
  const auto b = editor_.get_buffer();
  const auto a = b->get_iter_at_mark(b->get_insert()).get_offset(),
             z = b->get_iter_at_mark(b->get_selection_bound()).get_offset();
  mutate([&](auto &c) {
    occurrence_ = c.blocks.split_selection(occurrence_, std::min(a, z),
                                           std::max(a, z), "Trecho separado");
  });
}
void CompositionPage::classify(bool variant) {
  if (!current_ || occurrence_.empty())
    throw std::runtime_error("Abra um uso do bloco primeiro");
  flush();
  auto *window = dynamic_cast<Gtk::Window *>(get_root());
  if (!window)
    return;
  auto *dialog = new OverlayDialog(
      variant ? "Alternativa de leitura" : "Camada de leitura", *window);
  dialog->set_secondary_text(
      "Nome da nova alternativa/camada. A classificação será persistida; nada "
      "muda no Documento de origem.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar e selecionar", Gtk::ResponseType::ACCEPT);
  auto *entry = Gtk::make_managed<Gtk::Entry>();
  entry->set_placeholder_text("Nome");
  entry->set_max_length(64);
  dialog->get_content_area()->append(*entry);
  const auto id = current_->value.blocks.composition_id(), occ = occurrence_;
  const auto revision = current_->revision;
  dialog->signal_response().connect(sigc::track_object(
      [this, dialog, entry, variant, id, occ, revision](int r) {
        const auto name = entry->get_text().raw();
        dialog->hide();
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        attempt([&] {
          if (!current_ || current_->value.blocks.composition_id() != id ||
              current_->revision != revision || occurrence_ != occ)
            throw std::runtime_error("Contexto mudou; reabra a classificação");
          mutate([&](auto &c) {
            if (variant) {
              occurrence_ = c.blocks.alternative(occ, name);
              const auto &p = c.blocks.occurrence(occurrence_);
              c.profile.selected_variants[*p.variant_group] = *p.variant;
            } else {
              const auto layer = c.blocks.add_layer(name);
              const auto p = c.blocks.occurrence(occ);
              c.blocks.classify(occ, p.variant, layer);
              c.profile.visible_layers.insert(layer);
            }
          });
        });
      },
      *this));
  dialog->present();
}
void CompositionPage::show_origin() {
  flush();
  if (!current_ || occurrence_.empty())
    throw std::runtime_error("Abra um uso do bloco primeiro");
  const auto &lab = current_->value.blocks;
  const auto &p = lab.occurrence(occurrence_);
  const auto &b = lab.block(p.block_id);
  auto origin = b.origin_snapshot_sha256
                    ? current_->value.origins.find(*b.origin_snapshot_sha256)
                    : current_->value.origins.end();
  if (origin == current_->value.origins.end())
    throw std::runtime_error("Origem congelada ausente");
  origin_text_.get_buffer()->set_text(origin->second.content);
  origin_info_.set_text("Origem congelada: " + origin->second.title +
                        "\nCópia preservada da origem completo SHA256 " + origin->first +
                        "\nTexto e camadas preservados. Esta exibição é "
                        "textual e não acompanha futuras edições da origem.");
  pages_.set_visible_child("origin");
  message_.set_text(
      "Cópia preservada da origem histórico somente leitura; não é um vínculo de edição atual.");
}
void CompositionPage::prepare() {
  flush();
  if (!current_)
    throw std::runtime_error("Abra uma Montagem de texto");
  prepared_ = service_.compositions().prepare(*current_, current_->value.name);
  acknowledged_.set_active(false);
  materialize_.set_sensitive(false);
  export_.set_sensitive(false);
  output_text_.get_buffer()->set_text(prepared_->output.document.content);
  output_info_.set_text(
      "Saída independente, revisão " + std::to_string(current_->revision) +
      ". Confira a leitura e os avisos; nenhuma origem será alterada.");
  std::string info;
  for (const auto &warning : prepared_->reading.limitations)
    info += "• " + warning + "\n";
  output_losses_.set_text(info);
  output_details_.set_expanded(true);
  pages_.set_visible_child("output");
  message_.set_text(
      "Leitura preparada; confira o texto e os avisos antes de confirmar.");
}
void CompositionPage::export_reading() {
  if (!prepared_ || !acknowledged_.get_active())
    throw std::runtime_error("Confira a saída primeiro");
  auto *window = dynamic_cast<Gtk::Window *>(get_root());
  if (!window)
    return;
  const auto prepared = *prepared_;
  auto chooser = Gtk::FileChooserNative::create(
      "Pasta para exportar leitura", *window,
      Gtk::FileChooser::Action::SELECT_FOLDER, "Exportar", "Cancelar");
  chooser->signal_response().connect(sigc::track_object(
      [this, chooser, prepared](int r) {
        if (r != Gtk::ResponseType::ACCEPT || !chooser->get_file())
          return;
        attempt([&] {
          if (!prepared_ || !acknowledged_.get_active() ||
              prepared_->source.sha256 != prepared.source.sha256)
            throw std::runtime_error("Prévia mudou; prepare novamente");
          auto folder =
              std::filesystem::canonical(chooser->get_file()->get_path());
          const auto project =
              std::filesystem::canonical(service_.current()->path());
          const auto mismatch = std::mismatch(project.begin(), project.end(),
                                              folder.begin(), folder.end());
          if (mismatch.first == project.end())
            throw std::runtime_error("Escolha uma pasta fora do Projeto");
          folder /= "INDE-composicao-" + project::new_uuid();
          service_.compositions().export_reading(prepared, folder, true);
          message_.set_text("Leitura exportada e conferida: " +
                            folder.string());
        });
      },
      *this));
  chooser->show();
}
void CompositionPage::reload() {
  if (!current_)
    return;
  auto *window = dynamic_cast<Gtk::Window *>(get_root());
  if (!window)
    return;
  auto *dialog = new OverlayDialog("Recarregar Montagem de texto", *window);
  dialog->set_secondary_text(
      "Descartar somente alterações locais ainda não salvas e recarregar o "
      "Projeto? Rascunhos já persistidos continuam no Projeto. Copie o texto "
      "local antes se precisa conservá-lo.");
  dialog->add_button("Manter rascunho local", Gtk::ResponseType::CANCEL);
  dialog->add_button("Descartar local e recarregar", Gtk::ResponseType::ACCEPT);
  const auto id = current_->value.blocks.composition_id();
  dialog->signal_response().connect(sigc::track_object(
      [this, dialog, id](int r) {
        dialog->hide();
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        attempt([&] {
          auto persisted = service_.compositions().load(id);
          debounce_.disconnect();
          dirty_ = false;
          current_ = std::move(persisted);
          prepared_.reset();
          acknowledged_.set_active(false);
          show_order();
        });
      },
      *this));
  dialog->present();
}
} // namespace inde::ui
