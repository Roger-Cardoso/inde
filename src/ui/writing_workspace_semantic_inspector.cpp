#include "inde/ui/accessibility.hpp"
#include "writing_semantic_explorer_support.hpp"

namespace inde::ui {
using namespace semantic_ui;
void WritingWorkspace::show_semantic_layers(std::size_t offset) {
  if (!current_document_)
    return;
  try {
    require_completed_composition();
    commit_undo_group();
    auto d = capture_editor_state();
    application::SemanticAnchorEngine::inspect(d);
    if (semantic_explorer_document_id_ != d.id && !semantic_project_scope_) {
      semantic_filter_ = {};
      semantic_explorer_document_id_ = d.id;
      offset = 0;
    }
    const auto types = service_.writing().semantic_types();
    const auto diagnostics = project::semantic_diagnostics(d, types);
    while (auto *child = semantic_page_.get_first_child())
      semantic_page_.remove(*child);
    auto *header =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    auto *back = Gtk::make_managed<Gtk::Button>("Voltar ao texto");
    back->signal_clicked().connect([this] {
      page_stack_.set_visible_child("editor");
      editor_.grab_focus();
    });
    header->append(*back);
    auto *heading = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    label(*heading, "Marcações do texto do Documento")
        ->add_css_class("title-1");
    label(*heading, clipped(d.title, 90))->add_css_class("dim-label");
    heading->set_hexpand(true);
    header->append(*heading);
    auto *distribution=Gtk::make_managed<Gtk::Button>("Distribuição das marcações");
    distribution->signal_clicked().connect([this]{show_semantic_distribution();});header->append(*distribution);
    semantic_page_.append(*header);
    auto *perspective_box =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    const auto pc = application::TextSemanticService::perspective_at(
        d, editor_.get_buffer()
               ->get_iter_at_mark(editor_.get_buffer()->get_insert())
               .get_offset());
    auto *perspective_summary = label(
        *perspective_box,
        pc.ambiguous ? "Ponto de vista aqui: intervalos sobrepostos; nenhuma declaração escolhida automaticamente."
        : pc.incomplete
            ? "Ponto de vista aqui: declaração incompleta; revise os campos."
        : pc.effective_declaration
            ? "Ponto de vista aqui: " +
                  std::string(pc.interval_declarations.empty()
                                  ? "para todo o documento"
                                  : "deste trecho")
            : "Ponto de vista: nenhuma declaração aplicável ao cursor.");
    perspective_summary->set_hexpand(true);
    button(*perspective_box, "Ponto de vista do documento",
           [this] { show_document_perspective(); });
    semantic_page_.append(*perspective_box);
    button(semantic_page_,"Consultar avisos temporais do Documento",[this]{show_temporal_notices();});
    auto *scope = Gtk::make_managed<Gtk::ComboBoxText>();
    scope->append("document", "Documento atual — inclui edições não salvas");
    scope->append("project", "Projeto / Obra — marcações persistidas");
    scope->set_active_id(semantic_project_scope_ ? "project" : "document");
    set_accessible_label(*scope, "Onde procurar marcações");
    semantic_page_.append(*scope);
    auto *advanced =
        Gtk::make_managed<Gtk::Expander>("Tipos de marcação personalizados");
    auto *advanced_box =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    button(*advanced_box, "Criar tipo de marcação personalizado",
           [this] { register_semantic_type(); });
    advanced->set_child(*advanced_box);
    semantic_page_.append(*advanced);

    auto *filters =
        Gtk::make_managed<Gtk::Expander>("Pesquisa e filtros das camadas");
    auto *filter_box =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    filter_box->add_css_class("tool-section");
    auto *search = Gtk::make_managed<Gtk::SearchEntry>();
    search->set_placeholder_text(
        "Pesquisar trecho, campos, notas ou identidade");
    search->set_text(semantic_filter_.search);
    set_accessible_label(*search, "Pesquisar camadas do Documento");
    filter_box->append(*search);
    auto *choices =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto *role = Gtk::make_managed<Gtk::ComboBoxText>();
    role->append("", "Todos os tipos de marcação");
    std::map<std::string, std::string> roles;
    for (auto &t : types)
      roles[t.key] = author::annotation_label(t);
    for (auto &a : d.semantics.annotations)
      if (!roles.contains(a.type_key))
        roles[a.type_key] = a.type_key + " (indisponível)";
    for (auto &[key, name] : roles)
      role->append(key, name);
    role->set_active_id(semantic_filter_.role);
    if (semantic_project_scope_)
      role->set_active_id(semantic_use_filter_.type_key.value_or(""));
    set_accessible_label(*role, "Filtrar marcações por tipo");
    auto *status = Gtk::make_managed<Gtk::ComboBoxText>();
    status->append("", "Todos os estados");
    for (const auto &s : {"active", "incomplete", "archived"})
      status->append(s, status_label(s));
    status->set_active_id(semantic_filter_.status);
    set_accessible_label(*status, "Filtrar camadas por estado");
    auto *anchor = Gtk::make_managed<Gtk::ComboBoxText>();
    anchor->append("", "Qualquer trecho marcado");
    for (const auto &s : {"resolved", "ambiguous", "orphaned", "unavailable"})
      anchor->append(s, anchor_label(s));
    anchor->append("document", "Para todo o documento — sem trecho específico");
    anchor->set_active_id(semantic_filter_.anchor);
    set_accessible_label(*anchor, "Filtrar camadas por trecho marcado");
    for (auto *combo : {role, status, anchor}) {
      combo->set_hexpand(true);
      choices->append(*combo);
    }
    filter_box->append(*choices);
    auto *issues =
        Gtk::make_managed<Gtk::CheckButton>("Somente com pendências");
    issues->set_active(semantic_filter_.issues_only);
    filter_box->append(*issues);
    // Project-wide filters share the explorer; no second modal or competing
    // presentation. Document-only filters remain explicit about their scope.
    search->set_sensitive(!semantic_project_scope_);
    status->set_sensitive(!semantic_project_scope_);
    anchor->set_sensitive(!semantic_project_scope_);
    issues->set_sensitive(!semantic_project_scope_);
    search->set_visible(!semantic_project_scope_);
    status->set_visible(!semantic_project_scope_);
    anchor->set_visible(!semantic_project_scope_);
    issues->set_visible(!semantic_project_scope_);
    auto *project_filters =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    auto *category = Gtk::make_managed<Gtk::Entry>();
    category->set_placeholder_text("Categoria (chave exata, opcional)");
    category->set_text(semantic_use_filter_.category.value_or(""));
    auto *work = Gtk::make_managed<Gtk::ComboBoxText>();
    work->append("", "Todas as Obras e Documentos livres");
    for (const auto &w : service_.catalog().works)
      work->append(w.id, w.title);
    work->set_active_id(semantic_use_filter_.work_id.value_or(""));
    auto *entity = Gtk::make_managed<IncrementalSelector>(
        "Filtrar usos por elemento da história",
        [this](const std::string &s, std::size_t limit) {
          persistence::EntityQuery q;
          q.search = s;
          q.limit = limit;
          std::vector<IncrementalSelection> out;
          for (auto &e : service_.narrative().entities(q))
            out.push_back({e.id, e.name, e.summary});
          return out;
        });
    if (semantic_use_filter_.entity_id)
      entity->set_selected(*semantic_use_filter_.entity_id,
                           entity_label(*semantic_use_filter_.entity_id));
    auto *archived =
        Gtk::make_managed<Gtk::CheckButton>("Incluir marcações arquivadas");
    archived->set_active(semantic_use_filter_.include_archived);
    auto *annotation_scope = Gtk::make_managed<Gtk::ComboBoxText>();
    annotation_scope->append("", "Intervalos e declarações globais");
    annotation_scope->append("interval", "Somente marcações de intervalos");
    annotation_scope->append("document",
                             "Somente perspectiva para todo o documento");
    annotation_scope->set_active_id(
        semantic_use_filter_.annotation_scope
            ? project::to_string(*semantic_use_filter_.annotation_scope)
            : "");
    auto *time_filters = Gtk::make_managed<Gtk::Expander>(
        "Contexto no tempo da história (opcional)");
    auto *time_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    auto *time_axis = Gtk::make_managed<Gtk::ComboBoxText>();
    time_axis->append("", "Todas as linhas do tempo — sem filtro temporal");
    for (const auto &a : service_.planning().time_axes())
      time_axis->append(a.id, a.name);
    time_axis->set_active_id(
        semantic_use_filter_.fictional_axis_id.value_or(""));
    auto *time_point = Gtk::make_managed<IncrementalSelector>(
        "Pesquisar ponto ficcional do filtro",
        [this, time_axis](const std::string &s, std::size_t limit) {
          std::vector<IncrementalSelection> out;
          auto axis = time_axis->get_active_id().raw();
          if (axis.empty())
            return out;
          persistence::PlanningQuery q;
          q.search = s;
          q.limit = limit;
          for (const auto &p : service_.planning().time_points(axis, q))
            out.push_back(
                {p.id, p.label, "Ordem: " + std::to_string(p.ordinal)});
          return out;
        });
    if (semantic_use_filter_.fictional_point_id) {
      auto p = service_.planning().time_point(
          *semantic_use_filter_.fictional_point_id);
      if (p)
        time_point->set_selected(p->id, p->label);
    }
    time_axis->signal_changed().connect([time_point] {
      time_point->clear_selection();
      time_point->refresh();
    });
    time_box->append(*time_axis);
    time_box->append(*time_point);
    label(*time_box, "Por ponto: inclui vínculos pontuais e períodos fechados "
                     "que cobrem esse ponto. Só vínculos declarados pelo "
                     "autor; não interpreta datas do texto.");
    button(*time_box, "Limpar marco na linha do tempo do filtro",
           [time_point] { time_point->clear_selection(); });
    time_filters->set_child(*time_box);
    time_filters->set_expanded(
        semantic_use_filter_.fictional_axis_id.has_value());
    {
      label(*project_filters,
            "Busca no projeto: tipo de marcação, categoria, elemento da história e Obra. Trecho, estado do trecho marcado e pendências são filtros do Documento atual.");
      project_filters->append(*category);
      project_filters->append(*work);
      project_filters->append(*entity);
      project_filters->append(*archived);
      project_filters->append(*annotation_scope);
      project_filters->append(*time_filters);
      button(*project_filters, "Limpar elemento da história do filtro",
             [entity] { entity->clear_selection(); });
    }
    filter_box->append(*project_filters);
    project_filters->set_visible(semantic_project_scope_);
    auto *actions =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    actions->set_halign(Gtk::Align::END);
    auto apply = [this, search, role, status, anchor, issues, category, work,
                  entity, archived, annotation_scope, time_axis, time_point] {
      semantic_filter_ = {search->get_text().raw(), role->get_active_id().raw(),
                          status->get_active_id().raw(),
                          anchor->get_active_id().raw(), issues->get_active()};
      auto optional = [](const std::string &s) -> std::optional<std::string> {
        return s.empty() ? std::nullopt : std::optional<std::string>(s);
      };
      semantic_use_filter_.type_key = optional(role->get_active_id().raw());
      semantic_use_filter_.category = optional(category->get_text().raw());
      semantic_use_filter_.work_id = optional(work->get_active_id().raw());
      semantic_use_filter_.entity_id = entity->selected_id();
      semantic_use_filter_.include_archived = archived->get_active();
      const auto scope_key = annotation_scope->get_active_id().raw();
      semantic_use_filter_.annotation_scope =
          scope_key.empty() ? std::nullopt
                            : std::optional<project::AnnotationScope>(
                                  project::annotation_scope(scope_key));
      semantic_use_filter_.fictional_axis_id =
          optional(time_axis->get_active_id().raw());
      semantic_use_filter_.fictional_point_id = time_point->selected_id();
      show_semantic_layers();
    };
    button(*actions, "Limpar filtros", [this] {
      semantic_filter_ = {};
      semantic_use_filter_ = {};
      show_semantic_layers();
    });
    button(*actions, "Buscar camadas", apply);
    search->signal_activate().connect(apply);
    auto *filter_scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    filter_scroll->set_policy(Gtk::PolicyType::NEVER,
                              Gtk::PolicyType::AUTOMATIC);
    filter_scroll->set_min_content_height(80);
    filter_scroll->set_max_content_height(280);
    filter_scroll->set_propagate_natural_height(true);
    filter_scroll->set_child(*filter_box);
    auto *filter_surface =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    filter_surface->append(*filter_scroll);
    filter_surface->append(*actions);
    filters->set_child(*filter_surface);
    const bool filtered =
        !semantic_filter_.search.empty() || !semantic_filter_.role.empty() ||
        !semantic_filter_.status.empty() || !semantic_filter_.anchor.empty() ||
        semantic_filter_.issues_only;
    filters->set_expanded(filtered);
    semantic_page_.append(*filters);
    scope->signal_changed().connect([this, scope] {
      semantic_project_scope_ = scope->get_active_id() == "project";
      const auto epoch = edit_epoch_;
      std::weak_ptr<int> life = clipboard_lifetime_;
      // ComboBox still emits property notifications after changed. Destroying
      // it synchronously during that emission makes GTK touch a dead object.
      Glib::signal_idle().connect_once([this, life, epoch] {
        if (!life.expired() && edit_epoch_ == epoch)
          show_semantic_layers();
      });
    });
    if (semantic_project_scope_) {
      semantic_use_filter_.offset = offset;
      semantic_use_filter_.limit = 24;
      filters->set_expanded(true);
      render_semantic_uses(semantic_page_, semantic_use_filter_);
      page_stack_.set_visible_child("semantic-layers");
      back->grab_focus();
      return;
    }

    std::vector<const project::TextAnnotation *> matches;
    const auto needle = Glib::ustring(semantic_filter_.search).casefold();
    const Glib::ustring document_text(d.content);
    const auto character_count = document_text.size();
    std::map<std::string, std::string> entity_names;
    for (auto &a : d.semantics.annotations) {
      const auto *t =
          project::find_annotation_type(types, a.type_key, a.type_version);
      const bool pending =
          std::any_of(diagnostics.begin(), diagnostics.end(),
                      [&](auto &diag) { return diag.annotation_id == a.id; });
      if ((!semantic_filter_.role.empty() &&
           a.type_key != semantic_filter_.role) ||
          (!semantic_filter_.status.empty() &&
           a.status != semantic_filter_.status) ||
          (!semantic_filter_.anchor.empty() &&
           anchor_key(d, a) != semantic_filter_.anchor) ||
          (semantic_filter_.issues_only && !pending))
        continue;
      if (needle.empty()) {
        matches.push_back(&a);
        continue;
      }
      std::string corpus = a.id + " " + a.type_key + " " + (t ? t->label + " " + author::annotation_label(*t) : "") +
                           " " + a.notes + " " + a.source;
      // Search the complete resolved range, not only the clipped card preview.
      if (const auto *e = evidence(d, a);
          e && e->state == project::AnchorResolution::Resolved)
        for (auto &k : d.anchors)
          if (k.id == a.anchor_id && k.start_offset <= k.end_offset &&
              k.end_offset <= character_count)
            corpus +=
                " " + document_text
                          .substr(k.start_offset, k.end_offset - k.start_offset)
                          .raw();
      for (auto &[key, value] : a.fields) {
        corpus += " " + key;
        for (auto &v : value.values) {
          // Raw text/IDs are searchable verbatim. Only enums add translated
          // labels; never reinterpret a free-text value as an enum keyword.
          corpus += " " + v;
          if (value.kind == project::SemanticFieldKind::Entity) {
            if (!entity_names.contains(v))
              entity_names[v] = entity_label(v);
            corpus += " " + entity_names[v];
          } else if (value.kind == project::SemanticFieldKind::Enum)
            corpus += " " + value_label(v);
        }
      }
      for (const auto &b : a.fictional_time)
        corpus += " " + semantic_time_label(b) + " " + b.notes + " " + b.source;
      if (!needle.empty() &&
          Glib::ustring(corpus).casefold().find(needle) == Glib::ustring::npos)
        continue;
      matches.push_back(&a);
    }
    constexpr std::size_t page_size = 24;
    const auto n = matches.size();
    if (offset >= n)
      offset = n ? ((n - 1) / page_size) * page_size : 0;
    semantic_offset_ = offset;
    auto *summary =
        label(semantic_page_,
              std::to_string(n) + " resultado(s) de " +
                  std::to_string(d.semantics.annotations.size()) +
                  " marcação(ões) • página " +
                  std::to_string(offset / page_size + 1) + " de " +
                  std::to_string(std::max(std::size_t{1},
                                          (n + page_size - 1) / page_size)));
    summary->add_css_class("context-summary");
    summary->add_css_class("dim-label");
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll->set_propagate_natural_height(false);
    scroll->set_propagate_natural_width(false);
    scroll->set_vexpand(true);
    auto *cards = Gtk::make_managed<Gtk::FlowBox>();
    cards->set_selection_mode(Gtk::SelectionMode::NONE);
    cards->set_min_children_per_line(1);
    cards->set_max_children_per_line(4);
    cards->set_homogeneous(true);
    cards->set_column_spacing(12);
    cards->set_row_spacing(12);
    cards->set_valign(Gtk::Align::START);
    for (auto i = offset; i < std::min(offset + page_size, n); ++i) {
      const auto &a = *matches[i];
      const auto *t =
          project::find_annotation_type(types, a.type_key, a.type_version);
      auto *card = Gtk::make_managed<Gtk::Button>();
      card->add_css_class("entity-card");
      card->set_size_request(270, 140);
      card->set_hexpand(true);
      card->set_valign(Gtk::Align::START);
      auto *body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
      body->set_margin(12);
      auto *name = label(*body, a.scope == project::AnnotationScope::Document && a.type_key=="inde.semantic.perspective"
                                    ? "Ponto de vista do documento"
                                : t ? author::annotation_label(*t)
                                    : a.type_key + " (indisponível)");
      compact_label(*name);
      name->add_css_class("heading");
      auto *state = label(*body, status_label(a.status) + " • " +
                                     anchor_label(anchor_key(d, a)));
      compact_label(*state);
      state->add_css_class("dim-label");
      compact_label(*label(*body, excerpt(d, a)));
      const auto count =
          std::count_if(diagnostics.begin(), diagnostics.end(),
                        [&](auto &diag) { return diag.annotation_id == a.id; });
      if (count)
        compact_label(*label(*body, std::to_string(count) +
                                        " pendência(s) — abrir para revisar"));
      card->set_child(*body);
      card->set_tooltip_text("Abrir detalhe da anotação\n" + a.id);
      set_accessible_label(*card,
                           "Abrir anotação: " + (t ? author::annotation_label(*t) : a.type_key) +
                               ", " + status_label(a.status));
      card->signal_clicked().connect(
          [this, id = a.id] { show_semantic_annotation(id); });
      cards->append(*card);
    }
    if (!n) {
      auto *empty = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
      empty->set_margin(24);
      empty->add_css_class("content-card");
      label(*empty, d.semantics.annotations.empty()
                        ? "Este Documento ainda não tem marcações"
                        : "Nenhuma camada corresponde aos filtros");
      label(*empty, d.semantics.annotations.empty()
                        ? "Volte ao texto, selecione um trecho e use Marcar "
                          "seleção na barra do editor."
                        : "Limpe os filtros ou ajuste a pesquisa; as marcações "
                          "não foram removidas.");
      scroll->set_child(*empty);
    } else
      scroll->set_child(*cards);
    semantic_page_.append(*scroll);
    auto *pagination =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    pagination->set_halign(Gtk::Align::END);
    auto *previous =
        Gtk::make_managed<Gtk::Button>("Página anterior de camadas");
    auto *next = Gtk::make_managed<Gtk::Button>("Próxima página de camadas");
    previous->set_sensitive(offset > 0);
    next->set_sensitive(offset + page_size < n);
    previous->signal_clicked().connect([this, offset] {
      show_semantic_layers(offset >= page_size ? offset - page_size : 0);
    });
    next->signal_clicked().connect(
        [this, offset] { show_semantic_layers(offset + page_size); });
    pagination->append(*previous);
    pagination->append(*next);
    semantic_page_.append(*pagination);
    page_stack_.set_visible_child("semantic-layers");
    back->grab_focus();
  } catch (const std::exception &e) {
    show_error("Não foi possível explorar as camadas", e);
  }
}
} // namespace inde::ui
