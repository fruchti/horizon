#pragma once
#include <gtkmm.h>
#include <set>
#include "common/common.hpp"
#include "util/uuid.hpp"
#include "preview_base.hpp"
#include "generic_combo_box.hpp"
#include "util/paned_state_store.hpp"
#include <optional>

namespace horizon {
class PartPreview : public Gtk::Box, public PreviewBase {
public:
    PartPreview(class IPool &pool, bool show_goto = true, const std::string &instance = "");

    void load(const class Part *part);

private:
    class IPool &pool;
    const bool show_goto;
    const class Part *part = nullptr;
    class EntityPreview *entity_preview = nullptr;

    class PreviewCanvas *canvas_package = nullptr;
    GenericComboBox<UUID> *combo_package = nullptr;

    Gtk::Label *label_MPN = nullptr;
    Gtk::Label *label_manufacturer = nullptr;
    Gtk::Label *label_value = nullptr;
    Gtk::Label *label_description = nullptr;
    Gtk::Label *label_datasheet = nullptr;
    Gtk::Label *label_entity = nullptr;
    Gtk::Box *box_orderable_MPNs = nullptr;
    Gtk::Label *label_orderable_MPNs_title = nullptr;

    void handle_package_sel();
    std::optional<PanedStateStore> state_store;
};
} // namespace horizon
