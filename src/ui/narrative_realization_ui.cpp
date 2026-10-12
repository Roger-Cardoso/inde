#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "narrative_realization_labels.hpp"
#include <algorithm>
#include <memory>
namespace inde::ui {
namespace {
Gtk::Label *text(const std::string &value) {
  auto *l = Gtk::make_managed<Gtk::Label>(value);
  l->set_wrap(true);
  l->set_xalign(0);
  l->set_max_width_chars(72);
  return l;
}
Gtk::Button *action(Gtk::Box &box, const std::string &label,
                    std::function<void()> f) {
  auto *b = Gtk::make_managed<Gtk::Button>(label);
  b->set_halign(Gtk::Align::START);
  b->signal_clicked().connect(std::move(f));
  box.append(*b);
  return b;
}
std::string status(const std::string &value) {
  if (value == "missing")
    return "alvo removido; substituir ou desvincular";
  if (value == "empty")
    return "trecho vazio; escolher novo intervalo";
  if (value == "unresolved")
    return "trecho não localizado com segurança; localize-o novamente no texto";
  if (value == "changed")
    return "texto mudou desde a última conferência";
  if (value == "hidden")
    return "fora da leitura atual da montagem";
  if (value == "reference-only")
    return "referência sem incorporação textual";
  return "alvo disponível";
}
} // namespace
void NarrativeStructureWorkspace::choose_text_target(
    const std::string &replace) try {
  if (!selected_unit_id_ || !owner_window())
    return;
  const auto expected = service_.builder().snapshot(*selected_structure_id_);
  const auto unit = *selected_unit_id_;
  auto *dialog =
      new OverlayDialog(replace.empty() ? "Associar trecho ou uso de bloco"
                                        : "Substituir texto associado",
                        *owner_window(), true);
  dialog->set_secondary_text(
      "Escolha um alvo com identidade, sem copiar prosa. Para criar um novo trecho marcado, selecione o texto em Escrever e use o menu de botão direito. Substituir/retirar preserva os textos. Pesquisas exibem até 50 resultados; refine para encontrar outros. Usos de blocos usam a versão em uso na montagem, não rascunhos ainda fora da leitura.");
  auto *mode = Gtk::make_managed<Gtk::ComboBoxText>();
  mode->append("range", "Trecho de Documento");
  mode->append("occurrence", "Uso de bloco na montagem");
  mode->set_active_id("range");
  dialog->get_content_area()->append(*mode);
  auto *pages = Gtk::make_managed<Gtk::Stack>();
  auto *ranges = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8),
       *occurrences =
           Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  pages->set_hhomogeneous(false);
  pages->set_vhomogeneous(false);
  pages->add(*ranges, "range");
  pages->add(*occurrences, "occurrence");
  dialog->get_content_area()->append(*pages);
  auto source = std::make_shared<std::optional<project::Document>>();
  auto composition =
      std::make_shared<std::optional<project::StoredComposition>>();
  auto *documents = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar Documento",
      [this](const std::string &query, std::size_t limit) {
        persistence::DocumentQuery q;
        q.search = query;
        q.limit = limit;
        std::vector<IncrementalSelection> out;
        for (auto &v : service_.writing().document_summaries(q))
          out.push_back({v.id, v.title, "Escolha depois um trecho marcado"});
        return out;
      });
  ranges->append(*documents);
  auto *anchors = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar trecho marcado neste Documento",
      [source](const std::string &query, std::size_t limit) {
        std::vector<IncrementalSelection> out;
        if (!*source)
          return out;
        const auto key = project::alias_key(query);
        for (const auto &a : (**source).anchors)
          if (a.start_offset < a.end_offset &&
              project::alias_key(a.label).find(key) != std::string::npos) {
            out.push_back({a.id, a.label,
                           "Intervalo Unicode " +
                               std::to_string(a.start_offset + 1) + "–" +
                               std::to_string(a.end_offset)});
            if (out.size() >= limit)
              break;
          }
        return out;
      });
  ranges->append(*anchors);
  auto *preview = Gtk::make_managed<Gtk::Label>();
  preview->set_xalign(0);
  preview->set_wrap(true);
  preview->set_max_width_chars(64);
  ranges->append(*preview);
  documents->signal_selection_changed().connect(
      [this, dialog, documents, anchors, source, preview] {
        source->reset();
        anchors->clear_selection();
        preview->set_text("");
        try {
          if (documents->selected_id())
            *source = service_.writing().document(*documents->selected_id());
          anchors->refresh();
        } catch (const std::exception &e) {
          dialog->set_secondary_text(author::diagnostic(e.what()));
        }
      });
  anchors->signal_selection_changed().connect([source, anchors, preview] {
    preview->set_text("");
    if (!*source || !anchors->selected_id())
      return;
    for (const auto &a : (**source).anchors)
      if (a.id == *anchors->selected_id()) {
        const auto *first = g_utf8_offset_to_pointer((**source).content.c_str(),
                                                     a.start_offset),
                   *last = g_utf8_offset_to_pointer(
                       (**source).content.c_str(),
                       std::min(a.end_offset, a.start_offset + 240));
        preview->set_text("Prévia limitada a 240 caracteres: " +
                          std::string(first, last));
        break;
      }
  });
  auto *compositions = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar montagem de texto",
      [this](const std::string &query, std::size_t limit) {
        std::vector<IncrementalSelection> out;
        for (const auto &v : service_.compositions().search(query, limit))
          out.push_back(
              {v.id, v.name, "Revisão " + std::to_string(v.revision)});
        return out;
      });
  occurrences->append(*compositions);
  auto *placements = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar uso do bloco na montagem",
      [composition](const std::string &query, std::size_t limit) {
        std::vector<IncrementalSelection> out;
        if (!*composition)
          return out;
        const auto key = project::alias_key(query);
        const auto &c = (**composition).value;
        for (const auto &p : c.blocks.placements())
          if (p.mode != experimental::PlacementMode::ReferenceOnly) {
            const auto &block = c.blocks.block(p.block_id);
            if (project::alias_key(block.label).find(key) == std::string::npos)
              continue;
            out.push_back({p.id,
                           block.label + " · ocorrência " + p.id.substr(0, 8),
                           std::string(c.blocks.visible(p, c.profile)
                                           ? "na leitura"
                                           : "fora da leitura") +
                               " · " + experimental::name(p.mode)});
            if (out.size() >= limit)
              break;
          }
        return out;
      });
  occurrences->append(*placements);
  compositions->signal_selection_changed().connect(
      [this, dialog, compositions, placements, composition] {
        composition->reset();
        placements->clear_selection();
        try {
          if (compositions->selected_id())
            *composition =
                service_.compositions().load(*compositions->selected_id());
          placements->refresh();
        } catch (const std::exception &e) {
          dialog->set_secondary_text(author::diagnostic(e.what()));
        }
      });
  mode->signal_changed().connect(
      [mode, pages] { pages->set_visible_child(mode->get_active_id()); });
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button(replace.empty() ? "Vincular alvo" : "Substituir alvo",
                     Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect([this, dialog, expected, unit, replace,
                                     mode, source, composition, anchors,
                                     placements](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      if (mode->get_active_id() == "range") {
        if (!*source || !anchors->selected_id())
          throw std::runtime_error("Escolha Documento e trecho marcado");
        service_.builder().attach_anchor(expected, unit, **source,
                                         *anchors->selected_id(), replace);
      } else {
        if (!*composition || !placements->selected_id())
          throw std::runtime_error("Escolha uma montagem de texto e um uso do bloco");
        service_.builder().attach_occurrence(
            expected, unit, **composition, *placements->selected_id(), replace);
      }
      refresh_detail();
    } catch (const std::exception &e) {
      dialog->set_secondary_text(author::diagnostic(e.what()));
      dialog->keep_open_on_response();
    }
  });
  dialog->present();
} catch (const std::exception &e) {
  show_error(e);
}
void NarrativeStructureWorkspace::realization_actions(
    const application::BuilderRealization &r) {
  if (!owner_window())
    return;
  const auto expected = builder_snapshot_;
  std::string comparison;
  std::string source_label = r.document_id.empty() ? "Uso de bloco na montagem"
                             : r.anchor_id.empty() ? "Documento inteiro"
                                                   : "Trecho de Documento";
  for (const auto &u : expected.units)
    if (u.id == r.unit_id)
      comparison = "Parte: " + u.title + "\nResumo planejado: " + u.summary +
                   "\nIntenção declarada: " + u.purpose;
  try {
    std::string fragment;
    if (!r.document_id.empty()) {
      if (auto d = service_.writing().document(r.document_id)) {
        std::size_t begin = 0, end = project::utf8_character_count(d->content);
        if (!r.anchor_id.empty()) {
          auto at = std::find_if(d->anchors.begin(), d->anchors.end(),
                                 [&](auto &v) { return v.id == r.anchor_id; });
          if (at == d->anchors.end())
            throw std::runtime_error("Trecho marcado ausente");
          begin = at->start_offset;
          end = at->end_offset;
          source_label = "Trecho: " + at->label;
        }
        const auto *first = g_utf8_offset_to_pointer(d->content.c_str(), begin),
                   *last = g_utf8_offset_to_pointer(d->content.c_str(),
                                                    std::min(end, begin + 400));
        fragment = std::string(first, last);
      } else
        throw std::runtime_error("Documento ausente");
    } else {
      const auto c = service_.compositions().load(r.composition_id);
      const auto &value =
          c.value.blocks.revision(c.value.blocks.occurrence(r.occurrence_id))
              .text;
      const auto count = project::utf8_character_count(value);
      fragment = value.substr(
          0, static_cast<std::size_t>(
                 g_utf8_offset_to_pointer(value.c_str(),
                                          std::min<std::size_t>(400, count)) -
                 value.c_str()));
    }
    comparison +=
        "\n\nPrévia da fonte (até 400 caracteres):\n" + fragment +
        "\n\nPrévia não interpreta sentido nem substitui a leitura completa.";
  } catch (const std::exception &e) {
    comparison += "\nPrévia indisponível: " + std::string(author::diagnostic(e.what()));
  }
  auto *dialog =
      new OverlayDialog("Conferir texto que desenvolve esta parte", *owner_window(), true);
  dialog->set_secondary_text(
      status(r.status) +
      ". A associação não certifica significado. Editar a fonte compartilhada "
      "afeta suas outras realizações; separar muda apenas este vínculo, sem "
      "sincronização posterior.");
  dialog->get_content_area()->append(
      *text("Fonte: " + r.title + "\n" + source_label));
  auto *identity =
      Gtk::make_managed<Gtk::Expander>("Identificação técnica da fonte");
  std::string identifiers;
  for (const auto &[label, value] :
       std::initializer_list<std::pair<std::string, std::string>>{
           {"Documento", r.document_id},
           {"Trecho marcado", r.anchor_id},
           {"Montagem de texto", r.composition_id},
           {"Uso do bloco", r.occurrence_id}})
    if (!value.empty())
      identifiers += label + ": " + value + "\n";
  identity->set_child(*text(identifiers));
  dialog->get_content_area()->append(*identity);
  auto *comparison_section =
      Gtk::make_managed<Gtk::Expander>("Intenção e prévia da fonte");
  comparison_section->set_child(*text(comparison));
  dialog->get_content_area()->append(*comparison_section);
  auto *note = Gtk::make_managed<Gtk::Entry>();
  note->set_text(r.note);
  note->set_max_length(512);
  if (r.table == "narrative_text_targets")
    append_labeled_form_field(*dialog->get_content_area(),
                              "Nota da conferência", *note);
  else {
    dialog->get_content_area()->append(*note);
    note->set_visible(false);
  }
  auto *divergent = Gtk::make_managed<Gtk::CheckButton>(
      "O texto difere do plano por escolha minha");
  divergent->set_active(r.review_state == "diverged");
  if (r.table == "narrative_text_targets")
    dialog->get_content_area()->append(*divergent);
  else {
    dialog->get_content_area()->append(*divergent);
    divergent->set_visible(false);
    dialog->get_content_area()->append(
        *text("Esta é uma associação legada ao Documento inteiro, sem "
              "impressão de trecho conferido. Para conferência rastreável, "
              "associe uma seleção ou ocorrência. Separar o Documento continua "
              "possível, mediante confirmação."));
  }
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_text("Independente — " + r.title);
  title->set_max_length(128);
  auto *separation =
      Gtk::make_managed<Gtk::Expander>("Opções de texto independente");
  auto *separation_body =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  separation->set_child(*separation_body);
  dialog->get_content_area()->append(*separation);
  append_labeled_form_field(*separation_body,
                            "Nome para eventual cópia independente", *title);
  auto *agree = Gtk::make_managed<Gtk::CheckButton>(
      r.document_id.empty()
          ? "Se separar: criar outro uso com uma cópia independente do bloco, "
            "inicialmente fora da leitura; preservar a ocorrência atual e "
            "mudar apenas este vínculo"
          : "Se separar: copiar o Documento inteiro e suas camadas para outro documento independente; sem posição no manuscrito; mudar apenas este vínculo");
  if (auto *l = dynamic_cast<Gtk::Label *>(agree->get_child())) {
    l->set_wrap(true);
    l->set_max_width_chars(66);
  }
  separation_body->append(*agree);
  if (!r.provenance.empty()) {
    auto *e = Gtk::make_managed<Gtk::Expander>("Origem e histórico da separação");
    e->set_child(*text(r.provenance));
    dialog->get_content_area()->append(*e);
  }
  dialog->add_button("Fechar", Gtk::ResponseType::CANCEL);
  if (r.table == "narrative_text_targets" && r.status != "missing" &&
      r.status != "unresolved" && r.status != "empty")
    dialog->add_button("Registrar conferência", Gtk::ResponseType::ACCEPT);
  if (r.status != "missing" && r.status != "unresolved" && r.status != "empty")
    dialog->add_button("Criar texto independente", 1);
  dialog->signal_response().connect([this, dialog, expected, r, note, divergent,
                                     title, agree, separation](int response) {
    if (response != Gtk::ResponseType::ACCEPT && response != 1)
      return;
    try {
      if (response == 1) {
        if (!agree->get_active()) {
          separation->set_expanded(true);
          throw std::runtime_error("Confirme as consequências da separação "
                                   "antes de criar outra prosa");
        }
        service_.builder().make_independent(expected, r.id,
                                            title->get_text().raw());
      } else
        service_.builder().review_realization(
            expected, r.id, note->get_text().raw(), divergent->get_active());
      refresh_detail();
    } catch (const std::exception &e) {
      dialog->set_secondary_text(author::diagnostic(e.what()));
      dialog->keep_open_on_response();
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::show_continuity() {
  if (!selected_structure_id_)
    return;
  try {
    if (stack_.get_visible_child_name() != "continuity")
      continuity_origin_ = stack_.get_visible_child_name().raw();
    builder_snapshot_ = service_.builder().snapshot(*selected_structure_id_);
    continuity_notices_ = service_.builder().continuity(builder_snapshot_);
    continuity_page_index_ = 0;
    render_continuity();
    stack_.set_visible_child("continuity");
    continuity_back_.grab_focus();
  } catch (const std::exception &e) {
    show_error(e);
  }
}
void NarrativeStructureWorkspace::render_continuity() {
  while (auto *c = continuity_body_.get_first_child())
    continuity_body_.remove(*c);
  std::vector<application::BuilderNotice> filtered;
  const auto filter = continuity_filter_.get_active_id().raw();
  for (const auto &n : continuity_notices_)
    if (filter == "all" || filter == n.kind ||
        (filter == "reconciliation" &&
         (n.kind == "review" || n.kind == "author-declared")) ||
        (filter == "presence" && n.kind == "chronology") ||
        (filter == "dependencies" && (n.kind == "order" || n.kind == "cycle")))
      filtered.push_back(n);
  constexpr std::size_t page_size = 30;
  const auto pages =
      std::max<std::size_t>(1, (filtered.size() + page_size - 1) / page_size);
  continuity_page_index_ = std::min(continuity_page_index_, pages - 1);
  continuity_title_.set_text(
      "Continuidade · " + std::to_string(filtered.size()) + " avisos · " +
      std::to_string(continuity_page_index_ + 1) + "/" + std::to_string(pages));
  continuity_body_.append(
      *text("Consulta de declarações, não julgamento literário. Avisos não "
            "bloqueiam a escrita. Informação insuficiente não é contradição; "
            "janelas sobrepostas não provam simultaneidade. As quatro ordens "
            "permanecem independentes."));
  if (filtered.empty())
    continuity_body_.append(
        *text("Nenhum aviso neste filtro e nas dimensões verificadas. Isso não "
              "certifica continuidade, qualidade ou ausência de problemas não "
              "declarados."));
  const auto end =
      std::min(filtered.size(), (continuity_page_index_ + 1) * page_size);
  for (auto i = continuity_page_index_ * page_size; i < end; ++i) {
    const auto n = filtered[i];
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    box->set_margin(8);
    // Notice keys/evidence and exception fingerprints stay canonical. These
    // captions are generated diagnostics, never text chosen by the author.
    static const std::map<std::string, std::string> titles{
      {"Trecho/ocorrência precisa de reconciliação", "Confira a associação do trecho ou uso de bloco ao plano"},
      {"Ocorrência não integra a leitura atual", "Este uso do bloco não entra na leitura atual"},
      {"Divergência declarada pelo autor", "Diferença intencional entre texto e plano"},
      {"Dependência inclui unidade fora da fila", "A regra inclui uma parte fora da ordem de escrita"},
      {"Cronologia sem contexto de ocorrência único", "Declare quando esta parte acontece para comparar sua cronologia"},
      {"Datas não comparáveis ou sem limites suficientes", "Ainda não é possível comparar estas datas"},
      {"Presença canônica exige correlação explícita entre data e eixo", "Indique como a data corresponde à linha do tempo da presença registrada"},
      {"Janela planejada não permite avaliar presença canônica", "O período planejado não permite comparar a presença registrada"},
      {"Conferir Local planejado frente a presença canônica", "Compare o lugar planejado com a presença registrada na história"},
      {"Precedência editorial não demonstrada pela colocação textual", "A posição do texto não comprova esta regra na ordem do manuscrito"}
    };
    const auto translated=titles.find(n.title);
    box->append(*text((translated==titles.end()?n.title:translated->second) + " · " + continuity_kind_label(n.kind)));
    auto unit_name = [&](const std::string &id) {
      for (const auto &u : builder_snapshot_.units)
        if (u.id == id)
          return u.title;
      return std::string("Parte indisponível");
    };
    if (!n.unit_id.empty()) {
      auto context = unit_name(n.unit_id);
      if (!n.other_unit_id.empty() && n.other_unit_id != n.unit_id)
        context += " · " + unit_name(n.other_unit_id);
      box->append(*text("Partes: " + context));
    }
    auto *e = Gtk::make_managed<Gtk::Expander>("Evidência do aviso");
    e->set_child(*text(n.evidence));
    box->append(*e);
    if (!n.exception_note.empty())
      box->append(*text("Escolha intencional do autor: " + n.exception_note));
    auto *actions =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto open = [this, actions](const std::string &label,
                                const std::string &unit) {
      if (!unit.empty())
        action(*actions, label, [this, unit] {
          selected_unit_id_ = unit;
          show_unit();
        });
    };
    open("Abrir parte", n.unit_id);
    if (n.other_unit_id != n.unit_id)
      open("Abrir outra parte", n.other_unit_id);
    if (n.kind == "order" || n.kind == "cycle" || n.kind == "chronology" ||
        n.kind == "presence" || n.kind == "author-declared")
      action(*actions,
             n.exception_note.empty() ? "Registrar escolha intencional…"
                                      : "Rever escolha intencional…",
             [this, n] { justify_notice(n); });
    if (n.key.starts_with("stale-exception:"))
      action(*actions, "Retirar escolha intencional antiga", [this, n] {
        try {
          auto original = n;
          original.key = n.key.substr(std::string("stale-exception:").size());
          service_.builder().set_exception(builder_snapshot_, original, "");
          show_continuity();
        } catch (const std::exception &e) {
          show_error(e);
        }
      });
    box->append(*actions);
    continuity_body_.append(*box);
  }
  action(continuity_body_, "Atualizar consulta", [this] { show_continuity(); });
  continuity_previous_.set_sensitive(continuity_page_index_ > 0);
  continuity_next_.set_sensitive(continuity_page_index_ + 1 < pages);
  continuity_scroll_.get_vadjustment()->set_value(0);
}
void NarrativeStructureWorkspace::justify_notice(application::BuilderNotice n) {
  if (!owner_window())
    return;
  const auto observed = builder_snapshot_;
  auto *d = new OverlayDialog("Escolha intencional sobre a continuidade",
                              *owner_window(), true);
  d->set_secondary_text(
      "Justifique esta evidência específica. Viagem no tempo, narração não "
      "linear e outros contextos são legítimos. A exceção não transforma dados "
      "nem será reaplicada se a evidência mudar. Apagar a nota retira a "
      "exceção.");
  auto *note = Gtk::make_managed<Gtk::Entry>();
  note->set_text(n.exception_note);
  note->set_max_length(512);
  append_labeled_form_field(*d->get_content_area(), "Justificativa", *note);
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button("Registrar decisão", Gtk::ResponseType::ACCEPT);
  d->signal_response().connect([this, d, observed, n, note](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      service_.builder().set_exception(observed, n, note->get_text().raw());
      show_continuity();
    } catch (const std::exception &e) {
      d->set_secondary_text(author::diagnostic(e.what()));
      d->keep_open_on_response();
    }
  });
  d->present();
}
} // namespace inde::ui
