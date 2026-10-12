#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/production_page.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include <algorithm>
namespace inde::ui {
namespace {
ViewState production_selections(const application::ProductionChoices &choices) {
  ViewState::Object selections;
  for (const auto &[id, c] : choices) {
    ViewState::Array docs;
    for (const auto &d : c.documents)
      docs.emplace_back(d);
    selections[id] = ViewState::Object{{"heading", c.heading},
                                       {"documents", std::move(docs)}};
  }
  ViewState result(std::move(selections));
  if (serialize_view(result).size() > 24576)
    throw std::runtime_error("Seleção excede 24 KiB de contexto; reduza os "
                             "textos/unidades. Nada foi truncado.");
  return result;
}
} // namespace
ProductionPage::ProductionPage(application::ProjectService &s)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), service_(s) {
  set_vexpand(true);
  set_hexpand(true);
  pages_.set_hhomogeneous(false);
  pages_.set_vhomogeneous(false);
  pages_.set_vexpand(true);
  append(pages_);
  profile_.append("manuscript", "Manuscrito para avaliação");
  profile_.append("reading", "Leitura");
  profile_.set_active_id("manuscript");
  bound_choice_width(profile_);
  author_.set_placeholder_text("Nome autoral na saída (opcional)");
  author_.set_max_length(256);
  author_.set_hexpand(true);
  set_accessible_label(author_, "Nome autoral na saída");
  controls_.append(profile_);
  controls_.append(author_);
  plan_.append(controls_);
  docx_.set_active(true);
  epub_.set_active(true);
  pdf_.set_active(true);
  for (auto *b : {&docx_, &epub_, &pdf_})
    formats_.append(*b);
  auto *base =
      Gtk::make_managed<Gtk::Label>("HTML + TXT + relatório sempre incluídos");
  formats_.append(*base);
  plan_.append(formats_);
  hint_.set_wrap(true);
  hint_.set_xalign(0);
  hint_.add_css_class("dim-label");
  plan_.append(hint_);
  units_scroll_.set_child(units_);
  units_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  units_scroll_.set_vexpand(true);
  plan_.append(units_scroll_);
  navigation_.append(previous_);
  navigation_.append(next_);
  navigation_.append(prepare_);
  plan_.append(navigation_);
  message_.set_wrap(true);
  message_.set_xalign(0);
  append(message_);
  summary_.set_wrap(true);
  summary_.set_xalign(0);
  preview_.append(summary_);
  text_.set_editable(false);
  text_.set_cursor_visible(false);
  text_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  set_accessible_label(text_, "Prévia textual da Obra produzida");
  preview_scroll_.set_child(text_);
  preview_scroll_.set_vexpand(true);
  preview_.append(preview_scroll_);
  losses_.set_wrap(true);
  losses_.set_xalign(0);
  losses_.set_selectable(true);
  losses_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  losses_scroll_.set_min_content_height(100);
  losses_scroll_.set_max_content_height(180);
  losses_scroll_.set_propagate_natural_height(true);
  losses_scroll_.set_child(losses_);
  details_.set_child(losses_scroll_);
  preview_.append(details_);
  preview_.append(acknowledged_);
  preview_actions_.append(back_);
  preview_actions_.append(export_);
  preview_actions_.append(cancel_);
  cancel_.set_visible(false);
  preview_.append(preview_actions_);
  pages_.add(plan_, "plan");
  pages_.add(preview_, "preview");
  pages_.set_visible_child("plan");
  export_.set_sensitive(false);
  previous_.signal_clicked().connect([this] {
    if (offset_ >= 30)
      offset_ -= 30;
    render_units();
  });
  next_.signal_clicked().connect([this] {
    offset_ += 30;
    render_units();
  });
  prepare_.signal_clicked().connect(
      sigc::mem_fun(*this, &ProductionPage::prepare));
  back_.signal_clicked().connect([this] {
    if (!worker_.joinable())
      pages_.set_visible_child("plan");
  });
  acknowledged_.signal_toggled().connect([this] {
    export_.set_sensitive(acknowledged_.get_active() && !worker_.joinable());
  });
  export_.signal_clicked().connect(
      sigc::mem_fun(*this, &ProductionPage::export_snapshot));
  cancel_.signal_clicked().connect([this] {
    worker_.request_stop();
    message_.set_text("Cancelamento solicitado. Uma pasta parcial, se criada, "
                      "não será apresentada como concluída.");
  });
}
ProductionPage::~ProductionPage() {
  delivery_.disconnect();
  worker_.request_stop();
  if (worker_.joinable())
    worker_.join();
}
Gtk::Window *ProductionPage::owner() {
  return dynamic_cast<Gtk::Window *>(get_root());
}
void ProductionPage::reset() {
  delivery_.disconnect();
  worker_.request_stop();
  if (worker_.joinable())
    worker_.join();
  work_id_.reset();
  project_id_.clear();
  structure_id_.clear();
  nodes_.clear();
  choices_.clear();
  prepared_.reset();
  offset_ = 0;
  while (auto *c = units_.get_first_child())
    units_.remove(*c);
  text_.get_buffer()->set_text("");
  losses_.set_text("");
  message_.set_text("");
  author_.set_text("");
  profile_.set_active_id("manuscript");
  docx_.set_active(true);
  epub_.set_active(true);
  pdf_.set_active(true);
  cancel_.set_visible(false);
  pages_.set_visible_child("plan");
  prepare_.set_sensitive(false);
  back_.set_sensitive(true);
  acknowledged_.set_active(false);
  export_.set_sensitive(false);
}
void ProductionPage::set_work(const std::optional<std::string> &id) {
  if (!service_.current() || !id) {
    reset();
    message_.set_text("Crie/abra uma Obra no Manuscrito antes de produzir.");
    return;
  }
  const auto project = service_.current()->manifest().project_id;
  if (work_id_ != id || project_id_ != project) {
    reset();
    work_id_ = id;
    project_id_ = project;
  }
  try {
    const auto active = service_.structures().active_editorial(*id);
    if (!active)
      throw std::runtime_error("Obra sem estrutura ativa");
    if (!structure_id_.empty() && structure_id_ != active->id) {
      choices_.clear();
      prepared_.reset();
      message_.set_text("A estrutura ativa mudou; confirme uma nova seleção.");
    }
    structure_id_ = active->id;
    nodes_ =
        application::production_order(service_.structural_nodes_for_work(*id));
    if (nodes_.size() > 1000)
      throw std::runtime_error(
          "Seletor desta versão admite até 1000 unidades; nada foi truncado");
    pages_.set_visible_child("plan");
    render_units();
    prepare_.set_sensitive(!nodes_.empty());
  } catch (const std::exception &e) {
    nodes_.clear();
    while (auto *c = units_.get_first_child())
      units_.remove(*c);
    prepare_.set_sensitive(false);
    message_.set_text(author::diagnostic(e.what()));
  }
}
void ProductionPage::render_units() {
  while (auto *c = units_.get_first_child())
    units_.remove(*c);
  if (offset_ >= nodes_.size())
    offset_ = 0;
  const auto labels = project::structural_node_path_labels(nodes_);
  for (std::size_t i = offset_; i < std::min(nodes_.size(), offset_ + 30);
       ++i) {
    const auto &n = nodes_[i];
    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    card->add_css_class("content-card");
    card->set_margin(3);
    auto *label = Gtk::make_managed<Gtk::Label>(std::to_string(i + 1) + ". " +
                                                labels.at(n.id));
    label->set_wrap(true);
    label->set_xalign(0);
    card->append(*label);
    auto *line = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto *button = Gtk::make_managed<Gtk::Button>("Escolher textos");
    button->set_tooltip_text(n.id);
    const auto c = choices_.find(n.id);
    auto *state = Gtk::make_managed<Gtk::Label>(
        c == choices_.end()
            ? "Omitida — nenhuma escolha confirmada"
            : std::to_string(c->second.documents.size()) + " texto(s) · " +
                  (c->second.heading ? "com título" : "sem título"));
    state->set_wrap(true);
    state->set_xalign(0);
    line->append(*button);
    line->append(*state);
    card->append(*line);
    units_.append(*card);
    button->signal_clicked().connect([this, id = n.id] { choose_texts(id); });
  }
  previous_.set_sensitive(offset_ > 0);
  next_.set_sensitive(offset_ + 30 < nodes_.size());
}
void ProductionPage::choose_texts(const std::string &node) {
  if (!owner() || !service_.current())
    return;
  try {
    const auto project = project_id_, structure = structure_id_;
    auto selected = std::make_shared<application::ProductionChoice>(
        choices_.contains(node) ? choices_.at(node)
                                : application::ProductionChoice{});
    auto *dialog = new OverlayDialog("Textos da unidade", *owner(), true);
    dialog->set_secondary_text(
        "Escolha e ordene os textos desta unidade. Documentos de referência, "
        "esboços e revisões não entram automaticamente. Cancelar conserva a "
        "seleção anterior.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Confirmar seleção", Gtk::ResponseType::ACCEPT);
    auto *body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    body->set_margin(12);
    auto *heading =
        Gtk::make_managed<Gtk::CheckButton>("Incluir título da unidade");
    heading->set_active(selected->heading);
    body->append(*heading);
    auto *combo = Gtk::make_managed<Gtk::ComboBoxText>();
    combo->set_hexpand(true);
    bound_choice_width(*combo);
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto *add = Gtk::make_managed<Gtk::Button>("Adicionar texto");
    row->append(*combo);
    row->append(*add);
    body->append(*row);
    auto *nav = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto *prev = Gtk::make_managed<Gtk::Button>("Textos anteriores");
    auto *next = Gtk::make_managed<Gtk::Button>("Próximos textos");
    nav->append(*prev);
    nav->append(*next);
    body->append(*nav);
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_min_content_height(120);
    scroll->set_max_content_height(260);
    scroll->set_propagate_natural_height(true);
    auto *list = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    scroll->set_child(*list);
    body->append(*scroll);
    auto *notice = Gtk::make_managed<Gtk::Label>();
    notice->set_wrap(true);
    body->append(*notice);
    dialog->get_content_area()->append(*body);
    auto offset = std::make_shared<std::size_t>(0);
    auto refresh = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weak = refresh;
    *refresh = [this, selected, node, combo, list, notice, offset, prev, next,
                weak] {
      if (!service_.current())
        return;
      persistence::DocumentQuery q;
      q.editorial_node_id = node;
      q.limit = 50;
      q.offset = *offset;
      const auto count = service_.writing().document_count(q);
      combo->remove_all();
      for (const auto &d : service_.writing().document_summaries(q))
        combo->append(d.id, d.title + " — " + project::display_name(d.purpose));
      combo->set_active(0);
      prev->set_sensitive(*offset > 0);
      next->set_sensitive(*offset + 50 < count);
      while (auto *c = list->get_first_child())
        list->remove(*c);
      persistence::DocumentQuery title_query;
      title_query.editorial_node_id = node;
      title_query.limit = 500;
      const auto titles = service_.writing().document_summaries(title_query);
      for (std::size_t i = 0; i < selected->documents.size(); ++i) {
        const auto d =
            std::find_if(titles.begin(), titles.end(), [&](const auto &s) {
              return s.id == selected->documents[i];
            });
        auto *r = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
        auto *l = Gtk::make_managed<Gtk::Label>(
            std::to_string(i + 1) + ". " +
            (d != titles.end() ? d->title
                               : "Documento fora da janela de títulos"));
        l->set_ellipsize(Pango::EllipsizeMode::END);
        l->set_hexpand(true);
        l->set_xalign(0);
        r->append(*l);
        auto *up = Gtk::make_managed<Gtk::Button>("↑");
        auto *down = Gtk::make_managed<Gtk::Button>("↓");
        auto *remove = Gtk::make_managed<Gtk::Button>("Retirar");
        up->set_sensitive(i > 0);
        down->set_sensitive(i + 1 < selected->documents.size());
        r->append(*up);
        r->append(*down);
        r->append(*remove);
        list->append(*r);
        up->signal_clicked().connect([selected, i, weak] {
          std::swap(selected->documents[i], selected->documents[i - 1]);
          if (auto f = weak.lock())
            (*f)();
        });
        down->signal_clicked().connect([selected, i, weak] {
          std::swap(selected->documents[i], selected->documents[i + 1]);
          if (auto f = weak.lock())
            (*f)();
        });
        remove->signal_clicked().connect([selected, i, weak] {
          selected->documents.erase(selected->documents.begin() + i);
          if (auto f = weak.lock())
            (*f)();
        });
      }
      notice->set_text(std::to_string(count) +
                       " disponível(is). Seleção vazia sem título omite a "
                       "unidade. Até 100 textos por unidade neste seletor.");
    };
    prev->signal_clicked().connect([offset, refresh] {
      if (*offset >= 50)
        *offset -= 50;
      (*refresh)();
    });
    next->signal_clicked().connect([offset, refresh] {
      *offset += 50;
      (*refresh)();
    });
    add->signal_clicked().connect([selected, combo, refresh] {
      const auto id = combo->get_active_id().raw();
      if (!id.empty() && selected->documents.size() < 100 &&
          std::find(selected->documents.begin(), selected->documents.end(),
                    id) == selected->documents.end()) {
        selected->documents.push_back(id);
        (*refresh)();
      }
    });
    dialog->signal_response().connect(sigc::track_object(
        [this, dialog, selected, heading, node, project, structure,
         refresh](int r) {
          (void)refresh;
          dialog->hide();
          if (r != Gtk::ResponseType::ACCEPT)
            return;
          if (!service_.current() ||
              service_.current()->manifest().project_id != project ||
              structure_id_ != structure) {
            message_.set_text("Fonte ativa mudou; seleção não foi aplicada");
            return;
          }
          selected->heading = heading->get_active();
          auto candidate = choices_;
          if (!selected->heading && selected->documents.empty())
            candidate.erase(node);
          else
            candidate[node] = *selected;
          try {
            (void)production_selections(candidate);
          } catch (const std::exception &e) {
            message_.set_text(author::diagnostic(e.what()));
            return;
          }
          choices_ = std::move(candidate);
          prepared_.reset();
          render_units();
        },
        *this));
    (*refresh)();
    dialog->present();
  } catch (const std::exception &e) {
    message_.set_text(author::diagnostic(e.what()));
  }
}
void ProductionPage::prepare() {
  if (!service_.current() || !work_id_)
    return;
  try {
    const auto &works = service_.catalog().works;
    const auto w =
        std::find_if(works.begin(), works.end(),
                     [this](const auto &v) { return v.id == *work_id_; });
    if (w == works.end())
      throw std::runtime_error("Obra indisponível");
    const auto active = service_.structures().active_editorial(*work_id_);
    if (!active || active->id != structure_id_)
      throw std::runtime_error("Estrutura ativa mudou; atualize a produção");
    std::vector<project::Document> docs;
    std::size_t selected_bytes{};
    for (const auto &n : nodes_) {
      persistence::DocumentQuery q;
      q.editorial_node_id = n.id;
      q.limit = 500;
      const auto total = service_.writing().document_count(q);
      if (total > 500)
        throw std::runtime_error(
            "Unidade excede 500 textos nesta preparação; nada foi truncado");
      for (const auto &s : service_.writing().document_summaries(q)) {
        if (docs.size() >= 10000)
          throw std::runtime_error(
              "Preparação excede 10000 Documentos; nada foi truncado");
        const auto choice = choices_.find(n.id);
        if (choice != choices_.end() &&
            std::find(choice->second.documents.begin(),
                      choice->second.documents.end(),
                      s.id) != choice->second.documents.end()) {
          if (s.character_count > 8 * 1024 * 1024)
            throw std::runtime_error("Documento acima do limite de produção");
          const auto d = service_.writing().document(s.id);
          if (!d)
            throw std::runtime_error("Documento mudou durante preparação");
          selected_bytes += d->content.size();
          if (selected_bytes > 8 * 1024 * 1024)
            throw std::runtime_error("Seleção excede 8 MiB; nada foi truncado");
          docs.push_back(*d);
        } else {
          project::Document omitted;
          omitted.id = s.id;
          omitted.title = s.title;
          omitted.editorial_node_id = s.editorial_node_id;
          docs.push_back(std::move(omitted));
        }
      }
    }
    application::ProductionOptions options;
    options.profile = profile_.get_active_id() == "reading"
                          ? application::ProductionProfile::Reading
                          : application::ProductionProfile::Manuscript;
    options.author = author_.get_text().raw();
    options.docx = docx_.get_active();
    options.epub = epub_.get_active();
    options.pdf = pdf_.get_active();
    prepared_ = std::make_shared<const application::WorkProduction>(
        application::prepare_work_production(
            project_id_, structure_id_, *w,
            service_.structural_nodes_for_work(*work_id_), docs, choices_,
            options));
    text_.get_buffer()->set_text(prepared_->preview);
    summary_.set_text(prepared_->work.title + " · versão preparada para exportação em " +
                      prepared_->prepared_at +
                      "\nPrévia textual; paginação e tipografia finais "
                      "dependem do formato e perfil.");
    std::string detail = "Fontes na ordem confirmada:\n";
    for (const auto &u : prepared_->units)
      for (const auto &d : u.documents)
        detail += u.node.title + " → " + d.title + " · " + d.id + " · " +
                  d.updated_at + "\n";
    detail += "\nPerdas e limites:\n";
    for (const auto &s : prepared_->limitations)
      detail += "• " + s + "\n";
    detail += "\nOmissões:\n";
    for (const auto &s : prepared_->omissions)
      detail += "• " + s + "\n";
    losses_.set_text(detail);
    details_.set_expanded(false);
    acknowledged_.set_active(false);
    export_.set_sensitive(false);
    message_.set_text("Nada foi gravado. Confira a seleção e as perdas antes "
                      "de escolher um destino.");
    pages_.set_visible_child("preview");
  } catch (const std::exception &e) {
    message_.set_text(author::diagnostic(e.what()));
  }
}
void ProductionPage::export_snapshot() {
  if (!prepared_ || !acknowledged_.get_active() || !owner() ||
      !service_.current() || worker_.joinable())
    return;
  const auto snapshot = prepared_;
  const auto source =
      std::filesystem::weakly_canonical(service_.current()->path());
  auto chooser = Gtk::FileChooserNative::create(
      "Escolher pasta externa para produção", *owner(),
      Gtk::FileChooser::Action::SELECT_FOLDER, "Produzir", "Cancelar");
  export_.set_sensitive(false);
  back_.set_sensitive(false);
  // Keep the native dialog alive until response, then break the signal's
  // ownership cycle. Repeated exports must not retain each closed chooser.
  auto lifetime =
      std::make_shared<Glib::RefPtr<Gtk::FileChooserNative>>(chooser);
  chooser->signal_response().connect(sigc::track_object(
      [this, lifetime, snapshot, source](int response) {
        auto chooser = *lifetime;
        lifetime->reset();
        back_.set_sensitive(true);
        export_.set_sensitive(acknowledged_.get_active());
        if (!chooser || response != Gtk::ResponseType::ACCEPT ||
            !chooser->get_file())
          return;
        if (prepared_ != snapshot || !service_.current()) {
          message_.set_text("Prévia mudou enquanto o destino estava aberto; "
                            "prepare novamente.");
          return;
        }
        try {
          const auto parent =
              std::filesystem::canonical(chooser->get_file()->get_path());
          const auto mismatch = std::mismatch(source.begin(), source.end(),
                                              parent.begin(), parent.end());
          if (mismatch.first == source.end())
            throw std::runtime_error("Escolha uma pasta fora do Projeto");
          const auto target = parent / ("INDE-obra-" + project::new_uuid());
          {
            std::lock_guard lock(result_mutex_);
            result_ready_ = false;
            result_.clear();
          }
          export_.set_sensitive(false);
          back_.set_sensitive(false);
          cancel_.set_visible(true);
          message_.set_text("Gerando formatos do versão preparada e conferida. O Projeto continua preservado.");
          worker_ =
              std::jthread([this, snapshot, target](std::stop_token stop) {
                std::string result;
                bool ok{};
                try {
                  application::write_work_production(*snapshot, target, stop);
                  result = "Produção gravada e conferida: " + target.string();
                  ok = true;
                } catch (const std::exception &e) {
                  result = std::string(author::diagnostic(e.what())) +
                           "\nDestino previsto: " + target.string();
                }
                std::lock_guard lock(result_mutex_);
                result_ = result;
                result_ok_ = ok;
                result_ready_ = true;
              });
          delivery_ = Glib::signal_timeout().connect(
              sigc::track_object(
                  [this] {
                    std::string result;
                    bool ok;
                    {
                      std::lock_guard lock(result_mutex_);
                      if (!result_ready_)
                        return true;
                      result = result_;
                      ok = result_ok_;
                    }
                    worker_.join();
                    cancel_.set_visible(false);
                    back_.set_sensitive(true);
                    export_.set_sensitive(acknowledged_.get_active());
                    message_.set_text(
                        result + (ok ? "\nO relatório contém fontes, omissões, "
                                       "perdas e hashes. Não houve publicação."
                                     : "\nUma pasta parcial não equivale a "
                                       "produção concluída."));
                    return false;
                  },
                  *this),
              80);
        } catch (const std::exception &e) {
          message_.set_text(author::diagnostic(e.what()));
        }
      },
      *this));
  chooser->show();
}
ViewState ProductionPage::capture_view() const {
  auto selections = production_selections(choices_);
  return ViewState::Object{{"version", 1},
                           {"work", view_id(work_id_)},
                           {"structure", structure_id_},
                           {"choices", std::move(selections)},
                           {"profile", profile_.get_active_id().raw()},
                           {"author", author_.get_text().raw()},
                           {"docx", docx_.get_active()},
                           {"epub", epub_.get_active()},
                           {"pdf", pdf_.get_active()},
                           {"offset", static_cast<std::int64_t>(offset_)}};
}
void ProductionPage::restore_view(const ViewState &v) {
  if (v.at("version").as_integer() != 1)
    throw std::runtime_error("Contexto de produção incompatível");
  set_work(view_id(v.at("work")));
  if (v.at("structure").as_string() != structure_id_)
    throw std::runtime_error("Estrutura da seleção mudou; confirme novamente");
  const auto &values = v.at("choices").as_object();
  if (serialize_view(v.at("choices")).size() > 24576)
    throw std::runtime_error("Contexto de produção excede 24 KiB");
  if (values.size() > 1000)
    throw std::runtime_error("Contexto de produção excessivo");
  application::ProductionChoices parsed;
  for (const auto &[id, value] : values) {
    const auto node = view_id(ViewState(id));
    if (!node)
      throw std::runtime_error("Unidade inválida");
    application::ProductionChoice c;
    c.heading = value.at("heading").as_boolean();
    if (value.at("documents").as_array().size() > 100)
      throw std::runtime_error("Muitos textos na seleção");
    for (const auto &d : value.at("documents").as_array()) {
      const auto doc = view_id(d);
      if (!doc)
        throw std::runtime_error("Documento inválido");
      c.documents.push_back(*doc);
    }
    parsed[id] = c;
  }
  const auto profile = v.at("profile").as_string(),
             author = v.at("author").as_string();
  if ((profile != "reading" && profile != "manuscript") || author.size() > 1024)
    throw std::runtime_error("Opções incompatíveis");
  choices_ = std::move(parsed);
  profile_.set_active_id(profile);
  author_.set_text(author);
  docx_.set_active(v.at("docx").as_boolean());
  epub_.set_active(v.at("epub").as_boolean());
  pdf_.set_active(v.at("pdf").as_boolean());
  offset_ = view_offset(v.at("offset"));
  render_units();
  message_.set_text("Seleção retomada. Prepare uma nova prévia; fontes atuais "
                    "serão conferidas, sem reutilizar saída antiga.");
}
} // namespace inde::ui
