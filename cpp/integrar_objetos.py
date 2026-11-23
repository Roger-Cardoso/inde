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
        replace('  void build_ui();', '  void build_ui();\n  void build_object_specializations();\n  void show_object_specialization(const std::string &);\n  void flush_document_objects();\n  void reset_document_objects();\n  std::unique_ptr<ObjectDocumentEditor> object_editor_;\n  Gtk::Stack document_surfaces_;\n  Gtk::Box specialty_strip_{Gtk::Orientation::HORIZONTAL, 2};\n  bool committing_object_document_{};')
    elif name=='src/ui/main_window.cpp':
        replace('bool MainWindow::on_close_request() {', 'bool MainWindow::on_close_request() {\n  if(!writing_workspace_.objects_can_leave())return true;')
        replace('void MainWindow::setup_actions() {','void MainWindow::setup_actions() {\n  add_action("document-objects", sigc::mem_fun(*this, &MainWindow::show_objects));')
        replace('MainWindow::~MainWindow() {', 'void MainWindow::show_objects(){ if(workspace_shell_.current()!=WorkspaceId::Writing)show_workspace(WorkspaceId::Writing); writing_workspace_.show_document_objects(); }\nMainWindow::~MainWindow() {')
    elif name=='src/ui/writing_workspace.cpp':
        replace('#include "inde/ui/writing_workspace.hpp"', '#include "inde/ui/writing_workspace.hpp"\n#include "object_native_ui.hpp"')
        replace('  editor_scroll_.set_child(editor_);', '  editor_.set_name("writing-text-editor");\n  editor_scroll_.set_child(editor_);')
        replace('  editor_surface_.set_child(editor_reading_row_);','  document_surfaces_.add(editor_reading_row_, "text");\n  editor_surface_.set_child(document_surfaces_);')
        replace('void WritingWorkspace::reset() {','void WritingWorkspace::reset() {\n  reset_document_objects();')
        replace('WritingWorkspace::~WritingWorkspace() {','WritingWorkspace::~WritingWorkspace() {\n  reset_document_objects();')
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
    elif name=='src/ui/writing_workspace_keyboard.cpp':
        replace('std::vector<Gtk::Widget *> WritingWorkspace::keyboard_regions() {', 'std::vector<Gtk::Widget *> WritingWorkspace::keyboard_regions() {\n  if(document_surfaces_.get_visible_child_name()=="objects")return {&editor_toolbar_,&specialty_strip_,&document_surfaces_};')
    elif name=='src/main.cpp':
        replace('    // Routing belongs', '    if(argc==4 && std::string(argv[1])=="--objetos-smoke")return run_object_window_smoke(application,argv[2],argv[3]);\n    // Routing belongs')
    if name in ("src/ui/writing_workspace.cpp", "src/ui/writing_workspace_document.cpp"):
        text=text.replace('"writing_workspace_util.hpp"','"ui/writing_workspace_util.hpp"').replace('"editor_diagnostics.hpp"','"ui/editor_diagnostics.hpp"')
    return text
