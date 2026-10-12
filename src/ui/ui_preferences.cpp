#include "inde/ui/ui_preferences.hpp"
#include <glib.h>
#include <filesystem>
namespace inde::ui {
namespace {
std::filesystem::path preference_path() {
  return std::filesystem::path(g_get_user_config_dir()) / "inde" / "interface.ini";
}
GKeyFile *load() {
  auto *file = g_key_file_new();
  GError *error{};
  static bool warned{};
  if(!g_key_file_load_from_file(file, preference_path().c_str(), G_KEY_FILE_NONE, &error) &&
      !g_error_matches(error,G_FILE_ERROR,G_FILE_ERROR_NOENT)) {
    if(!warned)g_warning("Interface preferences unreadable; original file retained: %s",error->message);
    warned=true;g_clear_error(&error);g_key_file_unref(file);return nullptr;
  }
  warned=false;g_clear_error(&error);
  return file;
}
}
std::string ui_preference(const std::string &group, const std::string &key,
                          const std::string &fallback) {
  auto *file = load();
  if(!file)return fallback;
  auto *value = g_key_file_get_string(file, group.c_str(), key.c_str(), nullptr);
  const std::string result = value ? value : fallback;
  g_free(value); g_key_file_unref(file);
  return result;
}
void set_ui_preference(const std::string &group, const std::string &key,
                       const std::string &value) noexcept {
  auto *file = load();
  if(!file)return;
  g_key_file_set_string(file, group.c_str(), key.c_str(), value.c_str());
  gsize size{};
  auto *data = g_key_file_to_data(file, &size, nullptr);
  const auto path = preference_path();
  GError *error{};
  if (g_mkdir_with_parents(path.parent_path().c_str(), 0700) == 0 &&
      !g_file_set_contents_full(path.c_str(), data, static_cast<gssize>(size),
          G_FILE_SET_CONTENTS_CONSISTENT, 0600, &error)) {
    g_warning("Unable to save interface preferences: %s", error->message);
    g_clear_error(&error);
  }
  g_free(data); g_key_file_unref(file);
}
std::string project_preference_group(const std::string &id) {
  auto *digest = g_compute_checksum_for_string(G_CHECKSUM_SHA256, id.c_str(), -1);
  const auto result = std::string("project-") + digest;
  g_free(digest); return result;
}
}
