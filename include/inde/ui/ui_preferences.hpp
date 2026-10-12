#pragma once
#include <string>
namespace inde::ui {
[[nodiscard]] std::string ui_preference(const std::string &group,
                                       const std::string &key,
                                       const std::string &fallback = {});
void set_ui_preference(const std::string &group, const std::string &key,
                       const std::string &value) noexcept;
[[nodiscard]] std::string project_preference_group(const std::string &id);
}
