#include "inde/application/document_edit_session.hpp"
#include "inde/application/project_service.hpp"
#include "inde/project/document_paragraphs.hpp"
#include "object_composite_pilot.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using V=inde::persistence::json::Value;
using Session=inde::objects::pilot::Session;
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
V view(Session &s){return s.request(V::Object{{"op","view"}}).at("state");}
void command(Session &s,const char *op,V::Object args){args["op"]=op;args["expected"]=view(s).at("revision");auto r=s.request(args);if(!r.at("accepted").as_boolean())throw std::runtime_error(r.at("error").as_string());}
int main() try {
 char pattern[]="/tmp/inde-sync-bench-XXXXXX";const auto name=::mkdtemp(pattern);if(!name)throw std::runtime_error("mkdtemp");
 const std::filesystem::path root=name;::setenv("XDG_CONFIG_HOME",(root/"config").c_str(),1);
 for(const int edits:{0,8,32}){
  inde::application::ProjectService service;service.create(root/("Teste-"+std::to_string(edits)+".inde"),"Sintético");
  auto d=service.writing().create_document("Sincronização sem GTK");
  const std::string prefix="Abertura preservada";d.content=prefix+std::string(10,'\n');
  for(int i=0;i<200;++i)d.content+=(i?"\n":"")+std::string("Linha ")+std::to_string(i)+" preservada.";
  d.paragraphs=inde::project::identify_paragraphs(d.content);d=service.writing().update_document(d);
  const auto parent=service.current()->path()/"documents/composed";std::filesystem::create_directory(parent);std::filesystem::permissions(parent,std::filesystem::perms::owner_all);
  Session s(parent/d.id,service.current()->manifest().project_id,d,inde::objects::pilot::DocumentBinding::Live);
  if(edits){command(s,"set_context",{{"context","composition"}});const auto id=view(s).at("paragraphs").as_array()[0].at("id");for(int i=0;i<edits;++i)command(s,"set_presentation",{{"id",id},{"width",i%2?90:80},{"horizontal","start"},{"text_align","left"},{"vertical","start"}});}
  for(int sample=0;sample<3;++sample){
   const auto a=Clock::now();auto after=inde::application::replace_selection({d,{}},prefix.size(),prefix.size()+1,"").document;const auto b=Clock::now();
   s.synchronize_document(after);const auto c=Clock::now();const auto projection=view(s);const auto e=Clock::now();
   if(s.current_document().content!=after.content)throw std::runtime_error("content mismatch");
   std::cout<<"{\"prose_lines\":200,\"setup_style_edits\":"<<edits<<",\"sample\":"<<sample<<",\"edit_ms\":"<<ms(a,b)<<",\"adopt_ms\":"<<ms(b,c)<<",\"view_ms\":"<<ms(c,e)<<",\"content_exact\":true}"<<std::endl;d=after;
  }
 }
 std::filesystem::remove_all(root);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
