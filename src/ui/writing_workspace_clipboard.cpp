#include "inde/ui/writing_workspace.hpp"
#include "inde/application/text_clipboard.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/keyboard_commands.hpp"
#include "editor_diagnostics.hpp"
#include <algorithm>

namespace inde::ui {
struct WritingWorkspace::ClipboardRead {
  Glib::RefPtr<Gio::InputStream> stream;
  Glib::RefPtr<Gio::Cancellable> cancel;
  std::weak_ptr<int> lifetime;
  std::string bytes, mime, document_id;
  application::TextSelection selection;
  std::uint64_t epoch{}, revision{}, generation{};
  bool styles{};
};
void WritingWorkspace::build_clipboard_ui() {
  configure_overflow_button(clipboard_button_, {
      {"Copiar texto", [this] { copy_selection(false); }, "edit-copy-symbolic"},
      {"Copiar com estilos", [this] { copy_selection(true); }, "edit-copy-symbolic"},
      {"Recortar texto", [this] { copy_selection(false, true); }, "edit-cut-symbolic"},
      {"Colar texto", [this] { paste_clipboard(false); }, "edit-paste-symbolic"},
      {"Colar com estilos", [this] { paste_clipboard(true); }, "edit-paste-symbolic"}},
      "Clipboard conservador: texto puro por padrão; estilos somente por escolha. Vínculos nunca são copiados.");
  clipboard_button_.set_label("Copiar/Colar");
  clipboard_button_.remove_css_class("circular");
  formatting_toolbar_.append(clipboard_button_);
  // Covers native context-menu and keyboard action signals, not only our buttons.
  g_signal_connect(editor_.gobj(), "copy-clipboard", G_CALLBACK(+[](GtkTextView *view, gpointer data) {
    g_signal_stop_emission_by_name(view, "copy-clipboard");
    static_cast<WritingWorkspace *>(data)->copy_selection(false);
  }), this);
  g_signal_connect(editor_.gobj(), "cut-clipboard", G_CALLBACK(+[](GtkTextView *view, gpointer data) {
    g_signal_stop_emission_by_name(view, "cut-clipboard");
    static_cast<WritingWorkspace *>(data)->copy_selection(false, true);
  }), this);
  g_signal_connect(editor_.gobj(), "paste-clipboard", G_CALLBACK(+[](GtkTextView *view, gpointer data) {
    g_signal_stop_emission_by_name(view, "paste-clipboard");
    static_cast<WritingWorkspace *>(data)->paste_clipboard(false);
  }), this);
  editor_.get_buffer()->set_enable_undo(false); // Single authoritative undo engine.
  auto actions = Gio::SimpleActionGroup::create();
  actions->add_action("undo", [this] { undo(); });
  actions->add_action("redo", [this] { redo(); });
  actions->add_action("copy-styles", [this] { copy_selection(true); });
  actions->add_action("paste-styles", [this] { paste_clipboard(true); });
  actions->add_action("create-entity", [this] { show_text_entity(true); });
  actions->add_action("link-entity", [this] { show_text_entity(false); });
  actions->add_action("create-relation", [this] { show_text_relation(); });
  actions->add_action("materialize", [this] { show_text_entity(false,true); });
  actions->add_action("distribution", [this] { show_semantic_distribution(); });
  actions->add_action("mark", [this] { edit_semantic_fields(); });
  editor_.insert_action_group("inde-editor", actions);
  editor_context_popover_ = attach_context_menu(editor_, {
      {"Recortar", [this] { copy_selection(false, true); }, "edit-cut-symbolic"},
      {"Copiar", [this] { copy_selection(false); }, "edit-copy-symbolic"},
      {"Colar", [this] { paste_clipboard(false); }, "edit-paste-symbolic"},
      {"Desfazer", [this] { undo(); }, "edit-undo-symbolic", true, false, true},
      {"Refazer", [this] { redo(); }, "edit-redo-symbolic"},
      {"Consultar seleção no dicionário…", [this] { show_semantic_dictionary(); },
       "accessories-dictionary-symbolic", true, false, true, [this] {
         Gtk::TextIter a, b;
         auto buffer = editor_.get_buffer();
         if (!current_document_ || !buffer->get_selection_bounds(a, b)) return false;
         const auto term = buffer->get_text(a, b).raw();
         return !term.empty() && term.size() <= 256 && term.find_first_of("\n\r") == std::string::npos;
       }},
      {"Comentar seleção…", [this] { edit_semantic_fields({}, "inde.editor.comment"); }, "document-edit-symbolic", true, false, true},
      {"Marcar tempo no trecho…", [this] { edit_semantic_fields({}, "inde.semantic.temporal-context"); }, "x-office-calendar-symbolic"},
      {"Associar seleção a parte da narrativa…",[this]{show_narrative_association();},"insert-link-symbolic",true,false,false,[this]{Gtk::TextIter a,b;return current_document_ && editor_.get_buffer()->get_selection_bounds(a,b);}},
      {"Sugerir alteração…", [this] { suggest_editorial_change(); }, "document-edit-symbolic"},
      {"Adicionar elemento da história à seleção…", [this] { show_text_entity(true); }, "list-add-symbolic"},
      {"Vincular elemento da história existente…", [this] { show_text_entity(false); }, "insert-link-symbolic"},
      {"Adicionar relação ao elemento da história do trecho…", [this] { show_text_relation(); }, "insert-link-symbolic"},
      {"Associar o trecho a uma ficha…", [this] { show_text_entity(false, true); }, "insert-link-symbolic"},
      {"Marcar tipo de trecho…", [this] { edit_semantic_fields(); }, "document-edit-symbolic"}});
  auto keys = Gtk::EventControllerKey::create();
  keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect([this](guint key, guint, Gdk::ModifierType mods) {
    if (ime_active_) return false;
    const auto mask=shortcut_modifiers(mods);
    if (mask != GDK_CONTROL_MASK && mask != (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) return false;
    const bool shift = (mods & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType{};
    if (key == GDK_KEY_z || key == GDK_KEY_Z) { if (shift) redo(); else undo(); return true; }
    if (key == GDK_KEY_c || key == GDK_KEY_C) { copy_selection(shift); return true; }
    if (key == GDK_KEY_x || key == GDK_KEY_X) { copy_selection(false, true); return true; }
    if (key == GDK_KEY_v || key == GDK_KEY_V) { paste_clipboard(shift); return true; }
    return false;
  }, false);
  editor_.add_controller(keys);
}
void WritingWorkspace::invalidate_clipboard() {
  ++clipboard_generation_;
  if (clipboard_cancel_) clipboard_cancel_->cancel();
  clipboard_cancel_.reset();
}
void WritingWorkspace::copy_selection(bool styles, bool cut) {
  if (!current_document_ || page_stack_.get_visible_child_name() != "editor") return;
  try {
    EditorPhaseTimer timer("clipboard_copy_gtk");
    const auto before = capture_edit_state();
    const auto start = std::min(before.selection.cursor, before.selection.bound);
    const auto end = std::max(before.selection.cursor, before.selection.bound);
    if (start == end) { signal_status_message_.emit("Selecione um trecho para copiar ou recortar."); return; }
    const auto fragment = application::copy_fragment(before.document, start, end, styles);
    Glib::Value<Glib::ustring> text; text.init(Glib::Value<Glib::ustring>::value_type()); text.set(fragment.text);
    std::vector<Glib::RefPtr<Gdk::ContentProvider>> providers{Gdk::ContentProvider::create(text)};
    if (styles) {
      const auto encoded = application::encode_fragment(fragment);
      providers.push_back(Gdk::ContentProvider::create(application::clipboard_mime, Glib::Bytes::create(encoded.data(), encoded.size())));
    }
    if (!editor_.get_clipboard()->set_content(Gdk::ContentProvider::create(providers)))
      throw std::runtime_error("Não foi possível obter a área de transferência; nenhum trecho foi recortado");
    if (cut) apply_edit_transaction(application::paste_fragment(before, start, end, {"", {}}, false), "Recortar texto");
    signal_status_message_.emit(styles ? "Texto e estilos copiados; vínculos não foram copiados" : "Texto copiado sem vínculos ou estilos de origem");
  } catch (const std::exception &error) { show_error("Não foi possível copiar/recortar", error); }
}
void WritingWorkspace::paste_clipboard(bool styles) {
  if (!current_document_ || !get_mapped() || page_stack_.get_visible_child_name() != "editor") return;
  invalidate_clipboard();
  auto request = std::make_shared<ClipboardRead>();
  request->cancel = clipboard_cancel_ = Gio::Cancellable::create();
  request->lifetime = clipboard_lifetime_;
  const auto buffer = editor_.get_buffer();
  request->selection = {static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_insert()).get_offset()),
                        static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset())};
  request->epoch = edit_epoch_; request->revision = edit_revision_; request->generation = clipboard_generation_;
  request->document_id = current_document_->id; request->styles = styles;
  std::vector<Glib::ustring> mime{"text/plain;charset=utf-8", "text/plain"};
  if (styles) mime.insert(mime.begin(), application::clipboard_mime);
  auto clipboard = editor_.get_clipboard();
  clipboard->read_async(mime, Glib::PRIORITY_DEFAULT, [this, request, clipboard](const Glib::RefPtr<Gio::AsyncResult> &result) {
    if (request->lifetime.expired()) return;
    try {
      Glib::ustring mime;
      request->stream = clipboard->read_finish(result, mime); request->mime = mime.raw();
      if (!request->cancel->is_cancelled()) read_clipboard_chunk(request);
    } catch (const Glib::Error &) {
      if (!request->cancel->is_cancelled()) signal_status_message_.emit("Clipboard não contém texto compatível; Documento não alterado.");
    }
  }, request->cancel);
}
void WritingWorkspace::read_clipboard_chunk(const std::shared_ptr<ClipboardRead> &request) {
  request->stream->read_bytes_async(64 * 1024, [this, request](const Glib::RefPtr<Gio::AsyncResult> &result) {
    if (request->lifetime.expired()) return;
    try {
      auto bytes = request->stream->read_bytes_finish(result);
      if (request->cancel->is_cancelled()) return;
      gsize size{}; const auto *data = static_cast<const char *>(bytes->get_data(size));
      if (request->bytes.size() + size > application::clipboard_limit)
        throw std::runtime_error("Clipboard excede 16 MiB; refine a seleção. Documento não alterado");
      if (size) { request->bytes.append(data, size); read_clipboard_chunk(request); return; }
      const auto buffer = editor_.get_buffer();
      const auto cursor = static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_insert()).get_offset());
      const auto bound = static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset());
      if (!current_document_ || request->document_id != current_document_->id || request->epoch != edit_epoch_ ||
          request->revision != edit_revision_ || request->generation != clipboard_generation_ ||
          cursor != request->selection.cursor || bound != request->selection.bound) {
        signal_status_message_.emit("Colagem cancelada: o Documento ou a seleção mudou durante a leitura."); return;
      }
      EditorPhaseTimer timer("clipboard_paste_gtk");
      const bool formatted = request->styles && request->mime == application::clipboard_mime;
      const auto fragment = formatted ? application::decode_fragment(request->bytes)
                                      : application::TextFragment{request->bytes, {}};
      const auto before = capture_edit_state();
      auto after = application::paste_fragment(before, std::min(cursor, bound), std::max(cursor, bound), fragment, formatted);
      if (apply_edit_transaction(std::move(after), formatted ? "Colar com estilos" : "Colar texto"))
        signal_status_message_.emit(formatted ? "Texto e estilos colados; nenhum vínculo importado" : "Texto colado; nenhum vínculo ou estilo de origem importado");
    } catch (const Glib::Error &) {
      if (!request->cancel->is_cancelled()) signal_status_message_.emit("Falha na leitura do clipboard; Documento não alterado.");
    } catch (const std::exception &error) { show_error("Não foi possível colar", error); }
  }, request->cancel);
}
}
