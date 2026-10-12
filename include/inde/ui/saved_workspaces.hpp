#pragma once
#include "inde/ui/view_state.hpp"
#include <vector>
namespace inde::ui {
struct SavedWorkspace { std::string id, name, state; };
std::vector<SavedWorkspace> saved_workspaces(const std::string &project_id);
void save_workspace(const std::string &project_id, const SavedWorkspace &workspace);
void remove_workspace(const std::string &project_id, const std::string &id);
}
