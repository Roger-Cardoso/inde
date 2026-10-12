#pragma once
#include "inde/ui/author_vocabulary.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
namespace inde::ui::semantic_ui {
inline std::string value_label(const std::string &key) {
  if(key=="scene")return "Tempo da cena / ocorrência narrada";
  if(key=="reference")return "Referência temporal, sem presença presumida";
  if(key=="time_travel")return "Viagem no tempo";
  if(key=="prophecy")return "Profecia / antecipação";
  if(key=="unreliable_narrator")return "Narrador não confiável";
  if(key=="materialization")return "Apresenta a ficha no texto";
  if(key=="evidence")return "Evidência textual declarada";
  if(key=="mention")return "Menção contextual";
  if(key=="contradiction")return "Contradição / contraponto";
  if(key=="open")return "Aberto";
  if(key=="resolved")return "Resolvido";
  static const std::map<std::string, std::string> labels{
      {"action", "Ação"},
      {"exposition", "Exposição"},
      {"summary", "Resumo"},
      {"transition", "Transição"},
      {"backstory", "Antecedentes"},
      {"reflection", "Reflexão"},
      {"general", "Geral"},
      {"appearance", "Aparência"},
      {"body", "Corpo"},
      {"clothing", "Vestuário"},
      {"architecture", "Arquitetura"},
      {"weather", "Clima"},
      {"condition", "Condição"},
      {"movement", "Movimento"},
      {"sensory", "Sensorial"},
      {"spoken", "Oral"},
      {"written", "Escrito"},
      {"telepathy", "Telepatia"},
      {"statement", "Declaração"},
      {"question", "Pergunta"},
      {"command", "Ordem"},
      {"request", "Pedido"},
      {"promise", "Promessa"},
      {"threat", "Ameaça"},
      {"warning", "Aviso"},
      {"apology", "Pedido de desculpas"},
      {"greeting", "Saudação"},
      {"farewell", "Despedida"},
      {"accusation", "Acusação"},
      {"denial", "Negação"},
      {"confirmation", "Confirmação"},
      {"refusal", "Recusa"},
      {"suggestion", "Sugestão"},
      {"invitation", "Convite"},
      {"explanation", "Explicação"},
      {"joke", "Piada"},
      {"insult", "Insulto"},
      {"praise", "Elogio"},
      {"confession", "Confissão"},
      {"initiative", "Iniciativa"},
      {"response", "Resposta"},
      {"reply", "Réplica"},
      {"counter_reply", "Tréplica"},
      {"follow_up", "Continuação"},
      {"interruption", "Interrupção"},
      {"internal_monologue", "Monólogo interior"},
      {"free_indirect", "Discurso indireto livre"},
      {"stream_of_consciousness", "Fluxo de consciência"},
      {"memory", "Memória"},
      {"imagination", "Imaginação"},
      {"reasoning", "Raciocínio"},
      {"emotion", "Emoção"},
      {"intention", "Intenção"},
      {"decision", "Decisão"},
      {"clue", "Pista"},
      {"foreshadowing", "Antecipação"},
      {"motif", "Motivo recorrente"},
      {"callback", "Retomada"},
      {"editorial", "Editorial"}};
  static const std::map<std::string, std::string> perspective{
      {"first_person", "Primeira pessoa"}, {"second_person", "Segunda pessoa"},
      {"third_person", "Terceira pessoa"}, {"internal", "Interna"},
      {"external", "Externa"}, {"omniscient", "Onisciente"},
      {"variable", "Variável"}};
  if (auto p = perspective.find(key); p != perspective.end()) return p->second;
  auto i = labels.find(key);
  return i == labels.end() ? key : i->second;
}
inline void retire(OverlayDialog *d) {
  // Inner navigation buttons need deferred destruction. Footer responses are
  // already deleted by OverlayDialog::present and must only call hide().
  static std::set<OverlayDialog *> retiring;
  if (!retiring.insert(d).second)
    return;
  d->hide();
  Glib::signal_idle().connect_once([d] {
    retiring.erase(d);
    delete d;
  });
}
inline Gtk::Label *label(Gtk::Box &box, const std::string &s) {
  auto *l = Gtk::make_managed<Gtk::Label>(s);
  l->set_wrap(true);
  l->set_xalign(0);
  box.append(*l);
  return l;
}
template <class F> void button(Gtk::Box &box, const std::string &s, F f) {
  auto *b = Gtk::make_managed<Gtk::Button>(s);
  b->signal_clicked().connect(f);
  box.append(*b);
}
struct FieldWidget {
  project::SemanticFieldDefinition definition;
  Gtk::Entry *text{};
  std::shared_ptr<std::vector<Gtk::TextView *>> texts;
  Gtk::ComboBoxText *single{};
  std::vector<std::pair<std::string, Gtk::CheckButton *>> options;
  std::shared_ptr<std::vector<std::string>> entities;
};

} // namespace inde::ui::semantic_ui
