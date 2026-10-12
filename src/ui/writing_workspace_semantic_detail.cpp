#include "inde/ui/author_diagnostics.hpp"
#include "writing_semantic_explorer_support.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"

namespace inde::ui {
using namespace semantic_ui;
void WritingWorkspace::show_semantic_annotation(const std::string &id) {
  if (!current_document_ || !owner_window())
    return;
  try {
    auto d = capture_editor_state();
    application::SemanticAnchorEngine::inspect(d);
    const auto it = std::find_if(d.semantics.annotations.begin(),
                                 d.semantics.annotations.end(),
                                 [&](auto &a) { return a.id == id; });
    if (it == d.semantics.annotations.end())
      throw std::runtime_error("Anotação não encontrada; atualize as camadas");
    const auto a = *it;
    const auto types = service_.writing().semantic_types();
    const auto *t =
        project::find_annotation_type(types, a.type_key, a.type_version);
    auto *dialog = new OverlayDialog("Detalhe da marcação do texto",
                                     *owner_window(), true);
    dialog->add_button("Voltar às camadas", Gtk::ResponseType::CANCEL);
    dialog->set_secondary_text(
        "As ações se aplicam somente a esta anotação. Sobreposições e UUIDs "
        "permanecem independentes.");
    auto *box = dialog->get_content_area();
    auto *summary = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    summary->add_css_class("content-card");
    label(*summary, (a.scope == project::AnnotationScope::Document && a.type_key=="inde.semantic.perspective"
                         ? "Ponto de vista do documento"
                     : t ? author::annotation_label(*t)
                         : a.type_key + " (tipo indisponível)") +
                        " • " + status_label(a.status));
    label(*summary, anchor_label(anchor_key(d, a)));
    label(*summary, excerpt(d, a));
    if (a.type_key == "inde.semantic.perspective")
      label(*summary,
            std::string(
                a.scope == project::AnnotationScope::Document
                    ? "Declaração autoral para todo o documento. A perspectiva "
                    : "Declaração autoral apenas deste intervalo. A "
                      "perspectiva ") +
                "livre do Documento permanece independente; nenhuma "
                "conversão ou afirmação de realidade foi aplicada.");
    box->append(*summary);
    const auto diagnostics = project::semantic_diagnostics(d, types);
    for (auto &diag : diagnostics)
      if (diag.annotation_id == a.id)
        label(*box, "Pendência: " + diag.message);
    auto *fields = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    fields->add_css_class("tool-section");
    label(*fields, "Campos atribuídos")->add_css_class("heading");
    if (a.fields.empty())
      label(*fields, "Nenhum campo preenchido");
    for (auto &[key, v] : a.fields) {
      std::string name = key;
      if (t)
        for (auto &f : t->fields)
          if (f.key == key)
            name = author::field_label(*t, f);
      std::string text = name + ": ";
      for (std::size_t i = 0; i < v.values.size(); ++i) {
        if (i)
          text += "; ";
        text += v.kind == project::SemanticFieldKind::Entity
                    ? entity_label(v.values[i])
                : v.kind == project::SemanticFieldKind::Enum
                    ? value_label(v.values[i])
                    : v.values[i];
      }
      label(*fields, text);
    }
    box->append(*fields);
    auto *provenance =
        Gtk::make_managed<Gtk::Expander>("Identificadores, origem e histórico");
    auto *details = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    auto *identity = label(
        *details, "UUID: " + a.id + "\nTrecho marcado: " + a.anchor_id +
                      "\nTipo: " + a.type_key + " • versão " +
                      std::to_string(a.type_version) + "\nOrigem: " + a.origin +
                      "\nFonte: " + a.source + "\nNotas: " + a.notes);
    identity->set_selectable(true);
    provenance->set_child(*details);
    box->append(*provenance);
    if(a.type_key=="inde.editor.review-state" || a.type_key=="inde.editor.suggestion") {
      label(*box,"Item editorial somente leitura aqui. Use Revisar para examinar a proposta ou registrar uma decisão explícita. Seu histórico não é apagado nem arquivado pelo editor de campos semânticos.");
      dialog->signal_response().connect([this,dialog](int){dialog->hide();show_semantic_layers(semantic_offset_);});
      dialog->present();return;
    }
    const auto epoch = edit_epoch_, revision = edit_revision_;
    const auto docid = d.id;
    auto guard = [this, epoch, revision, docid] {
      if (!current_document_ || current_document_->id != docid ||
          edit_epoch_ != epoch || edit_revision_ != revision)
        throw std::runtime_error(
            "O Documento mudou; reabra o detalhe da anotação");
    };
    auto *navigation =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    if (a.scope == project::AnnotationScope::Interval)
      button(*navigation, "Ir ao trecho", [this, dialog, guard, a] {
        try {
          guard();
          auto state = capture_editor_state();
          application::SemanticAnchorEngine::inspect(state);
          const auto *e = evidence(state, a);
          if (!e || e->state != project::AnchorResolution::Resolved)
            throw std::runtime_error("Trecho não localizado; escolha uma nova seleção e marque novamente o trecho explicitamente");
          retire(dialog);
          page_stack_.set_visible_child("editor");
          select_anchor(a.anchor_id);
          open_selected_anchor();
        } catch (const std::exception &e) {
          retire(dialog);
          show_error("Não foi possível navegar", e);
        }
      });
    if (t)
      button(*navigation, "Editar campos", [this, dialog, guard, a] {
        try {
          guard();
          retire(dialog);
          edit_semantic_fields(a.id);
        } catch (const std::exception &e) {
          retire(dialog);
          show_error("Contexto mudou", e);
        }
      });
    button(*navigation, "Relações desta anotação", [this, dialog, guard, a] {
      try {
        guard();
        retire(dialog);
        edit_semantic_relations(a.id);
      } catch (const std::exception &e) {
        retire(dialog);
        show_error("Contexto mudou", e);
      }
    });
    box->append(*navigation);
    if (a.scope == project::AnnotationScope::Interval)
      label(*box, "Para reancorar: volte ao texto e selecione o novo trecho; "
                  "depois abra esta anotação e confirme abaixo. "
                  "Não há reassociação automática.");
    if (a.scope == project::AnnotationScope::Interval)
      button(*box, "Reancorar na seleção atual", [this, dialog, guard, a] {
        try {
          guard();
          auto s = capture_edit_state();
          auto b = std::min(s.selection.cursor, s.selection.bound),
               e = std::max(s.selection.cursor, s.selection.bound);
          auto next = application::TextSemanticService::reanchor(s.document,
                                                                 a.id, b, e);
          retire(dialog);
          apply_semantic_document(std::move(next), "Reancorar anotação");
          show_semantic_layers(semantic_offset_);
        } catch (const std::exception &e) {
          retire(dialog);
          show_error("Não foi possível reancorar", e);
        }
      });
    auto *dates=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,6);
    label(*dates,"Datas e horários da biblioteca de Tempo")->add_css_class("heading");
    label(*dates,"Contexto autoral deste trecho ou da perspectiva; não cria acontecimento nem altera o texto.");
    for(const auto &b:a.dates) {
      const auto ref=service_.worldbuilding().temporal_reference(b.reference_id);
      label(*dates,ref.label+" — "+project::format_temporal_reference(service_.worldbuilding().calendar(ref.calendar_id),ref));
      label(*dates,"Papel: "+value_label(b.role)+" · "+b.notes);
      button(*dates,"Remover data do texto",[this,dialog,guard,a,b]{
        try {guard();auto next=application::TextSemanticService::unbind_date(capture_editor_state(),a.id,b.id);
          retire(dialog);apply_semantic_document(std::move(next),"Remover data do trecho (desfazível)");show_semantic_layers(semantic_offset_);
        }catch(const std::exception &e){retire(dialog);show_error("Data preservada",e);}
      });
    }
    button(*dates,"Vincular data / horário…",[this,dialog,guard,a]{
      try {guard();retire(dialog);edit_semantic_dates(a.id);}
      catch(const std::exception &e){retire(dialog);show_error("Contexto mudou",e);}
    });
    box->append(*dates);
    auto *time = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    time->add_css_class("tool-section");
    label(*time, "Contexto no tempo da história — vínculos manuais")
        ->add_css_class("heading");
    label(*time,
          "Não cria acontecimento ou presença e não altera a cronologia. Sem "
          "fim = um ponto; dois limites = período fechado.");
    if (a.fictional_time.empty())
      label(*time, "Nenhum vínculo temporal declarado");
    for (const auto &b : a.fictional_time) {
      label(*time, semantic_time_label(b));
      label(*time, "Origem: " + b.origin + " • Nota: " + b.notes);
      button(*time, "Desvincular temporal " + b.id.substr(0, 8),
             [this, dialog, guard, a, b] {
               try {
                 guard();
                 auto next =
                     application::TextSemanticService::unbind_fictional_time(
                         capture_editor_state(), a.id, b.id);
                 retire(dialog);
                 apply_semantic_document(
                     std::move(next), "Desvincular temporal (desfazível)");
                 show_semantic_layers(semantic_offset_);
               } catch (const std::exception &e) {
                 retire(dialog);
                 show_error("Vínculo preservado", e);
               }
             });
    }
    button(*time, "Vincular tempo da história", [this, dialog, guard, a] {
      try {
        guard();
        retire(dialog);
        edit_semantic_time(a.id);
      } catch (const std::exception &e) {
        retire(dialog);
        show_error("Contexto mudou", e);
      }
    });
    box->append(*time);
    auto *management =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    button(*management,
           a.status == "archived" ? "Desarquivar (revisar campos)"
                                  : "Arquivar anotação",
           [this, dialog, guard, a] {
             try {
               guard();
               auto next = application::TextSemanticService::archive(
                   capture_editor_state(), a.id, a.status != "archived");
               retire(dialog);
               apply_semantic_document(std::move(next),
                                       "Arquivar/desarquivar anotação");
               show_semantic_layers(semantic_offset_);
             } catch (const std::exception &e) {
               retire(dialog);
               show_error("Não foi possível alterar anotação", e);
             }
           });
    button(*management, "Remover anotação", [this, dialog, guard, a] {
      bool retired{};
      try {
        guard();
        persistence::SemanticUseQuery q;
        q.target_annotation_id = a.id;
        q.exclude_document_id = current_document_->id;
        q.include_archived = true;
        q.limit = 1;
        if (service_.writing().semantic_uses(q).total)
          throw std::runtime_error(
              "Há relações recebidas; remova-as explicitamente ou arquive");
        auto next = application::TextSemanticService::remove(
            capture_editor_state(), a.id);
        retire(dialog);
        retired = true;
        apply_semantic_document(std::move(next),
                                "Remover anotação (desfazível)");
        show_semantic_layers(semantic_offset_);
      } catch (const std::exception &e) {
        if (!retired) retire(dialog);
        try {
          flush_changes();
          const auto preview = service_.removal_impact(project::RemovalKind::TextAnnotation, a.id);
          if (preview.uses.empty()) show_error("Anotação preservada", e);
          else confirm_project_removal(*owner_window(), service_, project::RemovalKind::TextAnnotation, a.id, {});
        } catch (const std::exception &error) { show_error("Anotação preservada", error); }
      }
    });
    box->append(*management);
    dialog->signal_response().connect([this, dialog](int) {
      dialog->hide();
      show_semantic_layers(semantic_offset_);
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível abrir a anotação", e);
  }
}

void WritingWorkspace::register_semantic_type() {
  if (!service_.current() || !owner_window())
    return;
  auto *form = new OverlayDialog("Novo tipo de marcação personalizado",
                                 *owner_window(), true);
  form->set_secondary_text("Dê um nome à marcação e escolha seus campos. Identificadores são gerados automaticamente. As marcações do INDE e seus textos não serão alterados.");
  form->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  form->add_button("Criar tipo de marcação", Gtk::ResponseType::ACCEPT);
  auto *body = form->get_content_area();
  auto *name = Gtk::make_managed<Gtk::Entry>();
  auto *description = Gtk::make_managed<Gtk::Entry>();
  append_labeled_form_field(*body, "Nome da marcação", *name, "Ex.: Ritmo ou Tensão.");
  append_labeled_form_field(*body, "Descrição", *description);
  struct Row {
    Gtk::Box *box;
    Gtk::Entry *name, *options;
    Gtk::ComboBoxText *kind;
    Gtk::CheckButton *required, *multiple;
  };
  auto rows = std::make_shared<std::vector<Row>>();
  auto *fields = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
  body->append(*fields);
  auto *add = Gtk::make_managed<Gtk::Button>("Adicionar campo");
  body->append(*add);
  const auto append_row = [rows, fields, add] {
    if (rows->size() >= 32) return;
    Row row{};
    row.box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    row.box->add_css_class("tool-section");
    row.name = Gtk::make_managed<Gtk::Entry>();
    row.kind = Gtk::make_managed<Gtk::ComboBoxText>();
    row.options = Gtk::make_managed<Gtk::Entry>();
    row.required = Gtk::make_managed<Gtk::CheckButton>("Obrigatório");
    row.multiple = Gtk::make_managed<Gtk::CheckButton>("Permitir mais de um valor");
    for (const auto &[key, title] : std::vector<std::pair<std::string, std::string>>{
        {"text", "Texto"}, {"integer", "Número inteiro"}, {"boolean", "Sim ou não"},
        {"entity", "Elemento da história"}, {"enum", "Lista de opções"}})
      row.kind->append(key, title);
    row.kind->set_active_id("text");
    append_labeled_form_field(*row.box, "Nome do campo", *row.name);
    append_labeled_form_field(*row.box, "Conteúdo do campo", *row.kind);
    auto *options_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    append_labeled_form_field(*options_box, "Opções", *row.options, "Separe com ponto e vírgula. Ex.: Baixa; Média; Alta.");
    row.box->append(*options_box);
    options_box->set_visible(false);
    row.kind->signal_changed().connect([kind=row.kind, options_box] {
      options_box->set_visible(kind->get_active_id() == "enum");
    });
    row.box->append(*row.required);
    row.box->append(*row.multiple);
    auto *remove = Gtk::make_managed<Gtk::Button>("Retirar este campo");
    row.box->append(*remove);
    fields->append(*row.box);
    rows->push_back(row);
    remove->signal_clicked().connect([rows, fields, add, target=row.box] {
      rows->erase(std::remove_if(rows->begin(), rows->end(), [target](const Row &v) {return v.box == target;}), rows->end());
      fields->remove(*target);
      add->set_sensitive(true);
    });
    add->set_sensitive(rows->size() < 32);
  };
  add->signal_clicked().connect(append_row);
  append_row();
  rows->front().name->set_text("Nota");
  const auto key = "project." + author::new_category_key();
  const auto definition = [rows, name, description, key] {
    std::vector<author::PersonalField> values;
    for (const auto &r : *rows) {
      author::PersonalField field;
      field.name = r.name->get_text().raw();
      field.kind = project::semantic_field_kind(r.kind->get_active_id().raw());
      field.required = r.required->get_active();
      field.multiple = r.multiple->get_active();
      if (field.kind == project::SemanticFieldKind::Enum) {
        std::string value = r.options->get_text().raw();
        std::size_t from = 0;
        do {
          const auto until = value.find(';', from);
          const auto item = value.substr(from, until == std::string::npos ? until : until-from);
          const auto begin = item.find_first_not_of(" \t\r\n");
          const auto end = item.find_last_not_of(" \t\r\n");
          if (begin == std::string::npos) throw std::runtime_error("Preencha cada opção da lista; não deixe opções vazias.");
          field.choices.push_back(item.substr(begin, end-begin+1));
          if (until == std::string::npos) break;
          from = until + 1;
        } while (true);
      }
      values.push_back(std::move(field));
    }
    return author::personal_definition(name->get_text().raw(), description->get_text().raw(), values, key);
  };
  auto *advanced = Gtk::make_managed<Gtk::Expander>("Editor técnico (opcional)");
  auto *advanced_body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  auto *manual = Gtk::make_managed<Gtk::CheckButton>("Usar a definição do editor técnico ao criar");
  advanced_body->append(*manual);
  auto *copy = Gtk::make_managed<Gtk::Button>("Conferir definição gerada pelo formulário");
  advanced_body->append(*copy);
  auto *text = Gtk::make_managed<Gtk::TextView>();
  text->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  text->set_size_request(320, 180);
  advanced_body->append(*text);
  auto *technical_message = Gtk::make_managed<Gtk::Label>();
  technical_message->set_wrap(true);
  technical_message->set_xalign(0);
  advanced_body->append(*technical_message);
  // Never overwrite a manual draft when toggling modes. Copy is explicit and
  // only offered while the editor is empty; clearing it is an author action.
  copy->signal_clicked().connect([definition, text, technical_message] {
    if (!text->get_buffer()->get_text().empty()) {
      technical_message->set_text("O editor já contém uma definição. Ela foi preservada; limpe-o explicitamente para gerar outra.");
      return;
    }
    try { text->get_buffer()->set_text(definition()); technical_message->set_text("Definição gerada; nada foi registrado ainda."); }
    catch (const std::exception &e) { technical_message->set_text(author::diagnostic(e.what())); }
  });
  advanced->set_child(*advanced_body);
  body->append(*advanced);
  const auto project_id = service_.current()->manifest().project_id;
  form->signal_response().connect([this, form, text, project_id, manual, definition](int r) {
    if (r != Gtk::ResponseType::ACCEPT) {
      form->hide();
      return;
    }
    try {
      if (!service_.current() ||
          service_.current()->manifest().project_id != project_id)
        throw std::runtime_error("Projeto mudou; definição não aplicada");
      service_.writing().register_semantic_type_json(
          manual->get_active() ? text->get_buffer()->get_text().raw() : definition());
      form->hide();
      show_semantic_layers(semantic_offset_);
      signal_status_message_.emit(
          "Tipo de marcação criado; selecione um trecho para usá-lo");
    } catch (const std::exception &e) {
      form->set_secondary_text(std::string("A marcação não foi criada. ") + author::diagnostic(e.what()));
      form->keep_open_on_response();
    }
  });
  form->present();
}
} // namespace inde::ui
