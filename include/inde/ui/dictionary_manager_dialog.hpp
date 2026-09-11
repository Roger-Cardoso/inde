#pragma once

#include "inde/application/proofreading_service.hpp"

#include <gtkmm.h>

namespace inde::ui {

class ProofreadingController;

void show_dictionary_manager(Gtk::Window &owner,
                             application::ProofreadingService &service,
                             ProofreadingController &controller);

} // namespace inde::ui
