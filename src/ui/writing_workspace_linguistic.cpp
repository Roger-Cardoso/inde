#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/writing_workspace.hpp"

#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "writing_workspace_util.hpp"

namespace inde::ui {
namespace {
void bound_search(Gtk::SearchEntry &entry) {
  // This gtkmm exposes SearchEntry as Entry, while GTK4's C SearchEntry is
  // a separate editable widget. Entry::set_max_length triggers a critical.
  auto *delegate = gtk_editable_get_delegate(GTK_EDITABLE(entry.gobj()));
  if (!delegate || !GTK_IS_TEXT(delegate))
    throw std::runtime_error("Campo de consulta sem editor de texto compatível");
  gtk_text_set_max_length(GTK_TEXT(delegate), 256);
}
}

void WritingWorkspace::show_semantic_dictionary() {
  auto *window = owner_window();
  if (!window)
    return;
  auto *dialog =
      new OverlayDialog("Dicionário", *window, true);
  dialog->set_secondary_text(
      "Compare os sentidos da palavra e suas fontes. A consulta não altera o texto nem escolhe um sentido por você. A base experimental Onto.PT é isolada da base revisada e serve somente para avaliação.");
  dialog->add_button("Fechar", Gtk::ResponseType::CLOSE);
  auto *body = dialog->get_content_area();
  body->set_margin(16);
  const auto availability = service_.linguistics().status();

  auto *query = Gtk::make_managed<Gtk::SearchEntry>();
  query->set_placeholder_text("Palavra ou expressão exata");
  bound_search(*query);
  const auto buffer = editor_.get_buffer();
  Gtk::TextBuffer::iterator begin;
  Gtk::TextBuffer::iterator end;
  if (buffer->get_selection_bounds(begin, end)) {
    auto selected = buffer->get_text(begin, end, true).raw();
    if (selected.size() <= 256 && selected.find('\n') == std::string::npos)
      query->set_text(selected);
  }
  auto *consult = Gtk::make_managed<Gtk::Button>("Consultar alternativas");
  consult->add_css_class("suggested-action");
  auto *controls =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  query->set_hexpand(true);
  controls->append(*query);
  controls->append(*consult);
  body->append(*controls);

  auto *quarantine = Gtk::make_managed<Gtk::CheckButton>(
      "Consultar somente a base experimental Onto.PT (isolada)");
  if (availability.quarantine_contract_version == "2.2.0") {
    quarantine->set_tooltip_text(
        "Quarentena v2.2: definições compostas foram decompostas por synset. "
        "Somente vínculos mensuráveis entram na consulta; fragmentos sem "
        "vínculo e hífens incertos permanecem pendentes." +
        std::string{availability.definition_evidence_review_available
                        ? " O overlay v2.4 (ou v2.3 legado) mostra evidências, "
                          "átomos autorizados, classes e gates de QA."
                        : ""});
  } else {
    quarantine->set_tooltip_text(
        "Registros avaliados pela rubrica editorial. A v1.2 distingue as 32 "
        "definições aprovadas das 8 abstenções deferidas e mantém os "
        "originais.");
  }
  body->append(*quarantine);

  auto *status = Gtk::make_managed<Gtk::Label>();
  prepare_tool_label(*status);
  body->append(*status);
  auto *results = Gtk::make_managed<Gtk::ListBox>();
  results->set_selection_mode(Gtk::SelectionMode::NONE);
  results->add_css_class("boxed-list");
  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll->set_min_content_height(320);
  scroll->set_child(*results);
  body->append(*scroll);

  const auto refresh =
      [this, query, quarantine, status, results, availability] {
    clear_list(*results);
    try {
      const bool isolated = quarantine->get_active();
      const auto response =
          isolated
              ? service_.linguistics().query_quarantine(
                    query->get_text().raw(), 50)
              : service_.linguistics().query_lexicon(
                    query->get_text().raw(), 50);
      status->set_text(
          response.total == 0
              ? isolated
                    ? "BASE EXPERIMENTAL — EM AVALIAÇÃO — nenhum sentido da palavra foi encontrado nesta leva seletiva."
                    : "Nenhum sentido da palavra foi encontrado na base local revisada."
              : (isolated ? "BASE EXPERIMENTAL — EM AVALIAÇÃO — " : "BASE REVISADA — ") +
                    std::string{"exibindo "} +
                    std::to_string(response.candidates.size()) +
                    " de " + std::to_string(response.total) +
                    " alternativas. Sem vencedor automático; critério de ordenação: " +
                    response.score_semantics +
                    (isolated
                         ? availability.quarantine_contract_version == "2.2.0"
                               ? ". Os sentidos da palavra separados permanecem pendentes de confirmação humana; fragmentos sem vínculo não são exibidos como sentidos da palavra." +
                                     std::string{
                                         availability
                                                 .definition_evidence_review_available
                                             ? " O overlay anexa evidência isolada e QA por sentido da palavra."
                                             : ""}
                               : ". Há 32 definições editoriais aceitas; "
                                 "abstenções, pontuação e demais registros não "
                                 "equivalem a aceite linguístico."
                         : availability.definition_promotion_available
                               ? ". O lote v2.5 acrescenta " +
                                     std::to_string(
                                         availability.definition_promoted_senses) +
                                     " sentidos da palavra aprovados na revisão do catálogo sem associação automática."
                               : "."));
      for (const auto &candidate : response.candidates) {
        auto *row =
            Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
        row->set_margin(10);
        std::string title_text = candidate.written + " — " + candidate.ptbr_pos;
        if (candidate.definition_promoted)
          title_text += " — Aprovada na revisão do catálogo";
        else if (candidate.editorial_status ==
            "policy_approved_decomposition_v22")
          title_text +=
              " — Sentido separado; revisão humana pendente";
        else if (candidate.definition_is_editorial &&
                 candidate.editorial_review_status == "accepted")
          title_text += " — Definição aprovada na revisão do catálogo";
        else if (candidate.definition_is_editorial)
          title_text += " — Rascunho de definição; não aprovado";
        else if (candidate.editorial_status.rfind("abstained_", 0) == 0)
          title_text += " — Definição da fonte com ressalvas";
        else if (candidate.review_status == "needs_review")
          title_text += " — Revisão da fonte pendente";
        else
          title_text += " — Definição da fonte; confira sua revisão nos detalhes";
        if (candidate.evidence_review_available) {
          if (candidate.definition_qa_gate == "blocked_family_alignment")
            title_text += " — Revisão do conjunto de sentidos pendente";
          else if (candidate.definition_qa_gate ==
                   "blocked_atomic_evidence_review")
            title_text += " — Revisão das evidências pendente";
          else if (candidate.definition_qa_gate ==
              "blocked_return_to_sense_alignment")
            title_text += " — Correspondência entre sentidos pendente";
          else if (candidate.definition_qa_gate ==
                   "blocked_incomplete_evidence_map")
            title_text += " — Evidência incompleta";
          else if (candidate.definition_qa_gate ==
                   "human_cross_definition_review")
            title_text += " — Revisão cruzada pendente";
        }
        auto *title = Gtk::make_managed<Gtk::Label>(title_text);
        title->set_halign(Gtk::Align::START);
        title->set_xalign(0.0F);
        title->add_css_class("heading");
        title->set_wrap(true);
        row->append(*title);
        auto *definition = Gtk::make_managed<Gtk::Label>(
            candidate.definition.value_or(
                "Definição não fornecida pela fonte para este sentido da palavra."));
        definition->set_halign(Gtk::Align::START);
        definition->set_xalign(0.0F);
        definition->set_wrap(true);
        row->append(*definition);
        auto *source = Gtk::make_managed<Gtk::Label>(
            "Classe na fonte: " + candidate.source_pos +
            (candidate.catalog_scope == "quarantine" ? " · Base experimental, isolada" : " · Base revisada") +
            " · Nenhuma associação automática ao texto.");
        prepare_tool_label(*source);
        row->append(*source);
        auto *details = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
        auto *detail_toggle = Gtk::make_managed<Gtk::Expander>("Fontes e detalhes da revisão");
        detail_toggle->set_child(*details);
        row->append(*detail_toggle);
        if (candidate.definition_promoted) {
          auto *promotion = Gtk::make_managed<Gtk::Label>(
              "Promoção controlada: qualidade " +
              std::to_string(candidate.definition_promotion_quality_score) +
              "/100 · registros de origem: " +
              std::to_string(
                  candidate.definition_promotion_provenance_count) +
              " · colisão: " +
              candidate.definition_promotion_collision_status +
              ". Nenhum vínculo semântico automático foi criado.");
          prepare_tool_label(*promotion);
          promotion->set_selectable(true);
          details->append(*promotion);
        }
        if (candidate.definition_is_editorial &&
            candidate.original_definition.has_value()) {
          auto *original = Gtk::make_managed<Gtk::Label>(
              "Original da fonte: " + *candidate.original_definition);
          prepare_tool_label(*original);
          original->set_selectable(true);
          details->append(*original);
        }
        if (candidate.evidence_review_available) {
          if (candidate.upos_variant_count > 1) {
            auto *classes = Gtk::make_managed<Gtk::Label>(
                "Mesma grafia em " +
                std::to_string(candidate.upos_variant_count) +
                " classes separadas: " + candidate.upos_variants +
                ". Esta linha pertence somente a " + candidate.upos + ".");
            prepare_tool_label(*classes);
            classes->set_selectable(true);
            details->append(*classes);
          }
          auto *evidence = Gtk::make_managed<Gtk::Label>(
              "Evidências autorizadas para este sentido da palavra (" +
              std::to_string(candidate.authorized_evidence_count) + "):\n" +
              candidate.authorized_evidence_summary);
          prepare_tool_label(*evidence);
          evidence->set_selectable(true);
          details->append(*evidence);
          auto *evidence_map = Gtk::make_managed<Gtk::Label>(
              "Mapa definição → evidência:\n" +
              candidate.definition_evidence_map);
          prepare_tool_label(*evidence_map);
          evidence_map->set_selectable(true);
          details->append(*evidence_map);
          if (!candidate.atomic_evidence_summary.empty()) {
            auto *atoms = Gtk::make_managed<Gtk::Label>(
                "Unidades atômicas candidatas (" +
                std::to_string(candidate.candidate_evidence_atom_count) +
                "; autorizadas: " +
                std::to_string(candidate.authorized_evidence_atom_count) +
                "):\n" + candidate.atomic_evidence_summary);
            prepare_tool_label(*atoms);
            atoms->set_selectable(true);
            details->append(*atoms);
          }
          if (!candidate.alignment_family_id.empty()) {
            auto *family = Gtk::make_managed<Gtk::Label>(
                "Família simétrica de alinhamento: " +
                candidate.alignment_family_id +
                ". Todos os membros permanecem bloqueados até decisão humana.");
            prepare_tool_label(*family);
            family->set_selectable(true);
            details->append(*family);
          }
          if (!candidate.definition_qa_issue_codes.empty()) {
            auto *qa = Gtk::make_managed<Gtk::Label>(
                "QA v" + candidate.evidence_review_contract_version + ": " +
                candidate.definition_qa_issue_codes +
                " · gate: " + candidate.definition_qa_gate +
                " · bloqueios: " +
                std::to_string(candidate.definition_qa_blocking_issues) +
                " · revisões: " +
                std::to_string(candidate.definition_qa_review_issues));
            prepare_tool_label(*qa);
            qa->set_selectable(true);
            details->append(*qa);
          }
        }
        if (!candidate.editorial_rationale.empty() ||
            !candidate.fallback_motive.empty()) {
          const auto &motive = candidate.fallback_motive.empty()
                                   ? candidate.editorial_rationale
                                   : candidate.fallback_motive;
          auto *rationale = Gtk::make_managed<Gtk::Label>(
              (candidate.editorial_status.rfind("abstained_", 0) == 0
                   ? "Motivo: "
                   : candidate.editorial_status ==
                             "policy_approved_decomposition_v22"
                         ? "Critério da decomposição: "
                         : "Motivo da revisão: ") +
              motive);
          prepare_tool_label(*rationale);
          rationale->set_selectable(true);
          details->append(*rationale);
        }
        std::string metadata_text =
            candidate.sense_id + " · Classe gramatical da fonte: " + candidate.source_pos + " · UPOS: " + candidate.upos +
            " · revisão: " + candidate.review_status;
        if (candidate.definition_promoted)
          metadata_text +=
              " · promoção: " + candidate.definition_promotion_id +
              " · base: " + candidate.definition_promotion_basis +
              " · autorização: " +
              candidate.definition_promotion_authorization_id;
        if (candidate.catalog_scope == "quarantine") {
          metadata_text += " · registro: " + candidate.source_record_id +
                           " · grupo de sinônimos deste sentido: " + candidate.source_synset_id +
                           " · OpenWordNet-PT: " +
                           (candidate.reference_overlap ? "sobreposição"
                                                        : "sem sobreposição") +
                           " · seleção: " + candidate.selection_criteria +
                           " · qualidade: " +
                           candidate.definition_quality_gate + " " +
                           std::to_string(candidate.definition_quality_score) +
                           "/12 · flags: " +
                           candidate.definition_quality_flags +
                           (candidate.editorial_status.empty()
                                ? ""
                                : " · editorial: " +
                                      candidate.editorial_status) +
                           (candidate.editorial_review_status.empty()
                                ? ""
                                : " · decisão editorial: " +
                                      candidate.editorial_review_status) +
                           (candidate.relation_validation_status.empty()
                                ? ""
                                : " · relações: " +
                                      candidate.relation_validation_status) +
                           (candidate.fallback_mode.empty()
                                ? ""
                                : " · fallback: original sinalizado") +
                           (candidate.evidence_review_available
                                ? " · cluster: " +
                                      candidate.sense_cluster_id +
                                      " · congelamento: " +
                                      candidate.cluster_freeze_status +
                                      " · geração: " +
                                      candidate.generation_status
                                : "");
        }
        auto *metadata =
            Gtk::make_managed<Gtk::Label>(metadata_text);
        prepare_tool_label(*metadata);
        metadata->set_selectable(true);
        details->append(*metadata);
        results->append(*row);
      }
    } catch (const std::exception &error) {
      status->set_text(std::string{"Consulta indisponível: "} + author::diagnostic(error.what()) +
                       (quarantine->get_active()
                            ? ". Configure INDE_QUARANTINE_CATALOG para a base experimental Onto.PT v2.2 (ou v1.2 legada) e INDE_DEFINITION_EVIDENCE_REVIEW para o overlay v2.4 (ou v2.3 legado)."
                            : ". Configure INDE_L06_CATALOG para a base revisada."));
    }
  };
  consult->signal_clicked().connect(refresh);
  query->signal_activate().connect(refresh);
  status->set_text(availability.lexical_message);
  quarantine->signal_toggled().connect(
      [quarantine, status, results, availability] {
        clear_list(*results);
        status->set_text(quarantine->get_active()
                             ? availability.quarantine_message
                             : availability.lexical_message);
      });
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->present();
  if (!query->get_text().empty())
    refresh(); // An explicit context-menu choice already requested the lookup.
}

void WritingWorkspace::show_linguistic_entities() {
  auto *window = owner_window();
  if (!window)
    return;
  auto *dialog = new OverlayDialog("Construções e cenários de significado", *window, true);
  dialog->set_secondary_text(
      "Catálogo de construções de significado, somente para consulta. São exemplos sintéticos aceitos para uso interno; a lista não afirma que algum padrão ocorre no documento aberto.");
  dialog->add_button("Fechar", Gtk::ResponseType::CLOSE);
  auto *body = dialog->get_content_area();
  body->set_margin(16);
  auto *search = Gtk::make_managed<Gtk::SearchEntry>();
  search->set_placeholder_text("Filtrar por nome");
  bound_search(*search);
  auto *refresh_button = Gtk::make_managed<Gtk::Button>("Consultar catálogo");
  refresh_button->add_css_class("suggested-action");
  auto *controls =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  search->set_hexpand(true);
  controls->append(*search);
  controls->append(*refresh_button);
  body->append(*controls);
  auto *status = Gtk::make_managed<Gtk::Label>();
  prepare_tool_label(*status);
  body->append(*status);
  auto *results = Gtk::make_managed<Gtk::ListBox>();
  results->set_selection_mode(Gtk::SelectionMode::NONE);
  results->add_css_class("boxed-list");
  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll->set_min_content_height(340);
  scroll->set_child(*results);
  body->append(*scroll);

  const auto refresh = [this, search, status, results] {
    clear_list(*results);
    try {
      const auto entities = service_.linguistics().linguistic_entities(
          search->get_text().raw(), 50);
      status->set_text(
          entities.empty()
              ? "Nenhum elemento corresponde ao filtro no catálogo atual."
              : std::to_string(entities.size()) +
                    " elementos da história exibidos. Nenhuma correspondência no texto foi procurada.");
      for (const auto &entity : entities) {
        auto *row =
            Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
        row->set_margin(10);
        auto *title = Gtk::make_managed<Gtk::Label>(
            project::display_name(entity.kind) + ": " + entity.name);
        title->set_halign(Gtk::Align::START);
        title->set_xalign(0.0F);
        title->add_css_class("heading");
        row->append(*title);
        auto *description =
            Gtk::make_managed<Gtk::Label>(entity.description);
        description->set_halign(Gtk::Align::START);
        description->set_xalign(0.0F);
        description->set_wrap(true);
        row->append(*description);
        auto *metadata = Gtk::make_managed<Gtk::Label>(
            entity.id + " · " + entity.subtype + " · " +
            entity.epistemic_status + " · " + entity.review_status);
        prepare_tool_label(*metadata);
        metadata->set_selectable(true);
        row->append(*metadata);
        results->append(*row);
      }
    } catch (const std::exception &error) {
      status->set_text(
          std::string{"Catálogo indisponível: "} + author::diagnostic(error.what()) +
          ". Configure INDE_L08_CATALOG para o catálogo promovido v0.6.");
    }
  };
  refresh_button->signal_clicked().connect(refresh);
  search->signal_activate().connect(refresh);
  const auto availability = service_.linguistics().status();
  status->set_text(availability.constructions_message);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->present();
}

} // namespace inde::ui
