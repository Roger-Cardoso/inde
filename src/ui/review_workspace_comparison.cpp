#include "inde/ui/author_diagnostics.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/review_workspace.hpp"
#include <chrono>
namespace inde::ui {
void ReviewWorkspace::compare(const project::Document &a,
                              const project::Document &b) {
  if (a.content.size() + b.content.size() > 2 * 1024 * 1024)
    throw std::runtime_error(
        "A comparação visual admite até 2 MiB de texto combinado. Divida a "
        "revisão em Documentos menores; nenhum texto foi truncado.");
  comparison_stop_.request_stop();
  comparison_delivery_.disconnect();
  comparison_stop_ = std::stop_source{};
  const auto token = comparison_stop_.get_token();
  comparison_left_ = a.id;
  comparison_right_ = b.id;
  project_id_ = service_.current()->manifest().project_id;
  comparison_left_source_ = a;
  comparison_right_source_ = b;
  difference_.reset();
  comparison_text_.get_buffer()->set_text("");
  comparison_previous_.set_sensitive(false);
  comparison_next_.set_sensitive(false);
  comparison_info_.set_text("Base: " + a.title + " → Documento: " + b.title +
                            "\nCalculando diferenças — somente leitura…");
  for (auto *child = comparison_.get_first_child(); child;) {
    auto *next = child->get_next_sibling();
    if (child->has_css_class("review-source-identity"))
      comparison_.remove(*child);
    child = next;
  }
  auto *identity = Gtk::make_managed<Gtk::Expander>("Identidade das fontes");
  identity->add_css_class("review-source-identity");
  auto *metadata = Gtk::make_managed<Gtk::Label>(
      "Base: " + a.id + " · " + a.updated_at + "\nDocumento: " + b.id + " · " +
      b.updated_at);
  metadata->set_xalign(0);
  metadata->set_wrap(true);
  metadata->set_selectable(true);
  metadata->add_css_class("dim-label");
  identity->set_child(*metadata);
  comparison_.insert_child_after(*identity, comparison_info_);
  pages_.set_visible_child("comparison");
  comparison_future_ = std::async(std::launch::async, [a, b, token] {
    return application::compare_documents(a, b, token);
  });
  const auto project = project_id_;
  comparison_delivery_ = Glib::signal_timeout().connect(
      [this, project] {
        if (!service_.current() ||
            service_.current()->manifest().project_id != project ||
            pages_.get_visible_child_name() != "comparison") {
          comparison_stop_.request_stop();
          return false;
        }
        if (comparison_future_.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready)
          return true;
        finish_comparison();
        return false;
      },
      30);
}
void ReviewWorkspace::finish_comparison() {
  try {
    difference_ = comparison_future_.get();
    const auto a = service_.writing().document(comparison_left_),
               b = service_.writing().document(comparison_right_);
    if (!a || !b)
      throw std::runtime_error(
          "Documento da comparação não está mais disponível");
    if (!comparison_left_source_ || !comparison_right_source_ ||
        !project::same_document_state(*a, *comparison_left_source_) ||
        !project::same_document_state(*b, *comparison_right_source_))
      throw std::runtime_error(
          "A fonte mudou durante o cálculo; reabra a comparação");
    comparison_info_.set_text(
        "Base: " + a->title + " → Documento: " + b->title +
        "\n− Base: " + std::to_string(difference_->removed) +
        " linhas · + Documento: " + std::to_string(difference_->added) +
        " linhas" +
        (difference_->coarse
             ? " · comparação por blocos (limite de cálculo atingido)"
             : "") +
        (difference_->layers_differ ? "\nCamadas/metadados diferem e não estão "
                                      "incorporados nesta comparação textual."
                                    : "\nSomente o corpo textual é comparado; "
                                      "não há aplicação automática."));
    render_comparison();
  } catch (const std::exception &e) {
    difference_.reset();
    comparison_info_.set_text(std::string("Comparação indisponível: ") +
                              author::diagnostic(e.what()));
  }
}
void ReviewWorkspace::color_comparison() {
  auto buffer = comparison_text_.get_buffer();
  auto removed = buffer->get_tag_table()->lookup("review-removed"),
       added = buffer->get_tag_table()->lookup("review-added");
  if (!removed)
    removed = buffer->create_tag("review-removed");
  if (!added)
    added = buffer->create_tag("review-added");
  Gdk::RGBA fg, bg;
  if (get_style_context()->lookup_color("inde_danger", fg))
    removed->property_foreground_rgba() = fg;
  if (get_style_context()->lookup_color("inde_danger_bg", bg))
    removed->property_background_rgba() = bg;
  if (get_style_context()->lookup_color("inde_success", fg))
    added->property_foreground_rgba() = fg;
  if (get_style_context()->lookup_color("inde_success_bg", bg))
    added->property_background_rgba() = bg;
}
void ReviewWorkspace::render_comparison() {
  if (!difference_)
    return;
  color_comparison();
  auto buffer = comparison_text_.get_buffer();
  auto removed = buffer->get_tag_table()->lookup("review-removed"),
       added = buffer->get_tag_table()->lookup("review-added");
  struct Range {
    int a, b;
    application::DifferenceKind kind;
  };
  std::vector<Range> ranges;
  std::string rendered;
  std::size_t characters{};
  difference_offsets_.clear();
  difference_index_ = 0;
  for (const auto &line : difference_->lines) {
    if (differences_only_.get_active() &&
        line.kind == application::DifferenceKind::Equal)
      continue;
    const auto prefix = line.kind == application::DifferenceKind::Removed ? "− "
                        : line.kind == application::DifferenceKind::Added
                            ? "+ "
                            : "  ";
    const auto number=line.kind==application::DifferenceKind::Added?line.right_line:line.left_line;
    auto value = std::string(prefix) + std::to_string(number) + "  " + line.text;
    if (value.empty() || value.back() != '\n')
      value += "\n";
    const auto begin = characters;
    characters += project::utf8_character_count(value);
    rendered += value;
    if (line.kind != application::DifferenceKind::Equal) {
      ranges.push_back(
          {static_cast<int>(begin), static_cast<int>(characters), line.kind});
      difference_offsets_.push_back(static_cast<int>(begin));
    }
  }
  buffer->set_text(rendered.empty() ? "Nenhuma diferença textual neste filtro."
                                    : rendered);
  for (const auto &r : ranges)
    buffer->apply_tag(
        r.kind == application::DifferenceKind::Removed ? removed : added,
        buffer->get_iter_at_offset(r.a), buffer->get_iter_at_offset(r.b));
  comparison_previous_.set_sensitive(!difference_offsets_.empty());
  comparison_next_.set_sensitive(!difference_offsets_.empty());
}
} // namespace inde::ui
