// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Interface for error handling.
 *
 * Copyright (C) 1999-2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_UI_ERROR_HANDLER_H
#define SEEN_UI_ERROR_HANDLER_H

// #include <gtkmm/messagedialog.h>

// #include "ui/dialog-run.h"

namespace Inkscape
{

class ErrorReporter {
public:
    explicit ErrorReporter(bool with_gui)
        : _with_gui(with_gui)
    {}

    void handleError(Glib::ustring const& primary, Glib::ustring const& secondary) const
    {
        if (_with_gui) {
#if 0 // Qt TODO
            Gtk::MessageDialog err(primary, false, Gtk::MessageType::WARNING, Gtk::ButtonsType::OK, true);
            err.set_secondary_text(secondary);
            Inkscape::UI::dialog_run(err);
#endif
        } else {
            g_message("%s", primary.data());
            g_message("%s", secondary.data());
        }
    }

private:
    bool _with_gui;
};

}

#endif // INKSCAPE_UI_ERROR_HANDLER_H
