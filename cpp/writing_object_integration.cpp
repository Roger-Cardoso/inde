#include "inde/project/document_state.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "object_native_ui.hpp"
#include <stdexcept>
namespace inde::ui {
namespace {
bool object_specialty(const std::string &id) {
  return id == "composition" || id == "tables" || id == "formulas" ||
         id == "math";
}
} // namespace
void WritingWorkspace::build_object_specializations() {
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{
           {"composition", "Composição"},
           {"tables", "Tabelas"},
           {"formulas", "Fórmulas"},
           {"math", "Matemática"}})
    specialization_selector_.append(id, name);
  specialty_strip_.set_name("writing-specialty-strip");
  back_button_.set_name("writing-library");
  title_entry_.set_name("writing-title");
  undo_button_.set_name("writing-undo");
  redo_button_.set_name("writing-redo");
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{
           {"text", "Texto"},
           {"semantic", "Semântica"},
           {"linguistic", "Linguística"},
           {"structure", "Estrutura"},
           {"bindings", "Vínculos"},
           {"composition", "Composição"},
           {"tables", "Tabelas"},
           {"formulas", "Fórmulas"},
           {"math", "Matemática"}}) {
    auto *b = Gtk::make_managed<Gtk::ToggleButton>(name);
    b->set_name("specialty-" + id);
    b->set_tooltip_text("Abrir a especialidade " + name + " deste Documento");
    b->signal_clicked().connect([this, id] {
      if (changing_specialization_)
        return;
      set_editor_specialization(id);
      if (object_specialty(id) && editor_specialization_ == id &&
          object_editor_)
        object_editor_->show_tools(true);
    });
    specialty_strip_.append(*b);
  }
  // Keep the old selector for saved view compatibility, with the strip as
  // access.
  specialization_selector_.set_visible(false);
  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroll->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::NEVER);
  scroll->set_child(specialty_strip_);
  editor_page_.insert_child_after(*scroll, editor_toolbar_);
}
bool WritingWorkspace::objects_can_leave() const {
  return !object_editor_ || object_editor_->can_leave();
}
void WritingWorkspace::show_document_objects() {
  set_editor_specialization("composition");
}
void WritingWorkspace::show_object_specialization(const std::string &id) {
  if (!object_specialty(id)) {
    document_surfaces_.set_visible_child("text");
    return;
  }
  if (!current_document_ || !owner_window())
    throw std::runtime_error("Abra um Documento na Escrita.");
  flush_changes();
  if (!object_editor_) {
    object_editor_ = std::make_unique<ObjectDocumentEditor>(
        service_, *owner_window(), current_document_->id);
    object_editor_->save_document = [this] { flush_changes(); };
    object_editor_->editing_changed = [this](bool editing) {
      editor_toolbar_.set_sensitive(!editing);
      specialty_strip_.set_sensitive(!editing);
      specialization_bar_.set_sensitive(!editing);
    };
    object_editor_->edit_paragraph = [this](const std::string &paragraph) {
      const auto row =
          std::find_if(current_document_->paragraphs.rows.begin(),
                       current_document_->paragraphs.rows.end(),
                       [&](const auto &p) { return p.id == paragraph; });
      if (row == current_document_->paragraphs.rows.end())
        return;
      const auto offset = row->start;
      set_editor_specialization("text");
      navigate_to_offset(offset);
    };
    object_editor_->commit_document = [this](const project::Document &after) {
      committing_object_document_ = true;
      try {
        const auto result =
            apply_editorial_change(*service_.writing().document(after.id),
                                   after, "Mover na composição");
        committing_object_document_ = false;
        return result;
      } catch (...) {
        committing_object_document_ = false;
        throw;
      }
    };
    document_surfaces_.add(*object_editor_, "objects");
  }
  flush_document_objects();
  object_editor_->set_specialty(id);
  document_surfaces_.set_visible_child("objects");
}
void WritingWorkspace::flush_document_objects() {
  if (!object_editor_ || !current_document_ || !service_.current())
    return;
  if (!refreshing_visibility_)
    object_editor_->synchronize_document(capture_editor_state());
  if (active_visibility_id_.empty())
    object_editor_->set_visible_text(std::nullopt);
  else {
    std::map<std::string, std::string> visible;
    const Glib::ustring body(current_document_->content);
    for (const auto &row : current_document_->paragraphs.rows) {
      const auto ranges = visible_ranges(row.start, row.end);
      if (ranges.empty())
        continue;
      std::string out;
      std::size_t previous = row.start;
      for (const auto &r : ranges) {
        if (previous < r.start)
          out += "…";
        out += body.substr(r.start, r.end - r.start).raw();
        previous = r.end;
      }
      if (previous < row.end)
        out += "…";
      visible[row.id] = out;
    }
    object_editor_->set_visible_text(std::move(visible));
  }
  if (!refreshing_visibility_)
    object_editor_->save_for_project(service_.current()->path());
}
void WritingWorkspace::reset_document_objects() {
  if (!object_editor_)
    return;
  document_surfaces_.set_visible_child("text");
  document_surfaces_.remove(*object_editor_);
  object_editor_.reset();
}
} // namespace inde::ui
