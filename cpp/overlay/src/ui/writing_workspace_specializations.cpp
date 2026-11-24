#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "object_native_ui.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::ui {
void WritingWorkspace::build_editor_specializations() {
  specialization_selector_.set_name("editor-specialization");
  for (const auto &[id, label] :
       std::vector<std::pair<std::string, std::string>>{
           {"text", "Texto e formatação"},
           {"semantic", "Semântica"},
           {"linguistic", "Linguística"},
           {"structure", "Estrutura"},
           {"bindings", "Vínculos narrativos"}})
    specialization_selector_.append(id, label);
  specialization_selector_.set_active_id("text");
  set_accessible_label(specialization_selector_,
                       "Especialização do mesmo editor");
  specialization_selector_.set_tooltip_text(
      "Muda instrumentos e realces; mantém documento, seleção, visibilidade e "
      "histórico.");
  editor_toolbar_.remove(format_menu_button_);
  editor_toolbar_.remove(context_menu_button_);
  editor_toolbar_.remove(document_menu_button_);
  editor_toolbar_.append(specialization_selector_);
  editor_toolbar_.append(format_menu_button_);
  editor_toolbar_.append(context_menu_button_);
  editor_toolbar_.append(document_menu_button_);
  for (auto *button : {&undo_button_, &redo_button_}) {
    if (auto *parent = dynamic_cast<Gtk::Box *>(button->get_parent()))
      parent->remove(*button);
    editor_toolbar_.append(*button);
  }
  specialization_hint_.set_xalign(0);
  specialization_hint_.set_hexpand(true);
  specialization_hint_.set_ellipsize(Pango::EllipsizeMode::END);
  specialization_hint_.add_css_class("dim-label");
  specialization_bar_.append(specialization_hint_);
  specialization_bar_.append(visibility_button_);
  specialization_bar_.append(show_all_button_);
  specialization_bar_.append(pending_edit_button_);
  pending_edit_button_.set_visible(false);
  editor_page_.insert_child_after(specialization_bar_, editor_toolbar_);
  visibility_button_.set_name("editor-visibility");
  visibility_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::show_visibility_profiles));
  show_all_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::show_all_editor_text));
  pending_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::review_pending_visibility_edit));
  show_all_button_.set_visible(false);
  auto extra = [&](const char *label, const char *specialization,
                   std::function<void()> action) {
    auto *b = Gtk::make_managed<Gtk::Button>(label);
    b->signal_clicked().connect([this, action] {
      context_popover_.popdown();
      action();
    });
    if (std::string(specialization) == "structure" &&
        (std::string(label) == "Criar alternativa do trecho" ||
         std::string(label) == "Folhas e alternativas…"))
      specialization_bar_.append(*b);
    else
      contextual_toolbar_.append(*b);
    specialized_controls_.emplace_back(b, specialization);
  };
  specialized_controls_ = {{&anchors_button_, "bindings"},
                           {&entities_button_, "bindings"},
                           {&semantic_button_, "semantic"},
                           {&semantic_lens_button_, "semantic"},
                           {&semantic_mark_button_, "semantic"},
                           {&comment_button_, "semantic"},
                           {&statistics_button_, "text"},
                           {&goal_button_, "text"},
                           {&repeated_button_, "linguistic"},
                           {&long_paragraphs_button_, "linguistic"},
                           {&inline_button_, "all"}};
  for (auto *child = contextual_toolbar_.get_first_child(); child;
       child = child->get_next_sibling())
    if (auto *b = dynamic_cast<Gtk::Button *>(child);
        b && b->get_label() == "Planejamento deste texto…")
      specialized_controls_.emplace_back(b, "bindings");
  extra("Consultar palavra selecionada", "linguistic",
        [this] { show_semantic_dictionary(); });
  extra("Construções e cenários de significado", "linguistic",
        [this] { show_linguistic_entities(); });
  extra("Parágrafos e identidades…", "structure",
        [this] { show_document_paragraphs(); });
  extra("Folhas e alternativas…", "structure",
        [this] { show_document_alternatives(); });
  extra("Criar alternativa do trecho", "structure",
        [this] { create_text_alternative(); });
  extra("Mover bloco acima na leitura", "structure",
        [this] { move_reading_current(-1); });
  extra("Mover bloco abaixo na leitura", "structure",
        [this] { move_reading_current(1); });
  extra("Experimentar leitura sem os blocos escolhidos", "structure",
        [this] { hide_selected_reading_blocks(); });
  extra("Visibilidade e lentes…", "structure",
        [this] { show_visibility_profiles(); });
  extra("Vincular seleção a elemento existente…", "bindings",
        [this] { show_text_entity(false); });
  extra("Referências a outros textos…", "bindings",
        [this] { show_text_references(); });
  specialization_selector_.signal_changed().connect([this] {
    if (!changing_specialization_)
      set_editor_specialization(specialization_selector_.get_active_id());
  });
  build_object_specializations();
  set_editor_specialization("text", false);
}
void WritingWorkspace::set_editor_specialization(const std::string &id,
                                                 bool focus) {
  if (changing_specialization_)
    return;
  changing_specialization_ = true;
  try {
    if (id != "text" && id != "semantic" && id != "linguistic" &&
        id != "structure" && id != "bindings" && id != "composition" &&
        id != "tables" && id != "formulas" && id != "math")
      throw std::runtime_error("Especialização desconhecida");
    if (!objects_can_leave()) {
      specialization_selector_.set_active_id(editor_specialization_);
    for(auto *c=specialty_strip_.get_first_child();c;c=c->get_next_sibling()) if(auto *b=dynamic_cast<Gtk::ToggleButton*>(c)) b->set_active(b->get_name().raw()=="specialty-"+editor_specialization_);
      for(auto *c=specialty_strip_.get_first_child();c;c=c->get_next_sibling()) if(auto *b=dynamic_cast<Gtk::ToggleButton*>(c)) b->set_active(b->get_name().raw()=="specialty-"+editor_specialization_);
      request_objects_leave("trocar de especialidade", [this,id,focus]{set_editor_specialization(id,focus);});
      changing_specialization_=false;return;
    }
    require_completed_composition();
    flush_visibility_edits();
    if (current_document_)
      commit_undo_group();
    const auto buffer = editor_.get_buffer();
    const auto cursor =
                   buffer->get_iter_at_mark(buffer->get_insert()).get_offset(),
               bound = buffer->get_iter_at_mark(buffer->get_selection_bound())
                           .get_offset();
    const auto scroll = editor_scroll_.get_vadjustment()->get_value();
    format_popover_.popdown();
    context_popover_.popdown();
    document_popover_.popdown();
    if (tools_visible_)
      toggle_tools();
    show_object_specialization(id);
    editor_specialization_ = id;
    specialization_selector_.set_active_id(id);
    for (auto *c=specialty_strip_.get_first_child();c;c=c->get_next_sibling())
      if (auto *b=dynamic_cast<Gtk::ToggleButton*>(c)) b->set_active(b->get_name().raw()=="specialty-"+id);
    format_menu_button_.set_visible(id == "text" && !focus_mode_);
    context_menu_button_.set_label(id == "text"         ? "Texto…"
                                   : id == "semantic"   ? "Marcações…"
                                   : id == "linguistic" ? "Linguística…"
                                   : id == "structure"  ? "Blocos…"
                                                        : "Vínculos…");
    for (auto &[w, group] : specialized_controls_)
      w->set_visible(group == id || group == "all");
    semantic_lens_button_.set_active(id == "semantic");
    specialization_hint_.set_text(
        id == "text"
            ? "Escreva e formate. O conteúdo e o histórico são compartilhados."
        : id == "semantic"   ? "Marcações sobre o texto: cores por tipo; "
                               "detalhes pelo ponteiro ou teclado."
        : id == "linguistic" ? "Consulta lexical e padrões disponíveis; "
                               "nenhuma acepção escolhida automaticamente."
        : id == "structure" ? "Blocos, folhas e ordem. Arraste em relação à "
                              "borda do vizinho visível."
                            : "Trechos, referências e vínculos com a obra; "
                              "nenhuma entidade inferida automaticamente.");
    const bool object_mode = id=="composition" || id=="tables" || id=="formulas" || id=="math";
    undo_button_.set_visible(!object_mode);redo_button_.set_visible(!object_mode);
    context_menu_button_.set_visible(!object_editor_ && !object_mode && !focus_mode_);
    refresh_specialty_tools();
    if(object_mode)specialization_hint_.set_text(id=="composition" ? "Organize os mesmos parágrafos e objetos. Clique direito para suas ações." : id=="tables" ? "Tabelas: tamanho, aparência, linhas e colunas nos instrumentos." : id=="formulas" ? "Cartões, condições e variáveis do Documento nos instrumentos." : "Matemática: equações editáveis com prévia nativa.");
    refresh_reading_blocks();
    buffer->select_range(buffer->get_iter_at_offset(cursor),
                         buffer->get_iter_at_offset(bound));
    editor_scroll_.get_vadjustment()->set_value(scroll);
    changing_specialization_ = false;
    if (focus && current_document_ && object_editor_) object_editor_->focus_text(cursor, false);
  } catch (const std::exception &e) {
    specialization_selector_.set_active_id(editor_specialization_);
    for(auto *c=specialty_strip_.get_first_child();c;c=c->get_next_sibling()) if(auto *b=dynamic_cast<Gtk::ToggleButton*>(c)) b->set_active(b->get_name().raw()=="specialty-"+editor_specialization_);
    changing_specialization_ = false;
    signal_status_message_.emit(e.what());
  }
}
} // namespace inde::ui
