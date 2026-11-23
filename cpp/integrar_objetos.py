"""Writing owns composition; no extra editor window and no frozen text fork."""
def expand(name,text):
    def replace(before,after):
        nonlocal text
        if before not in text: raise RuntimeError('Object integration: base changed: '+name+': '+before[:70])
        text=text.replace(before,after)
    if name=='include/inde/ui/main_window.hpp':
        replace('  void show_publication();','  void show_publication();\n  void show_objects();')
    elif name=='include/inde/ui/writing_workspace.hpp':
        replace('class KeyboardCommands;', 'class KeyboardCommands;\nclass ObjectDocumentEditor;')
        replace('  void flush_changes();', '  void flush_changes();\n  void show_document_objects();\n  bool objects_can_leave() const;')
        replace('  void build_ui();', '  void build_ui();\n  void build_object_specializations();\n  void show_object_specialization(const std::string &);\n  void flush_document_objects();\n  void reset_document_objects();\n  void schedule_common_sheet();\n  void focus_common_sheet();\n  Gtk::Widget *make_paragraph_view(const std::string &);\n  sigc::connection common_sheet_idle_;\n  std::unique_ptr<ObjectDocumentEditor> object_editor_;\n  Gtk::Stack document_surfaces_;\n  Gtk::Box specialty_strip_{Gtk::Orientation::HORIZONTAL, 2};\n  bool committing_object_document_{};')
    elif name=='src/ui/main_window.cpp':
        replace('bool MainWindow::on_close_request() {', 'bool MainWindow::on_close_request() {\n  if(!writing_workspace_.objects_can_leave())return true;')
        replace('void MainWindow::setup_actions() {','void MainWindow::setup_actions() {\n  add_action("document-objects", sigc::mem_fun(*this, &MainWindow::show_objects));')
        replace('MainWindow::~MainWindow() {', 'void MainWindow::show_objects(){ if(workspace_shell_.current()!=WorkspaceId::Writing)show_workspace(WorkspaceId::Writing); writing_workspace_.show_document_objects(); }\nMainWindow::~MainWindow() {')
    elif name=='src/ui/writing_workspace.cpp':
        replace('#include "inde/ui/writing_workspace.hpp"', '#include "inde/ui/writing_workspace.hpp"\n#include "object_native_ui.hpp"')
        replace('  editor_scroll_.set_child(editor_);', '  editor_.set_name("writing-text-editor");\n  editor_scroll_.set_child(editor_);')
        replace('  editor_surface_.set_child(editor_reading_row_);','  document_surfaces_.add(editor_reading_row_, "text");\n  editor_surface_.set_child(document_surfaces_);')
        replace('void WritingWorkspace::reset() {','void WritingWorkspace::reset() {\n  reset_document_objects();')
        replace('WritingWorkspace::~WritingWorkspace() {','WritingWorkspace::~WritingWorkspace() {\n  common_sheet_idle_.disconnect();\n  reset_document_objects();')
        replace('    mark_dirty();\n    refresh_formatting_toolbar();', '    mark_dirty();\n    schedule_common_sheet();\n    refresh_formatting_toolbar();')
        replace('          schedule_position_save();\n          refresh_alternative_context();', '          schedule_position_save();\n          refresh_alternative_context();\n          schedule_common_sheet();')
    elif name=='src/ui/writing_workspace_document.cpp':
        replace('  flush_visibility_edits();\n  composition_page_.flush();','  if (!committing_object_document_ && !objects_can_leave())\n    throw std::runtime_error("Aplique ou cancele a edição aberta antes de continuar.");\n  flush_visibility_edits();\n  composition_page_.flush();')
        replace('  remember_document_position();\n}', '  remember_document_position();\n  if (!committing_object_document_) flush_document_objects();\n}')
        replace('    current_document_ = value;', '    reset_document_objects();\n    current_document_ = value;')
        replace('    current_document_.reset();','    reset_document_objects();\n    current_document_.reset();')
    elif name=='src/ui/writing_workspace_specializations.cpp':
        replace('  set_editor_specialization("text", false);','  build_object_specializations();\n  set_editor_specialization("text", false);')
        replace('id != "structure" && id != "bindings")','id != "structure" && id != "bindings" && id != "composition" &&\n        id != "tables" && id != "formulas" && id != "math")')
        replace('    require_completed_composition();','    if (!objects_can_leave()) throw std::runtime_error("Aplique ou cancele a edição aberta antes de mudar de especialidade.");\n    require_completed_composition();')
        replace('    editor_specialization_ = id;', '    show_object_specialization(id);\n    editor_specialization_ = id;')
        replace('    refresh_reading_blocks();', '    const bool object_mode = id=="composition" || id=="tables" || id=="formulas" || id=="math";\n    undo_button_.set_visible(!object_mode);redo_button_.set_visible(!object_mode);\n    context_menu_button_.set_visible(!object_mode && !focus_mode_);\n    if(object_mode)specialization_hint_.set_text(id=="composition" ? "Organize os mesmos parágrafos e objetos. Clique direito para suas ações." : id=="tables" ? "Tabelas: tamanho, aparência, linhas e colunas nos instrumentos." : id=="formulas" ? "Cartões, condições e variáveis do Documento nos instrumentos." : "Matemática: equações editáveis com prévia nativa.");\n    refresh_reading_blocks();')
        replace('    if (focus && current_document_)','    if (focus && current_document_ && id != "composition" && id != "tables" && id != "formulas" && id != "math")')
        replace('    specialization_selector_.set_active_id(id);','    specialization_selector_.set_active_id(id);\n    for (auto *c=specialty_strip_.get_first_child();c;c=c->get_next_sibling())\n      if (auto *b=dynamic_cast<Gtk::ToggleButton*>(c)) b->set_active(b->get_name().raw()=="specialty-"+id);')
    elif name=='src/ui/writing_workspace_visible_edit.cpp':
        replace('    refreshing_visibility_ = false;\n    refresh_reading_blocks();', '    if(object_editor_ && !changing_specialization_ && !committing_object_document_)schedule_common_sheet();\n    refreshing_visibility_ = false;\n    refresh_reading_blocks();')
    elif name=='src/ui/writing_workspace_keyboard.cpp':
        replace('std::vector<Gtk::Widget *> WritingWorkspace::keyboard_regions() {', 'std::vector<Gtk::Widget *> WritingWorkspace::keyboard_regions() {\n  if(document_surfaces_.get_visible_child_name()=="objects")return {&editor_toolbar_,&specialty_strip_,&document_surfaces_};')
    elif name=='src/ui/writing_workspace_export.cpp':
        replace('#include "inde/ui/writing_workspace.hpp"', '#include "inde/ui/writing_workspace.hpp"\n#include "object_composite_pilot.hpp"')
        replace('    if(!document)throw std::runtime_error("Documento não encontrado para exportar");', '    if(!document)throw std::runtime_error("Documento não encontrado para exportar");\n    const auto composition=service_.current()->path()/"documents/composed"/document->id;\n    if(std::filesystem::exists(composition/"obj01-piloto.sqlite3") && !objects::pilot::Session::plain_text_compatible(composition,service_.current()->manifest().project_id,*document))\n      throw std::runtime_error("A exportação atual cobre apenas texto. Esta composição contém objetos ou arranjos que ainda precisam de suporte; nenhum arquivo foi gerado.");')
    elif name=='src/ui/writing_workspace_tools.cpp':
        replace(' no Documento integral', ' nos parágrafos do Documento')
    elif name=='src/ui/writing_workspace_ime.cpp':
        replace('      ime_commit_pending_=false;', '      ime_commit_pending_=false;\n      schedule_common_sheet();')
    elif name=='src/main.cpp':
        replace('    // Routing belongs', '    if(argc==4 && std::string(argv[1])=="--objetos-smoke")return run_object_window_smoke(application,argv[2],argv[3]);\n    // Routing belongs')
    if name=='src/ui/writing_workspace_specializations.cpp':
        text=text.replace('    if (focus && current_document_ && id != "composition" && id != "tables" && id != "formulas" && id != "math")\n      editor_.grab_focus();', '    if (focus && current_document_ && object_editor_) object_editor_->focus_text(cursor, false);')
        text=text.replace('#include "inde/ui/writing_workspace.hpp"', '#include "inde/ui/writing_workspace.hpp"\n#include "object_native_ui.hpp"')
    if name=='src/ui/writing_workspace_specializations.cpp':
        text=text.replace('    specialization_selector_.set_active_id(editor_specialization_);', '    specialization_selector_.set_active_id(editor_specialization_);\n    for(auto *c=specialty_strip_.get_first_child();c;c=c->get_next_sibling()) if(auto *b=dynamic_cast<Gtk::ToggleButton*>(c)) b->set_active(b->get_name().raw()=="specialty-"+editor_specialization_);')
    if name.startswith("src/ui/writing_workspace"):
        text=text.replace("editor_.grab_focus()", "focus_common_sheet()")
    if name in ("src/ui/writing_workspace.cpp", "src/ui/writing_workspace_document.cpp") or name.startswith("src/ui/writing_workspace"):

        text=text.replace('"writing_workspace_util.hpp"','"ui/writing_workspace_util.hpp"').replace('"editor_diagnostics.hpp"','"ui/editor_diagnostics.hpp"')
    return text
