#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/review_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/ui/application_theme.hpp"
#include <algorithm>
namespace inde::ui {
ReviewWorkspace::ReviewWorkspace(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12), service_(service) {
  set_margin(18); set_hexpand(true); set_vexpand(true);
  title_.add_css_class("title-1"); title_.set_halign(Gtk::Align::START);
  title_.set_hexpand(true); header_.append(title_);
  filter_.append("all", "Comentários e pendências");
  filter_.append("editorial", "Textos para revisar");
  filter_.append("documents", "Todos os Documentos");
  filter_.append("comments", "Comentários");
  filter_.append("issues", "Pendências de marcação");
  filter_.set_active_id("all");
  set_accessible_label(filter_, "Tipo de revisão");
  header_.append(filter_); header_.append(refresh_); queue_.append(header_);
  search_.set_placeholder_text("Pesquisar Documentos para revisar");queue_.append(search_);
  summary_.set_xalign(0); summary_.set_wrap(true); summary_.add_css_class("dim-label");
  queue_.append(summary_);
  list_.set_selection_mode(Gtk::SelectionMode::NONE);
  scroll_.set_child(list_); scroll_.set_vexpand(true);
  scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  queue_.append(scroll_); footer_.append(previous_); footer_.append(next_); queue_.append(footer_);
  detail_scroll_.set_policy(Gtk::PolicyType::NEVER,Gtk::PolicyType::AUTOMATIC);
  detail_scroll_.set_vexpand(true);detail_scroll_.set_child(detail_body_);detail_.append(detail_scroll_);
  pages_.set_hhomogeneous(false);pages_.set_vhomogeneous(false);pages_.set_vexpand(true);
  pages_.add(queue_,"queue");pages_.add(detail_,"detail");pages_.add(comparison_,"comparison");append(pages_);
  comparison_header_.append(comparison_back_);comparison_header_.append(differences_only_);
  comparison_header_.append(comparison_previous_);comparison_header_.append(comparison_next_);comparison_.append(comparison_header_);
  comparison_info_.set_wrap(true);comparison_info_.set_xalign(0);comparison_.append(comparison_info_);
  comparison_text_.set_editable(false);comparison_text_.set_cursor_visible(false);comparison_text_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  comparison_text_.set_left_margin(14);comparison_text_.set_right_margin(14);comparison_text_.add_css_class("review-comparison");
  set_accessible_label(comparison_text_,"Comparação textual somente leitura; sinais mais e menos identificam mudanças");
  comparison_scroll_.set_child(comparison_text_);comparison_scroll_.set_vexpand(true);comparison_.append(comparison_scroll_);
  comparison_back_.signal_clicked().connect([this]{comparison_stop_.request_stop();comparison_delivery_.disconnect();if(!selected_document_.empty())show_document(selected_document_);else pages_.set_visible_child("queue");});
  differences_only_.signal_toggled().connect([this]{render_comparison();});
  const auto navigate=[this](bool previous) {
    if(difference_offsets_.empty())return;
    difference_index_=previous?(difference_index_+difference_offsets_.size()-1)%difference_offsets_.size():(difference_index_+1)%difference_offsets_.size();
    auto iter=comparison_text_.get_buffer()->get_iter_at_offset(difference_offsets_[difference_index_]);comparison_text_.scroll_to(iter,0.1);
  };
  comparison_previous_.signal_clicked().connect([navigate]{navigate(true);});comparison_next_.signal_clicked().connect([navigate]{navigate(false);});
  theme_changed_=signal_application_theme_changed().connect([this]{color_comparison();});
  refresh_.signal_clicked().connect([this] { refresh(); });
  filter_.signal_changed().connect([this] {if(!refreshing_controls_){offset_=0;load_page();}});
  search_.signal_search_changed().connect([this]{if(!refreshing_controls_){offset_=0;load_page();}});
  previous_.signal_clicked().connect([this] { offset_ = offset_ >= 50 ? offset_ - 50 : 0; load_page(); });
  next_.signal_clicked().connect([this] { offset_ += 50; load_page(); });
  reset();
}
ReviewWorkspace::~ReviewWorkspace() { scan_.disconnect();comparison_delivery_.disconnect();comparison_stop_.request_stop();theme_changed_.disconnect(); }
void ReviewWorkspace::reset() {
  scan_.disconnect(); offset_ = total_ = position_ = found_ = 0;
  documents_.clear(); types_.clear(); project_id_.clear();
  comparison_delivery_.disconnect();comparison_stop_.request_stop();difference_.reset();
  comparison_left_source_.reset();comparison_right_source_.reset();
  selected_document_.clear();comparison_left_.clear();comparison_right_.clear();
  pages_.set_visible_child("queue");
  while(auto *child=detail_body_.get_first_child())detail_body_.remove(*child);
  refreshing_controls_=true;search_.set_text("");filter_.set_active_id("all");refreshing_controls_=false;
  while (auto *child = list_.get_first_child()) list_.remove(*child);
  summary_.set_text("Comentários e pendências do texto aparecem aqui.");
  previous_.set_sensitive(false); next_.set_sensitive(false);
}
void ReviewWorkspace::refresh() {
  if(!service_.current() || (!project_id_.empty() && project_id_!=service_.current()->manifest().project_id)){reset();if(!service_.current())return;}
  if(pages_.get_visible_child_name()=="detail" && !selected_document_.empty())show_document(selected_document_);
  else if(pages_.get_visible_child_name()=="comparison" && !comparison_left_.empty() && !comparison_right_.empty()) {
    const auto a=service_.writing().document(comparison_left_),b=service_.writing().document(comparison_right_);
    if(a && b)compare(*a,*b);else {pages_.set_visible_child("queue");load_page();}
  } else load_page();
}
void ReviewWorkspace::load_page() {
  scan_.disconnect();
  while (auto *child = list_.get_first_child()) list_.remove(*child);
  if (!service_.current()) { reset(); return; }
  try {
    project_id_ = service_.current()->manifest().project_id;
    types_ = service_.writing().semantic_types();
    persistence::DocumentQuery query;query.search=search_.get_text().raw();
    total_ = service_.writing().document_count(query);
    if (offset_ >= total_) offset_ = 0;
    query.limit = 50; query.offset = offset_;
    documents_ = service_.writing().document_summaries(query);
    position_ = found_ = 0;
    previous_.set_sensitive(offset_ > 0);
    next_.set_sensitive(offset_ + documents_.size() < total_);
    summary_.set_text("Consultando comentários e pendências…");
    scan_ = Glib::signal_timeout().connect(sigc::mem_fun(*this, &ReviewWorkspace::scan_next), 10);
  } catch (const std::exception &error) {
    summary_.set_text(std::string("Não foi possível consultar a revisão: ") + author::diagnostic(error.what()));
  }
}
bool ReviewWorkspace::scan_next() {
  if (!service_.current() || service_.current()->manifest().project_id != project_id_) return false;
  if (position_ >= documents_.size()) {
    summary_.set_text(std::to_string(found_) + " documentos para revisar nesta página · " +
        std::to_string(offset_ + documents_.size()) + " de " + std::to_string(total_) + " consultados");
    if (!found_) {
      auto *empty = Gtk::make_managed<Gtk::Label>(
          "Nenhum Documento corresponde ao filtro nesta página.\nEscolha Todos os Documentos para iniciar uma revisão.");
      empty->set_margin(32); empty->set_wrap(true); empty->add_css_class("dim-label");
      list_.append(*empty);
    }
    return false;
  }
  try {
    auto value = service_.writing().document(documents_[position_++].id);
    if (!value) return true;
    auto &d = *value;
    const auto counts=application::review_counts(d);const auto comments=counts.open_comments;
    const auto state=application::editorial_state(d);
    application::SemanticAnchorEngine::inspect(d);
    const auto diagnostics = project::semantic_diagnostics(d, types_);
    const auto filter = filter_.get_active_id();
    if ((filter == "comments" && comments == 0) || (filter == "issues" && diagnostics.empty()) ||
        (filter=="all" && comments==0 && counts.pending_suggestions==0 && diagnostics.empty() && state.state!="in_review" && state.state!="changes_requested" && !state.outdated) ||
        (filter=="editorial" && state.state!="in_review" && state.state!="changes_requested" && !state.outdated && counts.pending_suggestions==0))return true;
    ++found_;
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    row->add_css_class("review-row");
    auto *button = Gtk::make_managed<Gtk::Button>(d.title);
    button->set_tooltip_text(d.title);
    if(auto *label=dynamic_cast<Gtk::Label*>(button->get_child())) {label->set_ellipsize(Pango::EllipsizeMode::END);label->set_max_width_chars(80);}
    button->set_halign(Gtk::Align::START);
    button->signal_clicked().connect([this, id = d.id] { show_document(id); });
    row->append(*button);
    const auto badge=state.state=="approved"?"Revisão aprovada":state.state=="in_review"?"Em revisão":state.state=="changes_requested"?"Ajustes solicitados":"Em escrita";
    auto *count = Gtk::make_managed<Gtk::Label>(std::string(badge)+(state.outdated?" — decisão desatualizada":"")+" · "+std::to_string(comments) + " comentários abertos · " +
        std::to_string(counts.pending_suggestions)+" sugestões · "+std::to_string(diagnostics.size()) + " pendências");
    count->set_xalign(0); count->add_css_class("review-kind"); row->append(*count);
    if (!diagnostics.empty()) {
      auto *description = Gtk::make_managed<Gtk::Label>(diagnostics.front().message);
      description->set_xalign(0); description->set_wrap(true); description->add_css_class("dim-label");
      row->append(*description);
    }
    list_.append(*row);
  } catch (const std::exception &error) {
    const auto name=position_>0?documents_[position_-1].title:std::string("Documento");
    auto *unavailable=Gtk::make_managed<Gtk::Label>("Não foi possível avaliar “"+name+"”: "+author::diagnostic(error.what())+"\nNenhuma aprovação ou alteração foi inferida.");
    unavailable->set_xalign(0);unavailable->set_wrap(true);unavailable->add_css_class("dim-label");unavailable->set_margin(12);list_.append(*unavailable);++found_;
    return true;
  }
  return true;
}
ViewState ReviewWorkspace::capture_view() const {
  return ViewState::Object{{"page",pages_.get_visible_child_name().raw()},{"document",selected_document_},
    {"search",search_.get_text().raw()},{"filter",filter_.get_active_id().raw()},{"offset",static_cast<std::int64_t>(offset_)},
    {"left",comparison_left_},{"right",comparison_right_},{"differences_only",differences_only_.get_active()}};
}
void ReviewWorkspace::restore_view(const ViewState &v) {
  const auto page=v.at("page").as_string(),id=v.at("document").as_string();
  if(page!="queue" && page!="detail" && page!="comparison")throw std::runtime_error("Área de revisão inválida");
  const auto search=v.at("search").as_string(),filter=v.at("filter").as_string();
  const auto left=v.at("left").as_string(),right=v.at("right").as_string();
  const auto offset=view_offset(v.at("offset"));const auto only=v.at("differences_only").as_boolean();
  if(id.size()>128 || left.size()>128 || right.size()>128 || search.size()>4096 || filter.size()>32)
    throw std::runtime_error("Contexto de revisão excede o limite");
  refreshing_controls_=true;search_.set_text(search);
  if(!filter_.set_active_id(filter))filter_.set_active_id("all");
  offset_=offset;refreshing_controls_=false;
  pages_.set_visible_child("queue");load_page();
  if(page!="queue" && !id.empty() && service_.writing().document(id))show_document(id);
  if(page=="comparison") {
    const auto a=service_.writing().document(left),b=service_.writing().document(right);
    differences_only_.set_active(only);if(a && b && a->id!=b->id)compare(*a,*b);
  }
}
}
