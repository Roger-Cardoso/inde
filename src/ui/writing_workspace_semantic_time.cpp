#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/accessibility.hpp"
#include "writing_semantic_explorer_support.hpp"
#include "inde/ui/temporal_reference_editor.hpp"
#include "temporal_form_helpers.hpp"
#include "inde/project/manifest.hpp"
#include <tuple>
namespace inde::ui {
using namespace semantic_ui;
namespace {
auto point_state(const project::FictionalTimePoint &p) {
  return std::tie(p.id, p.axis_id, p.ordinal, p.label, p.description,
                  p.created_at, p.updated_at);
}
} // namespace
std::string WritingWorkspace::semantic_time_label(
    const project::SemanticTemporalBinding &b) const {
  std::string axis = b.fictional_axis_id;
  for (const auto &a : service_.planning().time_axes())
    if (a.id == b.fictional_axis_id)
      axis = a.name;
  auto point = [&](const std::string &id) {
    auto p = service_.planning().time_point(id);
    return p ? p->label + " (ordem " + std::to_string(p->ordinal) + ")"
             : "Ponto indisponível: " + id;
  };
  return "Linha do tempo: " + axis + " • " +
         (b.fictional_end_point_id ? "Período fechado: " : "Ponto: ") +
         point(b.fictional_start_point_id) +
         (b.fictional_end_point_id ? " → " + point(*b.fictional_end_point_id)
                                   : "");
}
void WritingWorkspace::edit_semantic_time(const std::string &annotation_id) {
  if (!current_document_ || !owner_window())
    return;
  try {
    require_completed_composition();
    auto d = capture_editor_state();
    if (std::none_of(d.semantics.annotations.begin(),
                     d.semantics.annotations.end(),
                     [&](auto &a) { return a.id == annotation_id; }))
      throw std::runtime_error("Anotação ausente");
    const auto axes = service_.planning().time_axes();
    const auto epoch = edit_epoch_, revision = edit_revision_;
    auto *dialog = new OverlayDialog("Vincular anotação ao tempo da história",
                                     *owner_window(), true);
    dialog->set_secondary_text(
        "Escolha explicitamente a linha do tempo e os pontos existentes no Planejamento. "
        "Esta é uma declaração de contexto textual, não uma data do sistema, "
        "acontecimento, presença ou calendário.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Confirmar vínculo temporal", Gtk::ResponseType::ACCEPT);
    auto *box = dialog->get_content_area();
    auto *axis = Gtk::make_managed<Gtk::ComboBoxText>();
    constrain_temporal_combo(*axis);
    axis->append("", "Escolha a linha do tempo ficcional");
    for (const auto &a : axes)
      axis->append(a.id, a.name);
    axis->set_active(0);
    set_accessible_label(*axis, "Linha do tempo do vínculo");
    box->append(*axis);
    auto chosen_axis = std::make_shared<std::string>();
    auto snapshots =
        std::make_shared<std::map<std::string, project::FictionalTimePoint>>();
    auto provider = [this, chosen_axis](const std::string &s,
                                        std::size_t limit) {
      std::vector<IncrementalSelection> out;
      if (chosen_axis->empty())
        return out;
      persistence::PlanningQuery q;
      q.search = s;
      q.limit = limit;
      for (const auto &p : service_.planning().time_points(*chosen_axis, q))
        out.push_back({p.id, p.label,
                       "Ordem ficcional: " + std::to_string(p.ordinal) + " • " +
                           p.description});
      return out;
    };
    auto *start = Gtk::make_managed<IncrementalSelector>(
        "Pesquisar ponto inicial / pontual", provider);
    auto *end = Gtk::make_managed<IncrementalSelector>(
        "Pesquisar ponto final (somente período)", provider);
    auto *period = Gtk::make_managed<Gtk::CheckButton>(
        "Vincular um período fechado, com início e fim");
    box->append(*period);
    box->append(*start);
    box->append(*end);
    end->set_visible(false);
    auto *note = Gtk::make_managed<Gtk::Entry>();
    note->set_placeholder_text("Nota autoral do vínculo temporal (opcional)");
    box->append(*note);
    auto *preview = label(
        *box,
        "Prévia: escolha primeiro a linha do tempo e um ponto. Nada foi atribuído.");
    auto refresh_preview = [this, axis, start, end, period, preview,
                            snapshots] {
      snapshots->clear();
      for (const auto &id :
           {start->selected_id(),
            period->get_active() ? end->selected_id() : std::nullopt})
        if (id)
          if (auto p = service_.planning().time_point(*id))
            snapshots->emplace(*id, *p);
      if (axis->get_active_id().empty() || !start->selected_id() ||
          (period->get_active() && !end->selected_id())) {
        preview->set_text("Prévia incompleta — nenhum vínculo será aplicado "
                          "sem linha do tempo e limites explícitos.");
        return;
      }
      project::SemanticTemporalBinding b;
      b.fictional_axis_id = axis->get_active_id().raw();
      b.fictional_start_point_id = *start->selected_id();
      if (period->get_active())
        b.fictional_end_point_id = end->selected_id();
      preview->set_text("Prévia: " + semantic_time_label(b) +
                        "\nOs limites do período são inclusivos. Ordinais "
                        "indicam ordem, não duração ou unidade de calendário.");
    };
    axis->signal_changed().connect(
        [axis, chosen_axis, start, end, refresh_preview] {
          *chosen_axis = axis->get_active_id().raw();
          start->clear_selection();
          end->clear_selection();
          start->refresh();
          end->refresh();
          refresh_preview();
        });
    period->signal_toggled().connect([period, end, refresh_preview] {
      end->set_visible(period->get_active());
      end->clear_selection();
      refresh_preview();
    });
    start->signal_selection_changed().connect(refresh_preview);
    end->signal_selection_changed().connect(refresh_preview);
    dialog->signal_response().connect([this, dialog, annotation_id,
                                       docid = d.id, epoch, revision, axis,
                                       start, end, period, note, axes,
                                       snapshots](int r) {
      if (r != Gtk::ResponseType::ACCEPT) {
        dialog->hide();
        return;
      }
      try {
        if (!current_document_ || current_document_->id != docid ||
            edit_epoch_ != epoch || edit_revision_ != revision)
          throw std::runtime_error("Documento mudou; reabra a prévia temporal");
        const auto aid = axis->get_active_id().raw();
        auto sid = start->selected_id();
        auto eid = period->get_active() ? end->selected_id() : std::nullopt;
        if (aid.empty() || !sid || (period->get_active() && !eid))
          throw std::runtime_error("Declare a linha do tempo e os limites do vínculo");
        const auto current_axes = service_.planning().time_axes();
        auto old_axis = std::find_if(axes.begin(), axes.end(),
                                     [&](auto &a) { return a.id == aid; });
        auto now_axis = std::find_if(current_axes.begin(), current_axes.end(),
                                     [&](auto &a) { return a.id == aid; });
        if (old_axis == axes.end() || now_axis == current_axes.end() ||
            old_axis->name != now_axis->name ||
            old_axis->description != now_axis->description)
          throw std::runtime_error("A linha do tempo mudou; reabra a prévia");
        for (const auto &id : {sid, eid})
          if (id) {
            auto now = service_.planning().time_point(*id);
            auto old = snapshots->find(*id);
            if (!now || old == snapshots->end() ||
                point_state(*now) != point_state(old->second) ||
                now->axis_id != aid)
              throw std::runtime_error("Marco na linha do tempo mudou; reabra a prévia");
          }
        auto next = application::TextSemanticService::bind_fictional_time(
            capture_editor_state(), annotation_id, aid, *sid, eid,
            note->get_text().raw());
        service_.writing().validate_semantic_document(next);
        dialog->hide();
        apply_semantic_document(std::move(next),
                                "Vincular tempo da história (manual)");
        show_semantic_layers(semantic_offset_);
      } catch (const std::exception &e) {
        dialog->hide();
        show_error("Vínculo temporal não aplicado", e);
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível abrir o vínculo temporal", e);
  }
}
void WritingWorkspace::edit_semantic_dates(const std::string &annotation) {
  if (!current_document_ || !owner_window())
    return;
  try {
    require_completed_composition();
    const auto context = interaction_context(false);
    const auto d = context.state.document;
    const auto epoch = edit_epoch_, revision = edit_revision_;
    auto *dialog =
        new OverlayDialog("Data ou horário no texto", *owner_window(), true);
    dialog->set_secondary_text(
        "Reutilize uma referência ou crie uma data aqui. O vínculo usa a mesmo trecho marcado da anotação, inclusive perspectiva, sem alterar o texto. Desfazer retira o vínculo e mantém a referência para reutilização.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Vincular data ao texto", Gtk::ResponseType::ACCEPT);
    auto *box = dialog->get_content_area();
    auto *axis = Gtk::make_managed<Gtk::ComboBoxText>();
    constrain_temporal_combo(*axis);
    axis->append("", "Escolha a linha do tempo");
    const auto axes = service_.planning().time_axes();
    for (const auto &a : axes)
      axis->append(a.id, a.name);
    axis->set_active(0);
    if(axes.size()==1) axis->set_active_id(axes.front().id);
    set_accessible_label(*axis, "Linha do tempo desta data");
    box->append(*axis);
    if(axes.empty()) label(*box,"Este Projeto ainda não tem linha do tempo. Crie uma linha do tempo em Planejar → Cronologia; nenhuma data será atribuída automaticamente.");
    auto *replace = Gtk::make_managed<Gtk::CheckButton>("Usar outra data no lugar de um vínculo deste trecho");
    auto *previous = Gtk::make_managed<Gtk::ComboBoxText>();
    constrain_temporal_combo(*previous);
    previous->append("", "Escolha o vínculo a substituir");
    auto source = std::find_if(d.semantics.annotations.begin(),d.semantics.annotations.end(),
                              [&](const auto &a){return a.id==annotation;});
    if(source==d.semantics.annotations.end()) throw std::runtime_error("Anotação ausente");
    for(const auto &binding:source->dates) {
      const auto r=service_.worldbuilding().temporal_reference(binding.reference_id);
      previous->append(binding.id,value_label(binding.role)+" — "+r.label);
    }
    previous->set_active(0); previous->set_visible(false);
    replace->set_visible(!source->dates.empty()); box->append(*replace); box->append(*previous);
    replace->signal_toggled().connect([=]{previous->set_visible(replace->get_active());});
    set_accessible_label(*previous,"Vínculo textual a substituir — não edita a referência compartilhada");
    auto *create = Gtk::make_managed<Gtk::CheckButton>("Criar nova data ou intervalo aqui");
    box->append(*create);
    auto *draft = Gtk::make_managed<TemporalReferenceEditor>(service_.worldbuilding().calendars());
    box->append(*draft);
    draft->set_visible(false);
    auto *date = Gtk::make_managed<Gtk::ComboBoxText>();
    constrain_temporal_combo(*date);
    auto *search = Gtk::make_managed<Gtk::SearchEntry>();
    search->set_placeholder_text("Pesquisar nome ou data da referência");
    box->append(*search);
    box->append(*date);
    set_accessible_label(*date, "Referência temporal do texto");
    create->signal_toggled().connect([=] {
      draft->set_visible(create->get_active());
      date->set_visible(!create->get_active());
      search->set_visible(!create->get_active());
    });
    auto refs =
        std::make_shared<std::map<std::string, project::TemporalReference>>();
    auto refresh = [=, this] {
      date->remove_all();
      date->append("", "Escolha explicitamente uma referência");
      date->set_active(0);
      refs->clear();
      if (axis->get_active_id().empty())
        return;
      const auto query = search->get_text().casefold();
      for (const auto &r : service_.worldbuilding().temporal_references(
               axis->get_active_id())) {
        const auto caption =
            r.label + " — " +
            project::format_temporal_reference(
                service_.worldbuilding().calendar(r.calendar_id), r);
        if (!query.empty() && Glib::ustring(caption).casefold().find(query) ==
                                  Glib::ustring::npos)
          continue;
        refs->emplace(r.id, r);
        date->append(
            r.id, r.label + " — " +
                      project::format_temporal_reference(
                          service_.worldbuilding().calendar(r.calendar_id), r));
      }
    };
    axis->signal_changed().connect(sigc::track_obj(refresh, *date, *axis, *search));
    search->signal_search_changed().connect(sigc::track_obj(refresh, *date, *axis, *search));
    refresh();
    auto *role = Gtk::make_managed<Gtk::ComboBoxText>();
    role->append("", "Escolha o que esta data indica");
    for (const auto &key : {"scene", "mention", "memory", "reference"})
      role->append(key, value_label(key));
    role->set_active(0);
    set_accessible_label(*role, "Papel temporal no texto");
    box->append(*role);
    auto *note = Gtk::make_managed<Gtk::Entry>();
    note->set_placeholder_text("Explicação autoral (opcional)");
    box->append(*note);
    dialog->signal_response().connect([=, this](int response) {
      if (response != Gtk::ResponseType::ACCEPT) {
        dialog->hide();
        return;
      }
      try {
        if (!current_document_ || current_document_->id != d.id ||
            edit_epoch_ != epoch || edit_revision_ != revision)
          throw std::runtime_error(
              "O Documento mudou; reabra a escolha temporal");
        require_interaction_context(context);
        auto found = refs->find(date->get_active_id());
        if (axis->get_active_id().empty() || role->get_active_id().empty() ||
            (!create->get_active() && found == refs->end()))
          throw std::runtime_error("Escolha a data e o que ela indica");
        if (!create->get_active() && service_.worldbuilding().temporal_reference(found->first) !=
            found->second)
          throw std::runtime_error(
              "A data mudou; confira novamente antes de vincular");
        project::TextDateCreation plan;
        if (create->get_active()) {
          plan.reference = draft->draft(axis->get_active_id());
          plan.calendar = draft->calendar();
          project::validate(plan.calendar, plan.reference);
          plan.reference.id = project::new_uuid();
          plan.reference.token = project::new_uuid();
        }
        auto before = context.state.document;
        if(replace->get_active()) {
          if(previous->get_active_id().empty()) throw std::runtime_error("Escolha qual vínculo deste trecho substituir");
          before = application::TextSemanticService::unbind_date(before,annotation,previous->get_active_id());
        }
        auto next = application::TextSemanticService::bind_date(
            before, annotation, create->get_active() ? plan.reference.id : found->first,
            role->get_active_id(), note->get_text());
        if (create->get_active()) commit_text_date(context, std::move(next), plan);
        else {
          auto state = context.state;
          state.document = std::move(next);
          service_.writing().validate_semantic_document(state.document);
          apply_edit_transaction(std::move(state), "Vincular data ou horário ao texto");
          refresh_semantic_lenses();
        }
        dialog->hide();
        page_stack_.set_visible_child("editor");
        editor_.grab_focus();
        signal_status_message_.emit("Data vinculada ao contexto textual. "
                                    "Avisos temporais nunca impedem salvar.");
      } catch (const std::exception &e) {
        dialog->set_secondary_text(std::string("Não aplicado: ") + author::diagnostic(e.what()));
        dialog->keep_open_on_response();
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível abrir datas do texto", e);
  }
}
void WritingWorkspace::show_temporal_notices() {
  if (!current_document_ || !owner_window())
    return;
  try {
    const auto notices =
        service_.worldbuilding().continuity_notices(capture_editor_state());
    auto *dialog = new OverlayDialog("Avisos temporais — não bloqueantes",
                                     *owner_window(), true);
    dialog->add_button("Voltar à escrita", Gtk::ResponseType::CLOSE);
    dialog->set_secondary_text(
        "Consulta derivada de contextos de cena, presença e nascimento "
        "formalmente declarados. Viagem no tempo, memória e narrador não "
        "confiável são possibilidades autorais. Nenhum aviso impede salvar ou "
        "altera a obra.");
    auto *box = dialog->get_content_area();
    if (notices.empty())
      label(*box, "Nenhum conflito comprovável pelas declarações comparáveis. Isso não certifica toda a continuidade: menções, múltiplos tempos, aproximações, trechos não localizados e linhas do tempo não alinhadas não são presumidos.");
    for (const auto &n : notices)
      label(*box, n.message);
    if (notices.size() == 200)
      label(*box, "Esta consulta mostra no máximo 200 avisos. Pode haver "
                  "outros; não é uma certificação de toda a continuidade.");
    dialog->signal_response().connect([this, dialog](int) {
      dialog->hide();
      page_stack_.set_visible_child("editor");
      editor_.grab_focus();
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Consulta temporal indisponível", e);
  }
}
} // namespace inde::ui
