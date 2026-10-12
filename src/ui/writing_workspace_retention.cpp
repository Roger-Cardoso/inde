#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "writing_semantic_ui_support.hpp"
#include <chrono>

namespace inde::ui {
namespace {
bool same_owner(const application::ProjectService &service,const project::Project &owner) {
  return service.current() && service.current()->path()==owner.path() &&
         service.current()->manifest().project_id==owner.manifest().project_id;
}
}
void WritingWorkspace::run_recovery_maintenance(std::function<std::string()> task) {
  if (recovery_maintenance_task_.valid() || recovery_inventory_task_.valid()) {
    signal_status_message_.emit("Uma operação de recuperação já está em andamento."); return;
  }
  recovery_maintenance_task_=std::async(std::launch::async,std::move(task));
  signal_status_message_.emit("Operação controlada em andamento; nenhum widget sai da thread GTK.");
  recovery_maintenance_delivery_=Glib::signal_timeout().connect([this] {
    if (recovery_maintenance_task_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return true;
    try { signal_status_message_.emit(recovery_maintenance_task_.get()); refresh_recovery_notice(); }
    catch (const std::exception &e) { show_error("Operação não concluída; confira as evidências preservadas",e); }
    return false;
  },50);
}
void WritingWorkspace::show_recovery_retention(unsigned keep_days) {
  if (!service_.current() || !owner_window()) return;
  if (recovery_maintenance_task_.valid() || recovery_inventory_task_.valid()) {
    signal_status_message_.emit("Aguarde a operação de recuperação em andamento."); return;
  }
  const auto owner=*service_.current();
  recovery_inventory_task_=std::async(std::launch::async,[owner,keep_days] {
    return persistence::DocumentRecoveryStore{}.retention(owner,keep_days);
  });
  signal_status_message_.emit("Preparando prévia de retenção, sem apagar ponto de recuperaçãos…");
  recovery_maintenance_delivery_=Glib::signal_timeout().connect([this,owner,keep_days] {
    if (recovery_inventory_task_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return true;
    try {
      auto page=recovery_inventory_task_.get();
      if (same_owner(service_,owner)) render_recovery_retention(owner,keep_days,std::move(page));
    } catch (const std::exception &e) { show_error("Registro de recuperação protegido; retenção indisponível",e); }
    return false;
  },50);
}
void WritingWorkspace::render_recovery_retention(const project::Project &owner,unsigned keep_days,
                                                project::RecoveryRetentionPage page) {
  using namespace semantic_ui;
  auto *dialog=new OverlayDialog("Retenção e exportação de ponto de recuperaçãos",*owner_window(),true);
  dialog->set_secondary_text("Limpeza somente por confirmação. Pendentes, ativos e ilegíveis são protegidos. "
      "Exportações conservam texto, base, seleção e camadas; não são backup integral do Projeto. "
      "Páginas liberadas são reutilizadas pelo SQLite; o arquivo não é compactado durante edição.");
  dialog->add_button("Fechar",Gtk::ResponseType::CANCEL);
  auto *box=dialog->get_content_area();
  label(*box,"Registro de recuperação: "+std::to_string(page.allocated_bytes)+" bytes; "+
             std::to_string(page.reusable_bytes)+" bytes reutilizáveis. "+
             std::to_string(page.items.size())+" registros preservados.");
  auto *days=Gtk::make_managed<Gtk::ComboBoxText>();
  days->append("30","Conservar fechados dos últimos 30 dias (padrão)");
  days->append("90","Conservar fechados dos últimos 90 dias");
  days->append("0","Permitir seleção de todos os fechados — decisão explícita");
  days->set_active_id(std::to_string(keep_days)); box->append(*days);
  button(*box,"Atualizar prévia",[this,dialog,days,owner] {
    if (!same_owner(service_,owner)) return;
    const auto value=static_cast<unsigned>(std::stoul(days->get_active_id()));
    retire(dialog); show_recovery_retention(value);
  });
  auto selected=std::make_shared<std::vector<std::pair<Gtk::CheckButton *,project::RecoveryRetentionEntry>>>();
  for (const auto &entry : page.items) {
    auto *row=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,4);
    row->add_css_class("content-card"); row->set_margin(8);
    label(*row,entry.title+" • "+entry.status+" • "+entry.updated_at);
    label(*row,entry.explanation+" • "+std::to_string(entry.payload_bytes)+" bytes");
    auto *choice=Gtk::make_managed<Gtk::CheckButton>("Selecionar para exportação e limpeza confirmada");
    choice->set_sensitive(entry.removable); row->append(*choice);
    selected->emplace_back(choice,entry);
    auto *export_button=Gtk::make_managed<Gtk::Button>("Exportar ponto de recuperação sem limpar");
    export_button->set_sensitive(entry.exportable); row->append(*export_button);
    export_button->signal_clicked().connect([this,dialog,owner,entry] {
      if (!same_owner(service_,owner)) return;
      retire(dialog);
      run_recovery_maintenance([owner,entry] {
        const auto path=persistence::DocumentRecoveryStore{}.export_record(owner,entry.session_id,entry.checksum);
        return "Ponto de recuperação exportado e reaberto com checksum verificado, sem limpeza: "+path.string();
      });
    });
    box->append(*row);
  }
  if (page.items.empty()) label(*box,"Nenhum ponto de recuperação retido neste Projeto.");
  if (page.more) label(*box,"Prévia limitada a 256 registros; os demais permanecem protegidos.");
  button(*box,"Revisar limpeza dos selecionados",[this,dialog,owner,selected,keep_days] {
    if (!same_owner(service_,owner)) return;
    std::vector<project::RecoveryRetentionEntry> entries;
    std::size_t bytes{};
    for (const auto &[widget,entry] : *selected) if (widget->get_active() && entry.removable) {
      entries.push_back(entry); bytes+=entry.payload_bytes;
    }
    if (entries.empty()) { signal_status_message_.emit("Selecione registros fechados elegíveis; nada foi apagado."); return; }
    retire(dialog);
    auto *confirm=new OverlayDialog("Confirmar exportação e limpeza?",*owner_window(),true);
    confirm->set_secondary_text(std::to_string(entries.size())+" ponto de recuperação(s), "+std::to_string(bytes)+
        " bytes de payload. Primeiro serão exportados e verificados; só então os registros selecionados "
        "serão removidos em uma transação. Documentos, pendências e arquivos exportados não serão apagados. "
        "As exportações ficam em recovery/editor-v1/exports dentro deste Projeto.");
    confirm->add_button("Cancelar",Gtk::ResponseType::CANCEL);
    confirm->add_button("Confirmar exportação e limpeza",Gtk::ResponseType::ACCEPT);
    confirm->signal_response().connect([this,confirm,owner,entries,keep_days](int response) {
      confirm->hide();
      if (response!=Gtk::ResponseType::ACCEPT || !same_owner(service_,owner)) return;
      run_recovery_maintenance([owner,entries,keep_days] {
        persistence::DocumentRecoveryStore store;
        for (const auto &entry : entries) static_cast<void>(store.export_record(owner,entry.session_id,entry.checksum));
        const auto n=store.purge_exported(owner,entries,keep_days);
        return std::to_string(n)+" ponto de recuperação(s) fechado(s) removido(s) após exportação verificada. Recuperáveis pelos arquivos em "+(owner.path()/"recovery/editor-v1/exports").string()+
               "; Documentos e pendências preservados.";
      });
    });
    confirm->present();
  });
  dialog->signal_response().connect([dialog](int) { dialog->hide(); }); dialog->present();
}
} // namespace inde::ui
