#include "inde/application/document_export.hpp"
#include "inde/persistence/json.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/saved_workspaces.hpp"
#include "inde/ui/ui_preferences.hpp"
#include <glib.h>
#include <cassert>
#include <fstream>
#include <iostream>
namespace {
template<class F> void rejects(F fn) {bool threw{};try {fn();}catch(const std::exception &){threw=true;}assert(threw);}
std::string read(const std::filesystem::path &p) { std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}}; }
}
int main() {
  const auto root=std::filesystem::temp_directory_path()/("inde-continuity-tests-"+inde::project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME",(root/"config").c_str(),true);
  using namespace inde;
  const auto group=ui::project_preference_group("A");
  const auto state=ui::serialize_view(ui::ViewState::Object{{"version",1},{"project","A"},{"cursor",4}});
  ui::save_workspace("A",{"one","Escrita à noite",state});
  assert(ui::saved_workspaces("A").size()==1 && ui::saved_workspaces("B").empty());
  ui::save_workspace("A",{"one","Revisão",state});assert(ui::saved_workspaces("A")[0].name=="Revisão");
  for(int i=1;i<20;++i)ui::save_workspace("A",{std::to_string(i),"Nome",state});
  rejects([&]{ui::save_workspace("A",{"overflow","Nome",state});});
  ui::remove_workspace("A","one");assert(ui::saved_workspaces("A").size()==19);
  rejects([&]{ui::save_workspace("A",{"bad","Nome",std::string(65537,'x')});});
  rejects([&]{ui::parse_view_state(std::string(20,'[')+std::string(20,']'));});
  rejects([&]{ui::view_offset(ui::ViewState(-1));});
  // A valid individual state can nearly double when stored as a JSON string.
  // Refuse an oversized aggregate before replacing the readable old list.
  const auto large=ui::serialize_view(ui::ViewState::Object{{"search",std::string(30000,'"')}});
  std::size_t accepted{};bool limited{};
  for(int i=0;i<20;++i) {
    const auto before=ui::ui_preference(ui::project_preference_group("large"),"saved-workspaces");
    try {ui::save_workspace("large",{std::to_string(i),"Grande",large});++accepted;}
    catch(const std::exception &) {limited=true;assert(ui::ui_preference(ui::project_preference_group("large"),"saved-workspaces")==before);break;}
  }
  assert(limited && accepted>0 && ui::saved_workspaces("large").size()==accepted);
  const auto corrupt="{\"version\":99,\"entries\":[]}";
  ui::set_ui_preference(group,"saved-workspaces",corrupt);
  rejects([&]{ui::save_workspace("A",{"two","Nome",state});});
  assert(ui::ui_preference(group,"saved-workspaces")==corrupt);
  const auto preferences=root/"config/inde/interface.ini";
  {std::ofstream bad(preferences);bad<<"[truncated preferences";}
  const auto broken=read(preferences);
  rejects([&]{ui::save_workspace("A",{"two","Nome",state});});
  assert(read(preferences)==broken); // Malformed key file is never overwritten.
  project::Document doc;doc.id=project::new_uuid();doc.title="Água <script>🙂</script>";doc.updated_at=project::utc_now();
  doc.content="Água 🙂 <script>alert('x')</script> &\n\nFim\r\n";
  doc.formatting={{project::DocumentTextStyle::Bold,0,7,{}},
    {project::DocumentTextStyle::Italic,3,9,{}},
    {project::DocumentTextStyle::Underline,0,4,{}},
    {project::DocumentTextStyle::Strikethrough,0,4,{}},
    {project::DocumentTextStyle::Foreground,0,4,"#102030"},
    {project::DocumentTextStyle::AlignCenter,0,7,{}}};
  const auto out=application::prepare_document_export("A",doc);
  assert(out.text==doc.content && out.html.find("<script>")==std::string::npos);
  assert(out.html.find("&lt;script&gt;")!=std::string::npos);
  assert(out.html.find("underline line-through")!=std::string::npos);
  assert(out.html.find("&#13;")!=std::string::npos);
  assert(std::any_of(out.limitations.begin(),out.limitations.end(),[](const auto &s){return s.find("align-center")!=std::string::npos;}));
  const auto dest=root/"export";application::write_document_export(out,dest);
  assert(read(dest/"documento.txt")==doc.content && read(dest/"documento.html")==out.html);
  const auto report=persistence::json::parse(read(dest/"relatorio.json"));
  assert(report.at("readback_verified").as_boolean() && report.at("document_id").as_string()==doc.id);
  rejects([&]{application::write_document_export(out,dest);});assert(read(dest/"documento.txt")==doc.content);
  std::filesystem::create_directory_symlink(root,root/"symlink");
  rejects([&]{application::write_document_export(out,root/"symlink"/"escape");});
  auto invalid=doc;invalid.formatting[0].end_offset=100000;rejects([&]{application::prepare_document_export("A",invalid);});
  invalid=doc;invalid.content.push_back('\0');rejects([&]{application::prepare_document_export("A",invalid);});
  invalid=doc;invalid.content="\xff";rejects([&]{application::prepare_document_export("A",invalid);});
  doc.content.clear();doc.formatting.clear();assert(application::prepare_document_export("A",doc).text.empty());
  std::cout<<"PASS continuity/preferences/export/readback; fixture="<<dest<<"\n";
  // Preserve the tiny export for independent HTML parser readback in QA.
}
