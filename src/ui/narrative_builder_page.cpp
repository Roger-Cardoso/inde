#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/temporal_context.hpp"
#include "inde/ui/context_menu.hpp"
#include "narrative_realization_labels.hpp"
#include <algorithm>
#include <memory>
namespace inde::ui {
namespace {
Gtk::Label *label(const std::string &text) {
  auto *l = Gtk::make_managed<Gtk::Label>(text);
  l->set_xalign(0);
  l->set_wrap(true);
  l->set_max_width_chars(72);
  return l;
}
Gtk::Button *button(Gtk::Box &body, const std::string &text,
                    std::function<void()> f) {
  auto *b = Gtk::make_managed<Gtk::Button>(text);
  if (auto *l = dynamic_cast<Gtk::Label *>(b->get_child())) {
    l->set_max_width_chars(56);
    l->set_ellipsize(Pango::EllipsizeMode::END);
  }
  b->set_tooltip_text(text);
  b->set_halign(Gtk::Align::START);
  b->signal_clicked().connect(std::move(f));
  body.append(*b);
  return b;
}
Gtk::Box *section(Gtk::Box &body, const std::string &name) {
  auto *e = Gtk::make_managed<Gtk::Expander>(name);
  if (auto *l = dynamic_cast<Gtk::Label *>(e->get_label_widget())) {
    l->set_max_width_chars(64);
    l->set_ellipsize(Pango::EllipsizeMode::END);
  }
  e->set_tooltip_text(name);
  auto *b = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  e->set_child(*b);
  body.append(*e);
  return b;
}
} // namespace
void NarrativeStructureWorkspace::build_unit_page() {
  unit_header_.append(unit_back_);
  unit_title_.set_hexpand(true);
  unit_title_.set_xalign(0);
  unit_title_.set_ellipsize(Pango::EllipsizeMode::END);
  unit_title_.add_css_class("title-2");
  unit_header_.append(unit_title_);
  unit_page_.append(unit_header_);
  unit_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  unit_scroll_.set_vexpand(true);
  unit_scroll_.set_child(unit_body_);
  unit_body_.set_margin(12);
  unit_page_.append(unit_scroll_);
  stack_.add(unit_page_, "unit");
  auto *header=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL,8);
  header->append(continuity_back_);continuity_title_.set_hexpand(true);continuity_title_.set_xalign(0);
  continuity_title_.set_ellipsize(Pango::EllipsizeMode::END);header->append(continuity_title_);
  continuity_page_.append(*header);continuity_filter_.append("all","Todos os avisos");
  continuity_filter_.append("reconciliation","Conferir texto e planejamento");continuity_filter_.append("insufficient","Informação insuficiente");
  continuity_filter_.append("presence","Tempo e presença");continuity_filter_.append("dependencies","Cadeias e ciclos");
  continuity_filter_.set_active_id("all");continuity_page_.append(continuity_filter_);
  continuity_scroll_.set_policy(Gtk::PolicyType::NEVER,Gtk::PolicyType::AUTOMATIC);
  continuity_scroll_.set_vexpand(true);continuity_scroll_.set_child(continuity_body_);continuity_body_.set_margin(12);
  continuity_page_.append(continuity_scroll_);auto *pagination=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL,8);
  pagination->append(continuity_previous_);pagination->append(continuity_next_);continuity_page_.append(*pagination);
  stack_.add(continuity_page_,"continuity");
  continuity_back_.signal_clicked().connect([this]{stack_.set_visible_child(continuity_origin_);refresh_detail();});
  continuity_filter_.signal_changed().connect([this]{continuity_page_index_=0;render_continuity();});
  continuity_previous_.signal_clicked().connect([this]{if(continuity_page_index_)--continuity_page_index_;render_continuity();});
  continuity_next_.signal_clicked().connect([this]{++continuity_page_index_;render_continuity();});
  unit_back_.signal_clicked().connect([this] {
    stack_.set_visible_child("detail");
    refresh_detail();
    open_unit_button_.grab_focus();
  });
}
void NarrativeStructureWorkspace::show_unit() {
  if (!selected_unit_id_)
    return;
  try {
    stack_.set_visible_child("unit");
    refresh_unit();
    unit_back_.grab_focus();
  } catch (const std::exception &e) {
    show_library();
    show_error(e);
  }
}
void NarrativeStructureWorkspace::refresh_unit() {
  if (!selected_structure_id_ || !selected_unit_id_)
    return;
  builder_snapshot_ = service_.builder().snapshot(*selected_structure_id_);
  const auto observed =
      std::make_shared<const application::NarrativeBuilderSnapshot>(
          builder_snapshot_);
  const auto &s = *observed;
  const auto id = *selected_unit_id_;
  auto at = std::find_if(s.units.begin(), s.units.end(),
                         [&](auto &u) { return u.id == id; });
  if (at == s.units.end()) {
    selected_unit_id_.reset();
    stack_.set_visible_child("detail");
    return;
  }
  std::vector<std::string> expanded;
  const auto scroll = displayed_unit_id_ == id
                          ? unit_scroll_.get_vadjustment()->get_value()
                          : 0.0;
  if (displayed_unit_id_ == id)
    for (auto *child = unit_body_.get_first_child(); child;
         child = child->get_next_sibling())
      if (auto *e = dynamic_cast<Gtk::Expander *>(child);
          e && e->get_expanded())
        expanded.push_back(e->get_label().raw());
  displayed_unit_id_ = id;
  while (auto *child = unit_body_.get_first_child())
    unit_body_.remove(*child);
  unit_title_.set_text(at->title);
  unit_title_.set_tooltip_text(at->title);
  unit_body_.append(*label(at->purpose.empty()
                               ? "Intenção ainda não informada — opcional"
                               : at->purpose));
  if (!at->summary.empty())
    unit_body_.append(*label(at->summary));
  if (!at->perspective.empty())
    unit_body_.append(*label("Ponto de vista planejado: " + at->perspective));
  auto *actions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  button(*actions, "Editar parte…", [this] { edit_unit(); });
  button(*actions, "Datas e horários…", [this, id] {
    if (auto *w = owner_window())
      show_temporal_context(*w, service_, "unit", id);
  });
  button(*actions, "Escrever…", [this] { choose_document(true, true); });
  button(*actions, "Continuidade…", [this] { show_continuity(); });
  unit_body_.append(*actions);
  unit_body_.append(*label("Ordem do plano narrativo, cronologia, ordem de escrita e apresentação editorial são independentes. Esta parte não reorganiza o manuscrito."));
  auto *orders = section(unit_body_, "Ordens e trabalho");
  const bool writing = order_combo_.get_active_id() == "writing";
  const bool in_line = order_combo_.get_active_id() == "line";
  bool queued = false;
  std::string local;
  for (auto &c : s.contexts)
    if (c.unit_id == id) {
      queued = c.writing_position.has_value();
      local = c.location_name;
    }
  orders->append(*label(
      in_line ? "Movimento na ordem própria da linha selecionada — não altera "
                "montagem, outras linhas, fila, datas ou editorial"
      : writing ? "Movimento na ordem de escrita — não altera ordem do plano narrativo, datas ou editorial"
                : "Movimento na ordem do plano narrativo — não altera ordem de escrita, datas ou editorial"));
  auto *moves = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  button(*moves, "Mover antes", [this] {
    move_unit(-1);
  })->set_sensitive(!writing || queued);
  button(*moves, "Mover depois", [this] {
    move_unit(1);
  })->set_sensitive(!writing || queued);
  button(*moves, "Trocar ordem", [this, writing] {
    order_combo_.set_active_id(writing ? "narrative" : "writing");
  });
  orders->append(*moves);
  button(*orders,
         queued ? "Retirar da ordem de escrita" : "Adicionar à ordem de escrita",
         [this, observed, id, queued] {
           try {
             service_.builder().queue_unit(*observed, id, !queued);
             refresh_detail();
           } catch (const std::exception &e) {
             show_error(e);
           }
         });
  auto *space = section(unit_body_, local.empty() ? "Local — não declarado"
                                                  : "Local — " + local);
  space->append(*label("Contexto planejado. Não cria presença, coordenada, viagem ou fato no cadastro do mundo da história."));
  button(*space, "Escolher Local…", [this] { choose_location(); });
  if (!local.empty())
    button(*space, "Retirar contexto de Local", [this, observed, id] {
      try {
        service_.builder().set_location(*observed, id, "");
        refresh_detail();
      } catch (const std::exception &e) {
        show_error(e);
      }
    });
  auto *associations =
      section(unit_body_, "Linhas, elementos da história e relações em foco");
  button(*associations, "Vincular à linha…", [this] { connect_unit_line(); });
  button(*associations, "Vincular elemento da história…",
         [this] { connect_unit_entity(); });
  button(*associations, "Relação em foco…", [this] { choose_relation(); });
  for (auto &m : service_.structures().unit_lines(s.structure_id))
    if (m.unit_id == id) {
      std::string name = m.line_id;
      for (auto &l : s.lines)
        if (l.id == m.line_id)
          name = l.name;
      button(*associations, "Retirar da linha: " + name,
             [this, id, line = m.line_id] {
               try {
                 service_.structures().remove_unit_from_line(id, line);
                 refresh_detail();
               } catch (const std::exception &e) {
                 show_error(e);
               }
             });
    }
  for (auto &m : service_.structures().unit_entities(s.structure_id))
    if (m.unit_id == id) {
      auto e = service_.narrative().entity(m.entity_id);
      button(*associations,
             "Retirar elemento da história: " + (e ? e->name : m.entity_id) + " · " +
                 m.role,
             [this, id, m] {
               try {
                 service_.structures().remove_entity_from_unit(id, m.entity_id,
                                                               m.role);
                 refresh_detail();
               } catch (const std::exception &e) {
                 show_error(e);
               }
             });
    }
  auto *presence=section(unit_body_,"Presença planejada — declaração explícita");
  presence->append(*label("Vincular ou mencionar um elemento da história não declara presença. Esta escolha usa o Local e uma data marcada como Ocorrência nesta parte; não cria fato no cadastro do mundo da história."));
  std::vector<std::string> candidates;
  for(const auto &m:s.entities)if(m.unit_id==id && std::find(candidates.begin(),candidates.end(),m.entity_id)==candidates.end())candidates.push_back(m.entity_id);
  for(const auto &v:s.presences)if(v.unit_id==id && std::find(candidates.begin(),candidates.end(),v.entity_id)==candidates.end())candidates.push_back(v.entity_id);
  for(const auto &entity:candidates) {
    const auto e=service_.narrative().entity(entity);
    const bool present=std::any_of(s.presences.begin(),s.presences.end(),[&](auto &v){return v.unit_id==id && v.entity_id==entity;});
    button(*presence,std::string(present?"Retirar presença planejada: ":"Declarar presença planejada: ")+(e?e->name:entity),[this,observed,id,entity,present]{
      try{service_.builder().declare_presence(*observed,id,entity,!present);refresh_detail();}catch(const std::exception &e){show_error(e);}});
  }
  for (auto &r : s.relations)
    if (r.unit_id == id)
      button(*associations, "Retirar relação em foco: " + r.title,
             [this, observed, r] {
               try {
                 service_.builder().detach(*observed, "relation", r.id);
                 refresh_detail();
               } catch (const std::exception &e) {
                 show_error(e);
               }
             });
  auto *docs = section(unit_body_, "Documentos que desenvolvem esta parte");
  docs->append(
      *label("Associação declarada; não afirma que todo o texto é esta parte. Retirar associação preserva o Documento."));
  button(*docs, "Vincular Documento existente…",
         [this] { choose_document(false); });
  button(*docs, "Criar Documento para desenvolver…",
         [this] { choose_document(true); });
  button(*docs,"Associar trecho ou uso de bloco…",[this]{choose_text_target();});
  for (auto &r : s.realizations)
    if (r.unit_id == id) {
      auto *row=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL,8);
      auto *open=button(*row,"Abrir escrita: "+r.title,[this,r]{signal_realization_requested_.emit(r);});
      open->set_hexpand(true);open->set_halign(Gtk::Align::FILL);
      open->set_sensitive(r.status!="missing" && r.status!="unresolved" && r.status!="empty");
      auto *more=Gtk::make_managed<Gtk::MenuButton>();row->append(*more);
      configure_overflow_button(*more,{{"Conferir texto e planejamento…",[this,r]{realization_actions(r);},"document-edit-symbolic"},
        {"Substituir por outro trecho/ocorrência…",[this,r]{choose_text_target(r.id);},"insert-link-symbolic"},
        {"Desvincular texto: "+r.title,[this,observed,r]{try{service_.builder().detach(*observed,r.table=="narrative_text_targets"?"target":"document",r.id);refresh_detail();}catch(const std::exception &e){show_error(e);}},"list-remove-symbolic"}},"Ações deste texto associado");
      docs->append(*row);
      if(r.table=="narrative_text_targets")docs->append(*label("Alvo: "+std::string(r.anchor_id.empty()?"uso de bloco na montagem":"trecho marcado")+" · "+realization_status_label(r.status)+
        " · "+realization_review_label(r.review_state)+(r.note.empty()?"":" · "+r.note)));
    }
  auto *advanced = section(unit_body_, "Encadeamento e remoção");
  button(*advanced, "Novo vínculo narrativo…", [this] { add_link(); });
  button(*advanced, "Remover parte…", [this] { delete_unit(); });
  for (auto *child = unit_body_.get_first_child(); child;
       child = child->get_next_sibling())
    if (auto *e = dynamic_cast<Gtk::Expander *>(child))
      e->set_expanded(std::find(expanded.begin(), expanded.end(),
                                e->get_label().raw()) != expanded.end());
  Glib::signal_idle().connect_once(sigc::track_obj(
      [this, scroll] { unit_scroll_.get_vadjustment()->set_value(scroll); },
      *this));
}
void NarrativeStructureWorkspace::move_unit(int direction) {
  if (!selected_unit_id_)
    return;
  try {
    if (!owner_window())
      return;
    const auto expected = builder_snapshot_;
    const auto id = *selected_unit_id_;
    const bool line = order_combo_.get_active_id() == "line";
    const auto line_id = selected_line_id_.value_or("");
    const auto order = order_combo_.get_active_id() == "writing"
                           ? application::BuilderOrder::WritingQueue
                           : application::BuilderOrder::Narrative;
    std::string text;
    if (line)
      text = "Só a posição de associação na linha selecionada mudará. A "
             "montagem, as outras linhas, a fila, as datas, os Documentos e o "
             "editorial serão preservados.";
    else {
      const auto impact =
          service_.builder().preview_move(expected, id, order, direction);
      text = "Ordem alterada: " +
             std::string(order == application::BuilderOrder::Narrative
                             ? "ordem do plano narrativo"
                             : "ordem de escrita") +
             ".\n" + std::to_string(impact.connected_units.size()) +
             " partes na vizinhança declarada; " +
             std::to_string(impact.documents) + " Documentos e " +
             std::to_string(impact.dates) +
             " vínculos temporais preservados. Vizinhança não comprova "
             "consequência ou inconsistência.";
      for (const auto &warning : impact.warnings)
        text += "\n\n" + warning;
    }
    auto *d = new OverlayDialog("Prévia do movimento", *owner_window(), true);
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_min_content_height(160);
    scroll->set_max_content_height(320);
    scroll->set_propagate_natural_height(true);
    scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll->set_child(*label(text));
    d->get_content_area()->append(*scroll);
    d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    d->add_button("Confirmar movimento", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect(
        [this, d, expected, id, line, line_id, order, direction](int response) {
          if (response != Gtk::ResponseType::ACCEPT)
            return;
          try {
            if (line)
              service_.builder().move_in_line(expected, id, line_id, direction);
            else
              service_.builder().move_unit(expected, id, order, direction);
            refresh_detail();
          } catch (const std::exception &e) {
            d->set_secondary_text(author::diagnostic(e.what()));
            d->keep_open_on_response();
          }
        });
    d->present();
  } catch (const std::exception &e) {
    show_error(e);
  }
}
void NarrativeStructureWorkspace::edit_unit() {
  if (!selected_unit_id_ || !owner_window())
    return;
  auto at = std::find_if(
      builder_snapshot_.units.begin(), builder_snapshot_.units.end(),
      [this](auto &u) { return u.id == *selected_unit_id_; });
  if (at == builder_snapshot_.units.end())
    return;
  const auto expected = builder_snapshot_;
  const auto value = *at;
  auto *d =
      new OverlayDialog("Editar parte da narrativa", *owner_window(), true);
  d->set_secondary_text("Editar intenção e descrição não altera nenhuma ordem, "
                        "data, presença ou texto.");
  auto *body = d->get_content_area();
  auto field = [&](const char *name, const std::string &text) {
    auto *e = Gtk::make_managed<Gtk::Entry>();
    e->set_text(text);
    append_labeled_form_field(*body, name, *e);
    return e;
  };
  auto *title = field("Título", value.title),
       *designator = field("Código curto", value.designator),
       *summary = field("Resumo", value.summary),
       *purpose = field("Função desta parte", value.purpose),
       *perspective = field("Ponto de vista planejado", value.perspective);
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button("Salvar parte", Gtk::ResponseType::ACCEPT);
  d->signal_response().connect([this, d, expected, value, title, designator,
                                summary, purpose, perspective](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      auto v = value;
      v.title = title->get_text();
      v.designator = designator->get_text();
      v.summary = summary->get_text();
      v.purpose = purpose->get_text();
      v.perspective = perspective->get_text();
      service_.builder().update_unit(expected, v);
      refresh_detail();
    } catch (const std::exception &e) {
      d->set_secondary_text(author::diagnostic(e.what()));
      d->keep_open_on_response();
    }
  });
  d->present();
}
void NarrativeStructureWorkspace::edit_line() {
  if (!selected_line_id_ || !owner_window())
    return;
  auto at = std::find_if(
      builder_snapshot_.lines.begin(), builder_snapshot_.lines.end(),
      [this](auto &l) { return l.id == *selected_line_id_; });
  if (at == builder_snapshot_.lines.end())
    return;
  const auto expected = builder_snapshot_;
  const auto value = *at;
  auto *d = new OverlayDialog("Editar linha narrativa", *owner_window(), true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(value.name);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(value.description);
  append_labeled_form_field(*d->get_content_area(), "Nome", *name);
  append_labeled_form_field(*d->get_content_area(),
                            "Descrição / pergunta da linha", *description);
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button("Salvar linha", Gtk::ResponseType::ACCEPT);
  d->signal_response().connect(
      [this, d, expected, value, name, description](int r) {
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        try {
          auto v = value;
          v.name = name->get_text();
          v.description = description->get_text();
          service_.builder().update_line(expected, v);
          refresh_detail();
        } catch (const std::exception &e) {
          d->set_secondary_text(author::diagnostic(e.what()));
          d->keep_open_on_response();
        }
      });
  d->present();
}
void NarrativeStructureWorkspace::choose_location() {
  if (!selected_unit_id_ || !owner_window())
    return;
  const auto s = builder_snapshot_;
  const auto id = *selected_unit_id_;
  auto *d = new OverlayDialog("Contexto de Local", *owner_window(), true);
  auto *selector = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar Local", [this](const std::string &search, std::size_t limit) {
        persistence::EntityQuery q;
        q.search = search;
        q.limit = limit;
        q.entity_type_ids = {"00000000-0000-4000-9000-000000000002"};
        std::vector<IncrementalSelection> r;
        for (auto &e : service_.narrative().entities(q))
          r.push_back({e.id, e.name, e.summary});
        return r;
      });
  d->get_content_area()->append(*selector);
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button("Usar Local", Gtk::ResponseType::ACCEPT);
  d->signal_response().connect([this, d, s, id, selector](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      if (!selector->selected_id())
        throw std::runtime_error("Escolha um Local");
      service_.builder().set_location(s, id, *selector->selected_id());
      refresh_detail();
    } catch (const std::exception &e) {
      d->set_secondary_text(author::diagnostic(e.what()));
      d->keep_open_on_response();
    }
  });
  d->present();
}
void NarrativeStructureWorkspace::choose_relation() {
  if (!selected_unit_id_ || !owner_window())
    return;
  const auto s = builder_snapshot_;
  const auto id = *selected_unit_id_;
  auto *d = new OverlayDialog("Relação em foco", *owner_window(), true);
  d->set_secondary_text("Selecionar não altera a relação registrada no mundo da história nem declara uma transformação.");
  auto *selector = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar relação",
      [this](const std::string &search, std::size_t limit) {
        persistence::RelationQuery q;
        q.search = search;
        q.limit = limit;
        std::vector<IncrementalSelection> r;
        for (auto &e : service_.narrative().relations(q)) {
          auto a = service_.narrative().entity(e.source_entity_id),
               b = service_.narrative().entity(e.target_entity_id);
          r.push_back({e.id, (a ? a->name : "") + " → " + (b ? b->name : ""),
                       e.description});
        }
        return r;
      });
  d->get_content_area()->append(*selector);
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button("Associar relação", Gtk::ResponseType::ACCEPT);
  d->signal_response().connect([this, d, s, id, selector](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      if (!selector->selected_id())
        throw std::runtime_error("Escolha uma relação");
      service_.builder().attach_relation(s, id, *selector->selected_id());
      refresh_detail();
    } catch (const std::exception &e) {
      d->set_secondary_text(author::diagnostic(e.what()));
      d->keep_open_on_response();
    }
  });
  d->present();
}
void NarrativeStructureWorkspace::choose_document(bool create,
                                                  bool prefer_existing) {
  if (!selected_unit_id_ || !owner_window())
    return;
  const auto s = builder_snapshot_;
  const auto id = *selected_unit_id_;
  // A unique existing realization is an explicit destination, not an inferred
  // choice among alternatives. Several realizations stay in the author's list.
  std::vector<application::BuilderRealization> docs;
  for (auto &r : s.realizations)
    if (r.unit_id == id)
      docs.push_back(r);
  if (prefer_existing && docs.size() == 1) {
    signal_realization_requested_.emit(docs.front());
    return;
  }
  if (prefer_existing && docs.size() > 1) {
    for (auto *child = unit_body_.get_first_child(); child;
         child = child->get_next_sibling())
      if (auto *e = dynamic_cast<Gtk::Expander *>(child);
          e && e->get_label() == "Documentos que desenvolvem esta parte")
        e->set_expanded(true);
    signal_status_message_.emit(
        "Escolha qual texto associado deseja escrever; nenhum Documento foi "
        "escolhido automaticamente.");
    return;
  }
  auto *d = new OverlayDialog(create ? "Criar texto que desenvolve esta parte"
                                     : "Vincular texto que desenvolve esta parte",
                              *owner_window(), true);
  d->set_secondary_text(
      "O Documento terá identidade própria. Não será colocado no manuscrito automaticamente; nenhuma ordem será alterada.");
  auto *title = create ? Gtk::make_managed<Gtk::Entry>() : nullptr;
  auto *selector =
      create ? nullptr
             : Gtk::make_managed<IncrementalSelector>(
                   "Pesquisar Documento",
                   [this](const std::string &search, std::size_t limit) {
                     persistence::DocumentQuery q;
                     q.search = search;
                     q.limit = limit;
                     std::vector<IncrementalSelection> r;
                     for (auto &v : service_.writing().document_summaries(q))
                       r.push_back({v.id, v.title, "Documento independente"});
                     return r;
                   });
  if (create) {
    for (auto &u : s.units)
      if (u.id == id)
        title->set_text(u.title);
    append_labeled_form_field(*d->get_content_area(), "Título do Documento",
                              *title);
  } else
    d->get_content_area()->append(*selector);
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button(create ? "Criar e escrever" : "Vincular Documento",
                Gtk::ResponseType::ACCEPT);
  d->signal_response().connect(
      [this, d, s, id, create, title, selector](int r) {
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        try {
          std::string doc;
          if (create)
            doc = service_.builder().create_document(s, id, title->get_text());
          else {
            if (!selector->selected_id())
              throw std::runtime_error("Escolha um Documento");
            doc = *selector->selected_id();
            service_.builder().attach_document(s, id, doc);
          }
          refresh_detail();
          if (create)
            signal_write_requested_.emit(doc);
        } catch (const std::exception &e) {
          d->set_secondary_text(author::diagnostic(e.what()));
          d->keep_open_on_response();
        }
      });
  d->present();
}
ViewState NarrativeStructureWorkspace::capture_view() const {
  ViewState::Array sections;
  for (auto *child = unit_body_.get_first_child(); child;
       child = child->get_next_sibling())
    if (auto *e = dynamic_cast<const Gtk::Expander *>(child);
        e && e->get_expanded())
      sections.emplace_back(e->get_label().raw());
  return ViewState::Object{
      {"sections", sections},
      {"work", work_combo_.get_active_id().raw()},
      {"structure", view_id(selected_structure_id_)},
      {"unit", view_id(selected_unit_id_)},
      {"line", view_id(selected_line_id_)},
      {"page", stack_.get_visible_child_name().raw()},
      {"detail_section", detail_sections_.get_visible_child_name().raw()},
      {"order", order_combo_.get_active_id().raw()},
      {"continuity_filter",continuity_filter_.get_active_id().raw()},
      {"continuity_page",static_cast<std::int64_t>(continuity_page_index_)},
      {"continuity_scroll",scroll_state(continuity_scroll_.get_vadjustment()->get_value())},
      {"continuity_origin",continuity_origin_},
      {"scroll", scroll_state(unit_scroll_.get_vadjustment()->get_value())}};
}
void NarrativeStructureWorkspace::restore_view(const ViewState &v) {
  const auto work = v.at("work").as_string(), page = v.at("page").as_string(),
             order = v.at("order").as_string();
  const auto structure = view_id(v.at("structure")),
             unit = view_id(v.at("unit")), line = view_id(v.at("line"));
  if ((page != "library" && page != "detail" && page != "roles" &&
       page != "unit" && page != "comparison" && page != "continuity") ||
      (order != "writing" && order != "narrative" && order != "line"))
    throw std::runtime_error("Contexto narrativo inválido");
  refresh_works();
  if (!work_combo_.set_active_id(work)) {
    show_library();
    return;
  }
  selected_structure_id_ = structure;
  selected_unit_id_ = unit;
  selected_line_id_ = line;
  order_combo_.set_active_id(order);
  if (v.as_object().contains("detail_section")) {
    const auto section=v.at("detail_section").as_string();
    if (section!="units" && section!="lines" && section!="links")
      throw std::runtime_error("Seção narrativa inválida");
    detail_sections_.set_visible_child(section);
  } else
    detail_sections_.set_visible_child("units");
  if (page == "roles")
    show_roles();
  else if (page == "comparison")
    show_library(); // Derived comparison must be recomputed, not restored as an
                    // approval.
  else if (structure && page != "library") {
    stack_.set_visible_child(page == "unit" ? "unit" : "detail");
    refresh_detail();
    if(page=="continuity"){
      show_continuity();
      if(v.as_object().contains("continuity_filter")){
        const auto filter=v.at("continuity_filter").as_string();
        if(filter!="all" && filter!="reconciliation" && filter!="insufficient" && filter!="presence" && filter!="dependencies")
          throw std::runtime_error("Filtro de continuidade inválido");
        continuity_filter_.set_active_id(filter);
        continuity_page_index_=view_offset(v.at("continuity_page"));
        if(continuity_page_index_>1000)throw std::runtime_error("Página de continuidade inválida");
        const auto origin=v.at("continuity_origin").as_string();
        if(origin!="unit" && origin!="detail")throw std::runtime_error("Origem de continuidade inválida");
        continuity_origin_=origin;render_continuity();
        const auto scroll=view_scroll(v.at("continuity_scroll"));
        Glib::signal_idle().connect_once(sigc::track_obj([this,scroll]{continuity_scroll_.get_vadjustment()->set_value(scroll);},*this));
      }
    }
  } else
    show_library();
  if (v.as_object().contains("sections")) {
    const auto &sections = v.at("sections").as_array();
    if (sections.size() > 8)
      throw std::runtime_error("Contexto de seções inválido");
    for (auto *child = unit_body_.get_first_child(); child;
         child = child->get_next_sibling())
      if (auto *e = dynamic_cast<Gtk::Expander *>(child))
        e->set_expanded(
            std::any_of(sections.begin(), sections.end(), [&](auto &name) {
              const auto &saved = name.as_string();
              const auto displayed = e->get_label().raw();
              // Restore only known former section captions. Author names,
              // work IDs and saved positions are not vocabulary-translated.
              return saved == displayed ||
                  (saved == "Linhas, entidades e relações em foco" &&
                   displayed == "Linhas, elementos da história e relações em foco") ||
                  (saved == "Documentos que desenvolvem esta unidade" &&
                   displayed == "Documentos que desenvolvem esta parte");
            }));
  }
  const auto scroll = view_scroll(v.at("scroll"));
  Glib::signal_idle().connect_once(sigc::track_obj(
      [this, scroll] { unit_scroll_.get_vadjustment()->set_value(scroll); },
      *this));
}
} // namespace inde::ui
