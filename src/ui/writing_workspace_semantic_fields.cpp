#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "writing_semantic_ui_support.hpp"
#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
namespace inde::ui {
using namespace semantic_ui;
void WritingWorkspace::edit_semantic_fields(
    std::optional<std::string> annotation_id,
    std::optional<std::string> type_key, project::AnnotationScope scope) {
  if (!current_document_ || !owner_window())
    return;
  try {
    require_completed_composition();
    auto state = capture_edit_state();
    auto types = service_.writing().semantic_types();
    const project::TextAnnotation *annotation = nullptr;
    if (annotation_id) {
      auto a = std::find_if(state.document.semantics.annotations.begin(),
                            state.document.semantics.annotations.end(),
                            [&](auto &x) { return x.id == *annotation_id; });
      if (a == state.document.semantics.annotations.end())
        throw std::runtime_error("Anotação não encontrada");
      annotation = &*a;
      type_key = a->type_key;
      scope = a->scope;
    }
    auto start = std::min(state.selection.cursor, state.selection.bound),
         end = std::max(state.selection.cursor, state.selection.bound);
    if (!annotation && scope == project::AnnotationScope::Interval &&
        start == end)
      throw std::runtime_error("Selecione um trecho para marcar");
    if (!type_key) {
      auto *dialog =
          new OverlayDialog("Escolher tipo de marcação", *owner_window(), true);
      dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
      dialog->add_button("Continuar", Gtk::ResponseType::ACCEPT);
      auto *select = Gtk::make_managed<Gtk::ComboBoxText>();
      select->append("", "Escolha explicitamente um tipo");
      for (auto &t : types) {
        if(t.key=="inde.editor.review-state" || t.key=="inde.editor.suggestion")continue;
        select->append(t.version == 1 ? t.key
                                      : t.key + "@" + std::to_string(t.version),
                       author::annotation_label(t) + " — v" + std::to_string(t.version));
      }
      select->set_active(0);
      dialog->get_content_area()->append(*select);
      const auto chooser_epoch = edit_epoch_, chooser_revision = edit_revision_;
      const auto chooser_document = state.document.id;
      dialog->signal_response().connect([this, dialog, select, chooser_epoch,
                                         chooser_revision,
                                         chooser_document](int r) {
        auto key = select->get_active_id().raw();
        dialog->hide();
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        try {
          if (!current_document_ || current_document_->id != chooser_document ||
              edit_epoch_ != chooser_epoch ||
              edit_revision_ != chooser_revision)
            throw std::runtime_error(
                "O contexto mudou; reabra a escolha de tipo de marcação");
          if (key.empty())
            throw std::runtime_error(
                "Escolha explicitamente um tipo de marcação");
          edit_semantic_fields({}, key);
        } catch (const std::exception &e) {
          show_error("Escolha não aplicada", e);
        }
      });
      dialog->present();
      return;
    }
    std::uint32_t selected_version = annotation ? annotation->type_version : 1;
    if (!annotation) {
      auto pos = type_key->find('@');
      if (pos != std::string::npos) {
        selected_version = std::stoul(type_key->substr(pos + 1));
        *type_key = type_key->substr(0, pos);
      }
    }
    if(*type_key=="inde.editor.review-state" || *type_key=="inde.editor.suggestion")
      throw std::runtime_error("Decisões editoriais são registradas em Revisar. Para propor uma alteração, selecione o trecho e use Sugerir alteração.");
    auto *t = project::find_annotation_type(types, *type_key, selected_version);
    if (!t)
      throw std::runtime_error("Tipo/versão indisponível; dados preservados");
    auto type = *t;
    auto *dialog =
        new OverlayDialog(scope == project::AnnotationScope::Document
                              ? "Ponto de vista do documento"
                          : annotation ? "Editar anotação: " + author::annotation_label(type)
                                       : "Marcar: " + author::annotation_label(type),
                          *owner_window(), true);
    dialog->set_secondary_text(
        "Campos obrigatórios vazios geram uma anotação INCOMPLETA, nunca uma escolha automática. Selecione elementos da história existentes; valores múltiplos são listas tipadas.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Salvar anotação", Gtk::ResponseType::ACCEPT);
    auto *box = dialog->get_content_area();
    if (scope == project::AnnotationScope::Document)
      label(*box,
            "Declaração global, sem trecho marcado ou cobertura artificial do texto. A perspectiva livre em Detalhes não é convertida. Intervalos podem declarar perspectiva própria; sobreposição continua explícita.");
    auto widgets = std::make_shared<std::vector<FieldWidget>>();
    for (auto &f : type.fields) {
      const auto field_name = author::field_label(type, f);
      FieldWidget w;
      w.definition = f;
      label(*box, field_name + (f.required ? " *" : "") +
                      (f.multiple ? " (múltiplo)" : ""));
      std::vector<std::string> old;
      if (annotation) {
        auto v = annotation->fields.find(f.key);
        if (v != annotation->fields.end())
          old = v->second.values;
      }
      if (f.kind == project::SemanticFieldKind::Entity) {
        w.entities = std::make_shared<std::vector<std::string>>(old);
        auto chosen = w.entities;
        auto *selected = Gtk::make_managed<Gtk::Label>();
        selected->set_wrap(true);
        selected->set_xalign(0);
        auto refresh = [this, selected, chosen] {
          std::string s = "Escolhas explícitas: ";
          for (auto &id : *chosen)
            s += entity_label(id) + "; ";
          selected->set_text(s);
        };
        auto *picker = Gtk::make_managed<IncrementalSelector>(
            "Pesquisar elemento da história para " + field_name,
            [this, allowed = f.entity_type_keys](const std::string &s,
                                                 std::size_t limit) {
              persistence::EntityQuery q;
              q.search = s;
              q.limit = limit;
              for (auto &t : service_.narrative().entity_types())
                if (!allowed.empty() &&
                    std::find(allowed.begin(), allowed.end(), t.key) !=
                        allowed.end())
                  q.entity_type_ids.push_back(t.id);
              if (!allowed.empty() && q.entity_type_ids.empty())
                return std::vector<IncrementalSelection>{};
              std::vector<IncrementalSelection> result;
              for (auto &e : service_.narrative().entities(q))
                result.push_back({e.id, e.name, e.summary});
              return result;
            });
        box->append(*picker);
        box->append(*selected);
        refresh();
        button(*box, "Adicionar escolha — " + field_name,
               [picker, chosen, refresh, multiple = f.multiple] {
                 if (auto id = picker->selected_id()) {
                   if (!multiple)
                     chosen->clear();
                   if (std::find(chosen->begin(), chosen->end(), *id) ==
                       chosen->end())
                     chosen->push_back(*id);
                   refresh();
                 }
               });
        button(*box, "Limpar escolhas — " + field_name, [chosen, refresh] {
          chosen->clear();
          refresh();
        });
      } else if ((f.kind == project::SemanticFieldKind::Enum ||
                  f.kind == project::SemanticFieldKind::Boolean) &&
                 f.multiple) {
        auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
        box->append(*row);
        auto opts = f.kind == project::SemanticFieldKind::Boolean
                        ? std::vector<std::string>{"true", "false"}
                        : f.values;
        for (auto &v : opts) {
          auto *c = Gtk::make_managed<Gtk::CheckButton>(
              type.system ? value_label(v) : v);
          c->set_active(std::find(old.begin(), old.end(), v) != old.end());
          row->append(*c);
          w.options.push_back({v, c});
        }
      } else if (f.kind == project::SemanticFieldKind::Enum ||
                 f.kind == project::SemanticFieldKind::Boolean) {
        w.single = Gtk::make_managed<Gtk::ComboBoxText>();
        w.single->append("", "Não informado");
        auto opts = f.kind == project::SemanticFieldKind::Boolean
                        ? std::vector<std::string>{"true", "false"}
                        : f.values;
        for (auto &v : opts)
          w.single->append(v, type.system ? value_label(v) : v);
        w.single->set_active(0);
        if (!old.empty())
          w.single->set_active_id(old.front());
        box->append(*w.single);
      } else {
        w.texts = std::make_shared<std::vector<Gtk::TextView *>>();
        auto entries = w.texts;
        auto add = [box, entries](const std::string &value) {
          auto *editor = Gtk::make_managed<Gtk::TextView>();
          editor->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
          editor->set_size_request(-1, 60);
          editor->get_buffer()->set_text(value);
          box->append(*editor);
          entries->push_back(editor);
        };
        if (old.empty())
          add("");
        else
          for (auto &s : old)
            add(s);
        if (f.multiple)
          button(*box, "Adicionar valor — " + field_name, [add, entries] {
            if (entries->size() < 128)
              add("");
          });
      }
      widgets->push_back(std::move(w));
    }
    const auto epoch = edit_epoch_, revision = edit_revision_;
    auto docid = state.document.id;
    dialog->signal_response().connect([this, dialog, widgets, type,
                                       annotation_id, epoch, revision, docid,
                                       start, end, scope](int r) {
      if (r != Gtk::ResponseType::ACCEPT) {
        dialog->hide();
        return;
      }
      try {
        if (!current_document_ || current_document_->id != docid ||
            edit_epoch_ != epoch || edit_revision_ != revision)
          throw std::runtime_error(
              "O contexto de edição mudou; formulário não aplicado");
        std::map<std::string, project::SemanticFieldValue> fields;
        for (auto &w : *widgets) {
          std::vector<std::string> values;
          if (w.entities)
            values = *w.entities;
          else if (w.texts) {
            for (auto *editor : *w.texts) {
              auto s = editor->get_buffer()->get_text().raw();
              if (!s.empty())
                values.push_back(s);
            }
          } else if (w.text) {
            auto s = w.text->get_text().raw();
            if (!s.empty())
              values.push_back(s);
          } else if (w.single) {
            auto s = w.single->get_active_id().raw();
            if (!s.empty())
              values.push_back(s);
          } else
            for (auto &[v, c] : w.options)
              if (c->get_active())
                values.push_back(v);
          fields.emplace(w.definition.key,
                         project::SemanticFieldValue{w.definition.kind,
                                                     std::move(values)});
        }
        auto d = capture_editor_state();
        auto next =
            annotation_id ? application::TextSemanticService::update(
                                d, *annotation_id, fields,
                                service_.writing().semantic_types())
            : scope == project::AnnotationScope::Document
                ? application::TextSemanticService::create_document_perspective(
                      d, fields)
                : application::TextSemanticService::create(d, type, start, end,
                                                           fields);
        dialog->hide();
        apply_semantic_document(std::move(next),
                                "Criar/editar marcação do texto");
        if (page_stack_.get_visible_child_name() == "semantic-layers")
          show_semantic_layers(semantic_offset_);
        if(!annotation_id && type.key=="inde.semantic.temporal-context")
          edit_semantic_dates(current_document_->semantics.annotations.back().id);
      } catch (const std::exception &e) {
        dialog->hide();
        show_error("Não foi possível salvar a anotação", e);
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível abrir campos semânticos", e);
  }
}

void WritingWorkspace::show_document_perspective() {
  if (!current_document_)
    return;
  auto d = capture_editor_state();
  auto a =
      std::find_if(d.semantics.annotations.begin(),
                   d.semantics.annotations.end(), [](auto &v) {
                     return v.scope == project::AnnotationScope::Document &&
                            v.type_key=="inde.semantic.perspective" &&
                            v.status != "archived";
                   });
  if (a != d.semantics.annotations.end())
    edit_semantic_fields(a->id);
  else
    edit_semantic_fields({}, "inde.semantic.perspective",
                         project::AnnotationScope::Document);
}

} // namespace inde::ui
