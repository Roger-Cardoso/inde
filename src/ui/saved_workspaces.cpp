#include "inde/ui/saved_workspaces.hpp"
#include "inde/ui/ui_preferences.hpp"
#include <glib.h>
namespace inde::ui {
namespace {
void write(const std::string &project, const std::vector<SavedWorkspace> &entries) {
  ViewState::Array values;
  for (const auto &e: entries) values.emplace_back(ViewState::Object{{"id",e.id},{"name",e.name},{"state",e.state}});
  const auto data=serialize_view(ViewState::Object{{"version",1},{"entries",std::move(values)}});
  // Each state has its own 64 KiB limit. Container stores strings, depth <= 4.
  if(data.size()>1500000)throw std::runtime_error("Lista de espaços de trabalho excede o limite; reduza os contextos salvos");
  set_ui_preference(project_preference_group(project),"saved-workspaces",data);
  if (ui_preference(project_preference_group(project),"saved-workspaces")!=data)
    throw std::runtime_error("Não foi possível confirmar o espaço de trabalho salvo");
}
}
std::vector<SavedWorkspace> saved_workspaces(const std::string &project) {
  const auto text=ui_preference(project_preference_group(project),"saved-workspaces");
  if (text.empty()) return {};
  if (text.size()>1500000) throw std::runtime_error("Lista de espaços de trabalho excede o limite");
  // The generic parser is bounded by a flat envelope with string-encoded states.
  int depth{}; bool quoted{},escaped{};
  for (const auto c:text) {
    if (quoted) { if (escaped) escaped=false; else if(c=='\\') escaped=true; else if(c=='"') quoted=false; }
    else if(c=='"') quoted=true;
    else if(c=='{' || c=='[') { if(++depth>4) throw std::runtime_error("Lista de espaços inválida"); }
    else if(c=='}' || c==']') --depth;
  }
  const auto root=persistence::json::parse(text);
  if(root.at("version").as_integer()!=1 || root.at("entries").as_array().size()>20)
    throw std::runtime_error("Versão/lista de espaços de trabalho incompatível");
  std::vector<SavedWorkspace> result;
  for(const auto &v:root.at("entries").as_array()) {
    SavedWorkspace e{v.at("id").as_string(),v.at("name").as_string(),v.at("state").as_string()};
    if(e.id.empty() || e.id.size()>128 || e.name.empty() || e.name.size()>256 ||
       !g_utf8_validate(e.name.data(),e.name.size(),nullptr)) throw std::runtime_error("Espaço de trabalho inválido");
    (void)parse_view_state(e.state);
    if(std::any_of(result.begin(),result.end(),[&](const auto &old){return old.id==e.id;}))
      throw std::runtime_error("Identidade de espaço duplicada");
    result.push_back(std::move(e));
  }
  return result;
}
void save_workspace(const std::string &project,const SavedWorkspace &e) {
  if(e.id.empty() || e.id.size()>128 || e.name.empty() || e.name.size()>256 ||
     !g_utf8_validate(e.name.data(),e.name.size(),nullptr)) throw std::runtime_error("Informe um nome válido para o espaço");
  (void)parse_view_state(e.state);
  auto values=saved_workspaces(project);
  const auto it=std::find_if(values.begin(),values.end(),[&](const auto &v){return v.id==e.id;});
  if(it!=values.end()) *it=e;
  else { if(values.size()==20) throw std::runtime_error("Limite de 20 espaços por Projeto"); values.push_back(e); }
  write(project,values);
}
void remove_workspace(const std::string &project,const std::string &id) {
  auto values=saved_workspaces(project);
  std::erase_if(values,[&](const auto &v){return v.id==id;});
  write(project,values);
}
}
