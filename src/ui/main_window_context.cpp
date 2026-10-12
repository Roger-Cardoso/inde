#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/saved_workspaces.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/project/manifest.hpp"
namespace inde::ui {
namespace {
void retire(OverlayDialog *dialog) {
  dialog->hide();
  Glib::signal_idle().connect_once([dialog]{delete dialog;});
}
}
ViewState MainWindow::capture_context() {
  if(!service_.current())throw std::runtime_error("Abra um Projeto para guardar o contexto");
  const auto writing=pending_views_.contains("writing") ? pending_views_.at("writing"):writing_workspace_.capture_view();
  const auto planning=pending_views_.contains("planning") ? pending_views_.at("planning"):planning_workspace_.capture_view();
  const auto editorial=pending_views_.contains("editorial") ? pending_views_.at("editorial"):editorial_workspace_.capture_view();
  const auto review=pending_views_.contains("review") ? pending_views_.at("review"):review_workspace_.capture_view();
  return ViewState::Object{{"version",1},{"project",service_.current()->manifest().project_id},
    {"workspace",std::string(workspace_name(workspace_shell_.current()))},{"focus",focused_},
    {"writing",writing},{"planning",planning},{"editorial",editorial},{"review",review}};
}
void MainWindow::update_builder_return() {
  bool visible=false;
  if(builder_return_context_) {
    const auto source=builder_return_context_->at("workspace").as_string();
    visible=source!=workspace_name(workspace_shell_.current());
    builder_return_.set_label(source=="writing"?"Voltar à escrita":"Voltar ao planejamento");
  }
  builder_return_.set_visible(visible);
}
void MainWindow::restore_pending_view(WorkspaceId target) {
  const auto key=std::string(workspace_name(target));const auto it=pending_views_.find(key);
  if(it==pending_views_.end())return;
  const auto value=it->second;pending_views_.erase(it);
  try {
    if(target==WorkspaceId::Writing)writing_workspace_.restore_view(value);
    else if(target==WorkspaceId::Planning)planning_workspace_.restore_view(value);
    else if(target==WorkspaceId::Editorial)editorial_workspace_.restore_view(value);
    else if(target==WorkspaceId::Review)review_workspace_.restore_view(value);
  } catch(const std::exception &error) {
    context_warning_=std::string("Uma localização salva não pôde ser restaurada: ")+author::diagnostic(error.what());
    status_bar_.set_text(context_warning_);
  }
}
void MainWindow::restore_context(const ViewState &v) {
  if(!service_.current() || v.at("version").as_integer()!=1 ||
      v.at("project").as_string()!=service_.current()->manifest().project_id)
    throw std::runtime_error("Contexto pertence a outro Projeto ou versão");
  const auto name=v.at("workspace").as_string();const auto focus=v.at("focus").as_boolean();
  auto target=WorkspaceId::Writing;bool known{};
  for(const auto id:{WorkspaceId::Writing,WorkspaceId::Planning,WorkspaceId::Editorial,
       WorkspaceId::Review,WorkspaceId::Catalog,WorkspaceId::Graphs,WorkspaceId::Cartography})
    if(name==workspace_name(id)) {target=id;known=true;}
  if(!known)throw std::runtime_error("Tarefa do contexto inválida");
  (void)v.at("writing").as_object();(void)v.at("planning").as_object();(void)v.at("editorial").as_object();
  const auto review=v.as_object().find("review");
  if(review!=v.as_object().end())(void)review->second.as_object();
  writing_workspace_.flush_changes();
  pending_views_={{"writing",v.at("writing")},{"planning",v.at("planning")},{"editorial",v.at("editorial")}};
  if(review!=v.as_object().end())pending_views_.emplace("review",review->second);
  restoring_context_=true;
  try {
    show_workspace(target);
    focus_button_.set_active(focus && target==WorkspaceId::Writing);
    set_focus_mode(focus && target==WorkspaceId::Writing);
    restored_project_id_=service_.current()->manifest().project_id;
  } catch(...) {restoring_context_=false;throw;}
  restoring_context_=false;
}
void MainWindow::manage_workspaces() {
  if(!service_.current() || modal_host_.has_dialog())return;
  try {
    const auto project=service_.current()->manifest().project_id;
    const auto entries=saved_workspaces(project);
    const auto snapshot=serialize_view(capture_context());
    auto *dialog=new OverlayDialog("Espaços de trabalho",*this,true);
    dialog->set_secondary_text("Guarde tarefa, modo Foco e contexto de Escrita, Planejar, Manuscrito e Revisar. Não copia textos nem altera a Obra. Mapas e gráficos guardam somente a tarefa, não seus filtros internos.");
    dialog->add_button("Fechar",Gtk::ResponseType::CLOSE);
    auto *body=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,10);body->set_margin(12);
    auto *row=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL,8);
    auto *name=Gtk::make_managed<Gtk::Entry>();name->set_placeholder_text("Nome do novo espaço");name->set_max_length(80);name->set_hexpand(true);
    set_accessible_label(*name,"Nome do espaço de trabalho");
    auto *save=Gtk::make_managed<Gtk::Button>("Salvar este espaço de trabalho");row->append(*name);row->append(*save);body->append(*row);
    auto *message=Gtk::make_managed<Gtk::Label>();message->set_wrap(true);message->set_xalign(0);body->append(*message);
    auto *scroll=Gtk::make_managed<Gtk::ScrolledWindow>();scroll->set_max_content_height(320);scroll->set_propagate_natural_height(true);
    auto *list=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,8);scroll->set_child(*list);body->append(*scroll);
    if(entries.empty()) { auto *empty=Gtk::make_managed<Gtk::Label>("Nenhum espaço salvo neste Projeto.");list->append(*empty); }
    const auto current=[this,project] {
      if(!service_.current() || service_.current()->manifest().project_id!=project)throw std::runtime_error("O Projeto ativo mudou");
    };
    save->signal_clicked().connect([this,dialog,name,message,project,snapshot,current] {
      try { current();const auto label=name->get_text().raw();
        if(label.find_first_not_of(" \t\r\n")==std::string::npos)throw std::runtime_error("Informe um nome para o espaço");
        save_workspace(project,{project::new_uuid(),label,snapshot});retire(dialog);manage_workspaces();
      } catch(const std::exception &error) {message->set_text(author::diagnostic(error.what()));}
    });
    for(const auto &e:entries) {
      auto *card=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,6);card->add_css_class("content-card");card->set_margin(4);
      auto *label=Gtk::make_managed<Gtk::Label>(e.name);label->set_xalign(0);label->set_ellipsize(Pango::EllipsizeMode::END);card->append(*label);
      auto *actions=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL,6);
      auto *open=Gtk::make_managed<Gtk::Button>("Retomar");auto *replace=Gtk::make_managed<Gtk::Button>("Atualizar este espaço com a configuração atual");
      auto *remove=Gtk::make_managed<Gtk::Button>("Remover espaço");actions->append(*open);actions->append(*replace);actions->append(*remove);card->append(*actions);list->append(*card);
      open->signal_clicked().connect([this,dialog,message,e,current] {
        try {
          current();const auto state=parse_view_state(e.state);writing_workspace_.flush_changes();dialog->hide();
          restore_context(state);retire(dialog);remember_context();status_bar_.set_text("Espaço retomado: "+e.name+(context_warning_.empty()?"":" — "+context_warning_));
        } catch(const std::exception &error){message->set_text(author::diagnostic(error.what()));dialog->present();}
      });
      replace->signal_clicked().connect([this,dialog,message,e,project,snapshot,current] {
        try {current();save_workspace(project,{e.id,e.name,snapshot});retire(dialog);manage_workspaces();}
        catch(const std::exception &error){message->set_text(author::diagnostic(error.what()));}
      });
      remove->signal_clicked().connect([this,dialog,message,e,project,current] {
        try {current();remove_workspace(project,e.id);retire(dialog);manage_workspaces();}
        catch(const std::exception &error){message->set_text(author::diagnostic(error.what()));}
      });
    }
    dialog->get_content_area()->append(*body);
    dialog->signal_response().connect([dialog](int){dialog->hide();});dialog->present();
  } catch(const std::exception &error){show_error("Não foi possível abrir os espaços de trabalho",error);}
}
}
