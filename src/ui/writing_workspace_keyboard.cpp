#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/keyboard_commands.hpp"
#include "inde/ui/overlay_dialog.hpp"

namespace inde::ui {
std::vector<Gtk::Widget *> WritingWorkspace::keyboard_regions() {
  if(page_stack_.get_visible_child_name()=="editor")return {&editor_toolbar_,&editor_,&tools_scroll_};
  if(page_stack_.get_visible_child_name()=="library")return {&library_header_,&library_scroll_};
  return {&composition_page_};
}
void WritingWorkspace::leave_view() {
  if (!get_mapped()) return;
  try { flush_changes(); close_inline_suggestions(); show_library(); }
  catch (const std::exception &e) { show_error("Não foi possível sair da vista", e); }
}
void WritingWorkspace::install_keyboard(KeyboardCommands &keys) {
  composition_page_.install_keyboard(keys);
  std::function<bool(Gtk::Widget *)> writing = [this](Gtk::Widget *focus) { return get_mapped() && focus_inside(focus, *this); };
  std::function<bool(Gtk::Widget *)> editor = [this](Gtk::Widget *focus) {
    return get_mapped() && (focus_inside(focus,editor_) || (!text_input_focus(focus) && focus_inside(focus,editor_toolbar_)));
  };
  auto document = [this] { return current_document_ && page_stack_.get_visible_child_name() == "editor" ? std::string{} : "Abra um documento para usar este comando."; };
  auto selection = [this, document] {
    if (auto reason = document(); !reason.empty()) return reason;
    Gtk::TextIter a, b;
    return editor_.get_buffer()->get_selection_bounds(a,b) ? std::string{} : "Selecione um trecho primeiro.";
  };
  auto add = [&](std::string id, std::string label, std::string key, std::function<void()> activate,
      std::function<std::string()> unavailable = {}, bool whole = false) {
    keys.add({"writing." + id, std::move(label), "Escrever", std::move(key), {}, whole ? writing : editor,
        std::move(unavailable), std::move(activate)});
  };
  // Navigation commands are window-scoped; text actions are editor-scoped.
  auto project = [this] { return service_.current() ? std::string{} : "Abra ou crie um projeto primeiro."; };
  keys.add({"writing.new-document", "Novo documento livre", "Escrever", "<Control><Shift>n", {}, {}, project, [this] {
    if(auto *window=dynamic_cast<Gtk::Window *>(get_root()))window->activate_action("win.show-writing");
    create_document_at({});
  }});
  // MainWindow opens the writing workspace before invoking these actions.
  keys.add({"writing.library","Biblioteca de documentos","Escrever","<Control><Shift>o",{}, {},project,[this] {
    if(auto *window=dynamic_cast<Gtk::Window *>(get_root()))window->activate_action("win.show-writing");
    leave_view();
  }});
  keys.add({"writing.compositions", "Montagens de texto…", "Escrever", "<Control><Shift>8", {}, {}, project, [this] {
    if(auto *window=dynamic_cast<Gtk::Window *>(get_root()))window->activate_action("win.show-writing");
    try { flush_changes(); close_inline_suggestions(); composition_page_.refresh(); page_stack_.set_visible_child("compositions"); }
    catch (const std::exception &e) { show_error("Não foi possível abrir Montagens", e); }
  }});
  add("tools", "Ferramentas do documento", "F4", [this] { toggle_tools(); }, document, true);
  add("undo", "Desfazer edição de texto", "<Control>z", [this] { undo(); }, document);
  add("redo", "Refazer edição de texto", "<Control><Shift>z", [this] { redo(); }, document);
  add("copy", "Copiar texto", "<Control>c", [this] { copy_selection(false); }, selection);
  add("cut", "Recortar texto", "<Control>x", [this] { copy_selection(false,true); }, selection);
  add("paste", "Colar texto sem vínculos", "<Control>v", [this] { paste_clipboard(false); }, document);
  add("copy-styles", "Copiar com estilos, sem vínculos", "<Control><Shift>c", [this] { copy_selection(true); }, selection);
  add("paste-styles", "Colar com estilos, sem vínculos", "<Control><Shift>v", [this] { paste_clipboard(true); }, document);
  for (const auto &[id,label,key,style] : std::vector<std::tuple<std::string,std::string,std::string,project::DocumentTextStyle>>{
      {"bold","Negrito","<Control>b",project::DocumentTextStyle::Bold},
      {"italic","Itálico","<Control>i",project::DocumentTextStyle::Italic},
      {"underline","Sublinhado","<Control>u",project::DocumentTextStyle::Underline},
      {"strike","Tachado","<Control><Shift>x",project::DocumentTextStyle::Strikethrough},
      {"align-left","Alinhar à esquerda","<Control>l",project::DocumentTextStyle::AlignLeft},
      {"align-center","Centralizar","<Control>e",project::DocumentTextStyle::AlignCenter},
      {"align-right","Alinhar à direita","<Control>r",project::DocumentTextStyle::AlignRight},
      {"align-justify","Justificar","<Control>j",project::DocumentTextStyle::AlignJustify}})
    add(id,label,key,[this,style] { apply_editor_format(style); },document);
  add("clear-format", "Limpar formatação sem apagar marcações", "<Control><Shift>r", [this] { clear_text_formatting(); }, document);
  add("find", "Localizar no texto", "<Control>f", [this] { open_find(false); },document);
  add("replace", "Localizar e substituir…", "<Control>h", [this] { open_find(true); },document);
  add("dictionary", "Consultar seleção no dicionário…", "<Control><Shift>d", [this] { show_semantic_dictionary(); }, [this,selection] {
    if (auto reason = selection(); !reason.empty()) return reason;
    Gtk::TextIter a,b; editor_.get_buffer()->get_selection_bounds(a,b);
    auto term=editor_.get_buffer()->get_text(a,b).raw();
    return term.size() <= 256 && term.find_first_of("\r\n") == std::string::npos ? std::string{} : "Selecione uma palavra ou termo de até 256 bytes, em uma linha.";
  });
  add("comment", "Comentar trecho ou consultar comentários", "<Control><Shift>m", [this] {
    Gtk::TextIter a,b;
    if (editor_.get_buffer()->get_selection_bounds(a,b)) edit_semantic_fields({},"inde.editor.comment");
    else { semantic_project_scope_=false; semantic_filter_={}; semantic_filter_.role="inde.editor.comment"; show_semantic_layers(); }
  },document);
  add("mark", "Marcar tipo de trecho…", "<Control>m", [this] { edit_semantic_fields(); },selection);
  add("time", "Marcar tempo no trecho…", "<Control><Shift>t", [this] { edit_semantic_fields({},"inde.semantic.temporal-context"); },selection);
  add("link-element", "Vincular elemento da história existente…", "<Control><Shift>l", [this] { show_text_entity(false); },selection);
  add("associate-part", "Associar seleção a parte da narrativa…", "<Control><Shift>b", [this] { show_narrative_association(); },selection);
  add("suggest", "Sugerir alteração…", "<Control><Shift>g", [this] { suggest_editorial_change(); },selection);
  add("inline", "Comandos inline…", "<Control><Shift>k", [this] { show_inline_commands(); },document);
  add("output", "Preparar saída do documento…", "<Control><Shift>e", [this] { export_document(); },document);
  add("layers", "Camadas e marcações…", "", [this] { show_semantic_layers(); },document);
  add("perspective", "Perspectiva do documento…", "", [this] { show_document_perspective(); },document);
  add("temporal-notices", "Avisos temporais — não são erros", "", [this] { show_temporal_notices(); },document);
  add("create-element", "Criar elemento da história para seleção…", "", [this] { show_text_entity(true); },selection);
  add("create-relation", "Criar relação para seleção…", "", [this] { show_text_relation(); },selection);
  add("references", "Referências a outros textos…", "", [this] { show_text_references(); },document);
  add("uses", "Usos narrativos do texto…", "", [this] { show_narrative_uses(); },document);
  add("recovery", "Consultar recuperação de rascunhos…", "", [this] { show_recovery_records(); },document);
  add("inline-manager", "Gerenciar modelos de comandos inline…", "", [this] { show_inline_commands(true); },document);
  for (const auto &[id,label] : std::vector<std::pair<std::string,std::string>>{
      {"anchors","Trechos marcados"},{"entities","Elementos da história usados"},{"statistics","Estatísticas"},
      {"goal","Meta de palavras"},{"repeated","Palavras repetidas"},{"paragraphs","Parágrafos longos"}})
    add("tools-"+id,label,"",[this,id] { show_tool_section(id); },document);
}
}
