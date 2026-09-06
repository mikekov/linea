// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Reusable object picker button implementation.
 */

#include "object-picker-button.h"

#include <QIcon>
#include <QPushButton>
#include <QTimer>

#include "actions/actions-tools.h"
#include "desktop.h"
#include "inkscape.h"
#include "ui/tools/object-picker-tool.h"

namespace Linea::UI {

QPushButton* create_object_picker_button(std::function<void(SPObject*)> on_picked, const char* tooltip) {
    auto btn = new QPushButton();
    btn->setIcon(QIcon(":/icons/object-pick"));
    btn->setCheckable(true);
    btn->setToolTip(QObject::tr(tooltip ? tooltip : "Pick object on canvas"));

    auto picker_conn = new sigc::scoped_connection();
    auto switch_conn = new sigc::scoped_connection();

    // Clean up connections when button is destroyed.
    QObject::connect(btn, &QObject::destroyed, [picker_conn, switch_conn](QObject*) {
        picker_conn->disconnect();
        switch_conn->disconnect();
        delete picker_conn;
        delete switch_conn;
    });

    QObject::connect(btn, &QPushButton::toggled, [btn, on_picked, picker_conn, switch_conn](bool checked) {
        auto desktop = SP_ACTIVE_DESKTOP;
        if (!desktop) return;

        if (!checked) {
            picker_conn->disconnect();
            switch_conn->disconnect();
            return;
        }

        // Capture the current tool name before switching — tool_switch has a bug
        // where it sets last_active_tool to the new tool's own name, so we can't
        // rely on get_last_active_tool() to switch back.
        auto prev_tool = desktop->getActiveTool();
        if (prev_tool != "Picker") {
            set_active_tool(desktop, "Picker");
        }

        auto tool = dynamic_cast<Inkscape::UI::Tools::ObjectPickerTool*>(desktop->getTool());
        if (!tool) {
            btn->setChecked(false);
            return;
        }

        *picker_conn = tool->signal_object_picked.connect([btn, on_picked, picker_conn, switch_conn, desktop, prev_tool](SPObject* obj) {
            picker_conn->disconnect();
            switch_conn->disconnect();
            // Defer the callback so document modifications happen after tool switch.
            QTimer::singleShot(0, [on_picked, obj]() {
                if (on_picked) on_picked(obj);
            });
            // Switch back to the previous tool explicitly.
            if (desktop->getActiveTool() == "Picker") {
                set_active_tool(desktop, prev_tool);
            }
            QSignalBlocker blocker(btn);
            btn->setChecked(false);
            return false; // stop picking
        });

        *switch_conn = tool->signal_tool_switched.connect([btn, picker_conn, switch_conn]() {
            picker_conn->disconnect();
            switch_conn->disconnect();
            QSignalBlocker blocker(btn);
            btn->setChecked(false);
        });
    });

    return btn;
}

} // namespace Linea::UI
