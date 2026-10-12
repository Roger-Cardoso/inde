#include "inde/ui/keyboard_commands.hpp"
#include "inde/ui/ui_preferences.hpp"
#include <algorithm>
#include <set>
#include <unordered_map>

namespace inde::ui {
namespace {
using BindingCache=std::unordered_map<std::string,std::string>;
constexpr auto cache_key="inde-keyboard-binding-cache";
BindingCache &binding_cache(Gtk::Window &window) {
  return *static_cast<BindingCache *>(g_object_get_data(G_OBJECT(window.gobj()),cache_key));
}
}
unsigned shortcut_modifiers(Gdk::ModifierType modifiers) {
  return static_cast<unsigned>(modifiers) &
      (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK);
}
bool shortcut_matches(guint key, Gdk::ModifierType modifiers, const std::string &accelerator) {
  if(key>=GDK_KEY_KP_0 && key<=GDK_KEY_KP_9)key=GDK_KEY_0+(key-GDK_KEY_KP_0);
  if(key==GDK_KEY_KP_Add)key=GDK_KEY_plus;
  if(key==GDK_KEY_KP_Subtract)key=GDK_KEY_minus;
  if(key==GDK_KEY_KP_Enter)key=GDK_KEY_Return;
  guint expected{}; GdkModifierType mods{};
  gtk_accelerator_parse(accelerator.c_str(), &expected, &mods);
  if(expected==GDK_KEY_plus && key==GDK_KEY_equal && shortcut_modifiers(modifiers)==GDK_CONTROL_MASK)key=GDK_KEY_plus;
  return expected && gdk_keyval_to_lower(key) == gdk_keyval_to_lower(expected) &&
      shortcut_modifiers(modifiers) == shortcut_modifiers(static_cast<Gdk::ModifierType>(mods));
}
bool focus_inside(Gtk::Widget *focus, const Gtk::Widget &region) {
  return focus && (focus == &region || gtk_widget_is_ancestor(focus->gobj(), const_cast<GtkWidget *>(region.gobj())));
}
bool text_input_focus(Gtk::Widget *focus) {
  for (auto *widget = focus; widget; widget = widget->get_parent())
    if (GTK_IS_EDITABLE(widget->gobj()) || GTK_IS_TEXT_VIEW(widget->gobj())) return true;
  return false;
}
bool input_composition_active(Gtk::Widget *focus) {
  for (auto *w = focus; w; w = w->get_parent())
    if (g_object_get_data(G_OBJECT(w->gobj()), "inde-preedit-active")) return true;
  return false;
}
KeyboardCommands::KeyboardCommands(Gtk::Window &window, Gtk::Widget &anchor,
    std::function<bool()> blocked, std::function<void(const std::string &)> status)
    : window_(window), blocked_(std::move(blocked)), status_(std::move(status)) {
  g_object_set_data_full(G_OBJECT(window_.gobj()),cache_key,new BindingCache,[](gpointer value){delete static_cast<BindingCache *>(value);});
  g_weak_ref_init(&origin_, nullptr);
  g_weak_ref_init(&last_context_focus_, nullptr);
  auto watch_focus = [this,&anchor] {
    if(auto *focus=window_.get_focus(); focus && !focus_inside(focus,palette_) && !focus_inside(focus,anchor))
      g_weak_ref_set(&last_context_focus_,G_OBJECT(focus->gobj()));
    for (auto *w = window_.get_focus(); w; w = w->get_parent()) {
      auto *object = G_OBJECT(w->gobj());
      if (!g_signal_lookup("preedit-changed", G_OBJECT_TYPE(object)) || g_object_get_data(object, "inde-preedit-watched")) continue;
      g_object_set_data(object, "inde-preedit-watched", GINT_TO_POINTER(1));
      g_signal_connect(object, "preedit-changed", G_CALLBACK(+[](GObject *input, const char *preedit, gpointer) {
        g_object_set_data(input, "inde-preedit-active", preedit && *preedit ? GINT_TO_POINTER(1) : nullptr);
      }), nullptr);
    }
  };
  focus_changed_ = window_.property_focus_widget().signal_changed().connect(watch_focus);
  watch_focus();
  palette_.set_parent(anchor);
  palette_.set_has_arrow(false);
  palette_.set_child(body_);
  body_.set_margin(12);
  body_.set_size_request(520, -1);
  heading_.set_xalign(0); heading_.add_css_class("title-3");
  hint_.set_xalign(0); hint_.set_wrap(true); hint_.set_max_width_chars(64);
  hint_.set_text("Enter executa • Esc volta • Tab navega. Comandos revalidam o contexto; nada é aplicado ao fundo.");
  search_.set_placeholder_text("Buscar comando ou recurso…");
  scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll_.set_min_content_height(180); scroll_.set_max_content_height(320);
  scroll_.set_propagate_natural_height(true); scroll_.set_child(results_);
  results_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  body_.append(heading_); body_.append(search_); body_.append(scroll_); body_.append(hint_);
  bindings_.append(binding_entry_); bindings_.append(assign_); bindings_.append(remove_);
  binding_entry_.set_placeholder_text("Clique aqui e pressione o novo atalho…");
  binding_entry_.set_editable(false);
  binding_entry_.set_tooltip_text("Pressione a combinação desejada e escolha Alterar atalho. Esc cancela; Tab muda de controle.");
  auto binding_capture=Gtk::EventControllerKey::create();binding_capture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  binding_capture->signal_key_pressed().connect([this,raw=binding_capture->gobj()](guint key,guint code,Gdk::ModifierType mods) {
    if(input_composition_active(window_.get_focus()) || key==GDK_KEY_Escape || key==GDK_KEY_Tab || key==GDK_KEY_ISO_Left_Tab)return false;
    const auto mask=shortcut_modifiers(mods);
    if(key==GDK_KEY_Control_L || key==GDK_KEY_Control_R || key==GDK_KEY_Shift_L || key==GDK_KEY_Shift_R || key==GDK_KEY_Alt_L || key==GDK_KEY_Alt_R)return true;
    auto *event=gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(raw));
    if(event && gdk_event_get_event_type(event)==GDK_KEY_PRESS) {
      if(gdk_key_event_get_level(event)>1){hint_.set_text("AltGr é reservado à entrada de texto.");return true;}
      guint base{};int group{},level{};GdkModifierType consumed{};
      if((mask&GDK_SHIFT_MASK) && gdk_display_translate_key(gdk_event_get_display(event),code,static_cast<GdkModifierType>(static_cast<unsigned>(mods)&~GDK_SHIFT_MASK),static_cast<int>(gdk_key_event_get_layout(event)),&base,&group,&level,&consumed))key=base;
    }
    if(!(mask&(GDK_CONTROL_MASK|GDK_ALT_MASK)) && !(key>=GDK_KEY_F1 && key<=GDK_KEY_F12)) {
      hint_.set_text("Use uma combinação com Ctrl/Alt ou uma tecla de função. Texto simples permanece protegido.");return true;
    }
    auto *name=gtk_accelerator_name(gdk_keyval_to_lower(key),static_cast<GdkModifierType>(mask));
    auto *label=gtk_accelerator_get_label(gdk_keyval_to_lower(key),static_cast<GdkModifierType>(mask));
    g_object_set_data_full(G_OBJECT(binding_entry_.gobj()),"inde-binding-candidate",name,g_free);
    binding_entry_.set_text(label ? label : "");g_free(label);
    hint_.set_text("Combinação preparada. Alterar atalho verifica conflitos e salva somente neste perfil.");return true;
  },false);binding_entry_.add_controller(binding_capture);
  body_.append(bindings_); body_.append(reset_);
  search_.signal_search_changed().connect([this] { rebuild(); });
  results_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    if (!row || row->get_index() < 0 || static_cast<std::size_t>(row->get_index()) >= result_ids_.size()) return;
    const auto &id = result_ids_[row->get_index()];
    auto it = std::find_if(commands_.begin(), commands_.end(), [&](const auto &c) { return c.id == id; });
    if (it != commands_.end()) {
      const auto chord=binding(*it);guint key{};GdkModifierType mods{};gtk_accelerator_parse(chord.c_str(),&key,&mods);
      auto *label=key ? gtk_accelerator_get_label(key,mods) : nullptr;
      binding_entry_.set_text(label ? label : "");g_free(label);
      g_object_set_data_full(G_OBJECT(binding_entry_.gobj()),"inde-binding-candidate",g_strdup(chord.c_str()),g_free);
    }
  });
  results_.signal_row_activated().connect([this](Gtk::ListBoxRow *row) {
    if (row && static_cast<std::size_t>(row->get_index()) < result_ids_.size()) execute(result_ids_[row->get_index()]);
  });
  auto change = [this](const std::string &value) {
    auto *row = results_.get_selected_row();
    if (!row || static_cast<std::size_t>(row->get_index()) >= result_ids_.size()) return;
    std::string error;
    if (!rebind(result_ids_[row->get_index()], value, error)) hint_.set_text(error);
    else { hint_.set_text("Preferência de teclado salva neste perfil; o projeto não foi alterado."); rebuild(); }
  };
  assign_.signal_clicked().connect([this, change] {
    auto *candidate=static_cast<const char *>(g_object_get_data(G_OBJECT(binding_entry_.gobj()),"inde-binding-candidate"));
    if(candidate)change(candidate);
  });
  remove_.signal_clicked().connect([change] { change(""); });
  reset_.signal_clicked().connect([this] { restore_defaults(); rebuild(); });
  auto keys = Gtk::EventControllerKey::create();
  keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect([this](guint key, guint, Gdk::ModifierType mods) {
    if(input_composition_active(window_.get_focus()))return false;
    if (shortcut_modifiers(mods)) return false;
    if (key == GDK_KEY_Escape) { palette_.popdown(); restore_origin(); return true; }
    if (focus_inside(window_.get_focus(), search_) && (key == GDK_KEY_Down || key == GDK_KEY_Return)) {
      auto *row = results_.get_selected_row();
      if (!row) row = results_.get_row_at_index(0);
      if (row) { results_.select_row(*row); if (key == GDK_KEY_Return) execute(result_ids_[row->get_index()]); else row->grab_focus(); }
      return true;
    }
    return false;
  }, false);
  palette_.add_controller(keys);
  palette_.signal_closed().connect([this] { restore_origin(); });
  auto router = Gtk::EventControllerKey::create();
  router->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  router->signal_key_pressed().connect([this, raw = router->gobj()](guint key, guint code, Gdk::ModifierType mods) {
    auto *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(raw));
    if (event && gdk_event_get_event_type(event) == GDK_KEY_PRESS) {
      if (gdk_key_event_get_level(event) > 1) return false; // AltGr/level 3
      if(key==GDK_KEY_plus && shortcut_modifiers(mods)==(GDK_CONTROL_MASK|GDK_SHIFT_MASK)) {
        if(dispatch(key,mods))return true;
        return dispatch(key,static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK));
      }
      // Shift+digits produces punctuation on ABNT2. Translate only Shift away;
      // retain the full modifier mask for exact chord matching.
      guint base{}; int group{}, level{}; GdkModifierType consumed{};
      if ((shortcut_modifiers(mods) & GDK_SHIFT_MASK) &&
          gdk_display_translate_key(gdk_event_get_display(event), code,
              static_cast<GdkModifierType>(static_cast<unsigned>(mods) & ~GDK_SHIFT_MASK),
              static_cast<int>(gdk_key_event_get_layout(event)), &base, &group, &level, &consumed)) key = base;
    }
    return dispatch(key, mods);
  }, false);
  window_.add_controller(router);
  // The controller is owned by the window; disconnect it before this dies.
  g_object_set_data_full(G_OBJECT(router->gobj()), "inde-router-owner", this, nullptr);
}
KeyboardCommands::~KeyboardCommands() {
  focus_changed_.disconnect();
  // Window controllers can survive members during Gtk destruction.
  if(window_.gobj()) {
    auto *controllers = gtk_widget_observe_controllers(GTK_WIDGET(window_.gobj()));
    for (guint i = 0; i < g_list_model_get_n_items(controllers); ++i) {
      auto *controller = G_OBJECT(g_list_model_get_item(controllers, i));
      if (g_object_get_data(controller, "inde-router-owner") == this)
        gtk_widget_remove_controller(GTK_WIDGET(window_.gobj()), GTK_EVENT_CONTROLLER(controller));
      g_object_unref(controller);
    }
    g_object_unref(controllers);
  }
  if(palette_.get_parent())palette_.unparent();
  g_weak_ref_clear(&origin_);g_weak_ref_clear(&last_context_focus_);
}
void KeyboardCommands::add(KeyboardCommand command) {
  if (command.id.empty() || !command.activate || std::any_of(commands_.begin(), commands_.end(), [&](const auto &c) { return c.id == command.id; }))
    throw std::invalid_argument("Identidade de comando inválida ou duplicada");
  const auto label=Glib::ustring(command.label).casefold();
  for(const auto &[author,technical]:std::vector<std::pair<std::string,std::string>>{
      {"elemento da história","entidade"},{"universo","propriedade intelectual IP"},
      {"plano","estrutura narrativa"},{"parte","unidade narrativa"},
      {"montagem","composição ocorrência bloco"},{"marco","ponto temporal"},
      {"papel","função narrativa"},{"item do manuscrito","nó estrutural elemento editorial"}})
    if(label.find(author.c_str())!=Glib::ustring::npos)command.aliases+=" "+technical;
  commands_.push_back(std::move(command));
  const auto &id=commands_.back().id;
  binding_cache(window_)[id]=ui_preference("keyboard-v1",id,"@default");
}
std::string KeyboardCommands::binding(const KeyboardCommand &command) const {
  const auto &cache=binding_cache(window_);const auto it=cache.find(command.id);
  const auto stored=it==cache.end() ? std::string("@default") : it->second;
  if(stored=="@default" || stored.empty())return stored=="@default" ? command.accelerator : "";
  guint key{};GdkModifierType mods{};gtk_accelerator_parse(stored.c_str(),&key,&mods);
  const auto mask=static_cast<unsigned>(mods);
  if(!key || !gtk_accelerator_valid(key,mods) || (mask&(GDK_SUPER_MASK|GDK_META_MASK|GDK_HYPER_MASK)) ||
      ((mask&GDK_CONTROL_MASK) && (mask&GDK_ALT_MASK)) ||
      (!(key>=GDK_KEY_F1 && key<=GDK_KEY_F12) && !(mask&(GDK_CONTROL_MASK|GDK_ALT_MASK))) ||
      shortcut_matches(key,static_cast<Gdk::ModifierType>(mods),"<Control><Shift>u") ||
      key==GDK_KEY_Escape || key==GDK_KEY_Tab || key==GDK_KEY_Return || key==GDK_KEY_Delete)return command.accelerator;
  return stored;
}
Gtk::Widget *KeyboardCommands::origin() const {
  auto *object = g_weak_ref_get(const_cast<GWeakRef *>(&origin_));
  if (!object) return nullptr;
  auto *widget = Glib::wrap(GTK_WIDGET(object)); g_object_unref(object); return widget;
}
void KeyboardCommands::restore_origin() {
  if (auto *widget = origin(); widget && widget->get_mapped() && widget->get_sensitive()) {
    widget->grab_focus();
    if (origin_buffer_ && origin_cursor_ && origin_bound_)
      origin_buffer_->select_range(origin_buffer_->get_iter_at_mark(origin_cursor_), origin_buffer_->get_iter_at_mark(origin_bound_));
    if (origin_editable_ && GTK_IS_EDITABLE(widget->gobj()))
      gtk_editable_select_region(GTK_EDITABLE(widget->gobj()),origin_entry_bound_,origin_entry_cursor_);
  }
  if(origin_buffer_) { origin_buffer_->delete_mark(origin_cursor_);origin_buffer_->delete_mark(origin_bound_); }
  origin_buffer_.reset();origin_cursor_.reset();origin_bound_.reset();origin_editable_=false;
  // Closed may arrive after a command opened a modal; restore only once.
  g_weak_ref_set(&origin_,nullptr);
}
void KeyboardCommands::show(bool help) {
  if (blocked_ && blocked_()) return;
  // Refresh only on explicit help/palette entry. Typing never reads an INI
  // once per command per key; another window's preferences are picked up here.
  for(const auto &command:commands_)binding_cache(window_)[command.id]=ui_preference("keyboard-v1",command.id,"@default");
  auto *saved=g_weak_ref_get(&last_context_focus_);
  auto *focus=window_.get_focus();
  g_weak_ref_set(&origin_, focus && !focus_inside(focus,*palette_.get_parent()) ? G_OBJECT(focus->gobj()) : saved);
  if(saved)g_object_unref(saved);
  if(auto *widget=origin()) {
    if(auto *view=dynamic_cast<Gtk::TextView *>(widget)) {
      origin_buffer_=view->get_buffer();
      origin_cursor_=origin_buffer_->create_mark(origin_buffer_->get_iter_at_mark(origin_buffer_->get_insert()));
      origin_bound_=origin_buffer_->create_mark(origin_buffer_->get_iter_at_mark(origin_buffer_->get_selection_bound()));
    } else if(GTK_IS_EDITABLE(widget->gobj())) {
      origin_editable_=true;origin_entry_cursor_=gtk_editable_get_position(GTK_EDITABLE(widget->gobj()));
      int start{},end{};gtk_editable_get_selection_bounds(GTK_EDITABLE(widget->gobj()),&start,&end);
      origin_entry_bound_=start==origin_entry_cursor_ ? end : start;
    }
  }
  help_ = help; heading_.set_text(help ? "Teclado e comandos deste contexto" : "Comandos");
  bindings_.set_visible(help); reset_.set_visible(help);
  search_.set_text(""); rebuild(); palette_.popup(); search_.grab_focus();
}
void KeyboardCommands::rebuild() {
  while (auto *child = results_.get_first_child()) results_.remove(*child);
  result_ids_.clear();
  const auto query = search_.get_text().casefold();
  auto *focus = origin();
  for (const auto &command : commands_) {
    if (!help_ && command.scope && !command.scope(focus)) continue;
    const auto chord = binding(command);
    guint key{};GdkModifierType modifiers{};gtk_accelerator_parse(chord.c_str(),&key,&modifiers);
    auto *caption=key ? gtk_accelerator_get_label(key,modifiers) : nullptr;
    const std::string visible_chord=caption ? caption : "";g_free(caption);
    const auto searchable = Glib::ustring(command.label + " " + command.group + " " + command.aliases + " " + chord).casefold();
    if (!query.empty() && searchable.find(query) == Glib::ustring::npos) continue;
    const bool scoped = !command.scope || command.scope(focus);
    const auto reason = !scoped ? "Disponível no contexto indicado" : command.unavailable ? command.unavailable() : "";
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
    box->set_margin(8);
    auto *label = Gtk::make_managed<Gtk::Label>(command.label + (visible_chord.empty() ? "" : "    " + visible_chord));
    label->set_xalign(0); label->set_wrap(true); box->append(*label);
    auto *detail = Gtk::make_managed<Gtk::Label>(command.group + (reason.empty() ? "" : " — " + reason));
    detail->set_xalign(0); detail->set_wrap(true); detail->add_css_class("dim-label"); box->append(*detail);
    results_.append(*box); result_ids_.push_back(command.id);
  }
  if (auto *first = results_.get_row_at_index(0)) results_.select_row(*first);
}
void KeyboardCommands::execute(std::string id) {
  const auto it = std::find_if(commands_.begin(), commands_.end(), [&](const auto &c) { return c.id == id; });
  if (it == commands_.end()) return;
  auto command = *it;
  auto *focus = origin();
  const auto reason = command.scope && !command.scope(focus) ? "Volte ao contexto indicado antes de usar este comando." :
      command.unavailable ? command.unavailable() : "";
  if (!reason.empty()) { hint_.set_text(reason); return; }
  palette_.popdown(); restore_origin();
  if (blocked_ && blocked_()) return;
  command.activate();
}
bool KeyboardCommands::dispatch(guint key, Gdk::ModifierType modifiers) {
  if (palette_.get_mapped() || input_composition_active(window_.get_focus())) return false;
  const bool blocked=blocked_ && blocked_();
  auto *focus = window_.get_focus();
  for(auto *widget=focus;widget;widget=widget->get_parent())
    if(GTK_IS_POPOVER(widget->gobj()))return false;
  for (const auto &command : commands_) {
    // F7 consults only the field inside the modal; no background command is
    // exempt from the gate. Suggestions still require explicit selection.
    if(blocked && (command.id!="proofread-current" || !text_input_focus(focus)))continue;
    if (!shortcut_matches(key, modifiers, binding(command)) || (command.scope && !command.scope(focus))) continue;
    const auto reason = command.unavailable ? command.unavailable() : "";
    if (!reason.empty()) { if (status_) status_(reason); return true; }
    command.activate(); return true;
  }
  if(blocked)return false;
  if (shortcut_matches(key, modifiers, "F6")) { cycle_region(false); return true; }
  if (shortcut_matches(key, modifiers, "<Shift>F6")) { cycle_region(true); return true; }
  return false;
}
void KeyboardCommands::cycle_region(bool reverse) {
  if (!regions_) return;
  auto regions = regions_();
  std::erase_if(regions, [](auto *w) { return !w || !w->get_mapped() || !w->get_sensitive(); });
  if (regions.empty()) return;
  std::size_t current = regions.size() - 1;
  for (std::size_t i = 0; i < regions.size(); ++i) if (focus_inside(window_.get_focus(), *regions[i])) { current = i; break; }
  auto *target = regions[(current + (reverse ? regions.size() - 1 : 1)) % regions.size()];
  if (!target->grab_focus()) target->child_focus(Gtk::DirectionType::TAB_FORWARD);
}
bool KeyboardCommands::rebind(const std::string &id, const std::string &accelerator, std::string &error) {
  auto it = std::find_if(commands_.begin(), commands_.end(), [&](const auto &c) { return c.id == id; });
  if (it == commands_.end()) { error = "Comando inexistente"; return false; }
  guint key{}; GdkModifierType mods{};
  if (!accelerator.empty()) {
    gtk_accelerator_parse(accelerator.c_str(), &key, &mods);
    const auto mask = static_cast<unsigned>(mods);
    const bool functional=key>=GDK_KEY_F1 && key<=GDK_KEY_F12;
    if (!key || !gtk_accelerator_valid(key, mods) || (mask & (GDK_SUPER_MASK | GDK_META_MASK | GDK_HYPER_MASK)) ||
        (!functional && !(mask&(GDK_CONTROL_MASK|GDK_ALT_MASK))) ||
        ((mask & GDK_CONTROL_MASK) && (mask & GDK_ALT_MASK)) ||
        shortcut_matches(key, static_cast<Gdk::ModifierType>(mods), "<Control><Shift>u") ||
        shortcut_matches(key, static_cast<Gdk::ModifierType>(mods), "F6") ||
        shortcut_matches(key, static_cast<Gdk::ModifierType>(mods), "<Shift>F6") ||
        key == GDK_KEY_Escape || key == GDK_KEY_Tab || key == GDK_KEY_Return || key == GDK_KEY_Delete) {
      error = "Atalho inválido ou reservado para texto, navegação, IME ou sistema."; return false;
    }
    for (const auto &other : commands_) if (other.id != id && shortcut_matches(key, static_cast<Gdk::ModifierType>(mods), binding(other))) {
      error = "Conflito com “" + other.label + "”. Remova ou altere o outro vínculo primeiro."; return false;
    }
  }
  set_ui_preference("keyboard-v1", id, accelerator);
  if (ui_preference("keyboard-v1", id, "@missing") != accelerator) { error = "Não foi possível confirmar a gravação da preferência."; return false; }
  binding_cache(window_)[id]=accelerator;
  return true;
}
void KeyboardCommands::restore_defaults() {
  for (const auto &command : commands_) {
    set_ui_preference("keyboard-v1", command.id, "@default");
    binding_cache(window_)[command.id]=ui_preference("keyboard-v1",command.id,"@default");
  }
}
}
