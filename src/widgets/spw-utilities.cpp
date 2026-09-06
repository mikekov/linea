// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape Widget Utilities
 *
 * Authors:
 *   Bryce W. Harrington <brycehar@bryceharrington.org>
 *   bulia byak <buliabyak@users.sf.net>
 *
 * Copyright (C) 2003 Bryce W. Harrington
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "spw-utilities.h"

#include <glibmm/variant.h>
#include <gtkmm/actionable.h>
#include <gtkmm/widget.h>

Glib::ustring sp_get_action_target(Gtk::Widget* widget) {
    Glib::ustring target;

    if (auto const actionable = dynamic_cast<Gtk::Actionable *>(widget)) {
        if (auto const variant = actionable->get_action_target_value();
            variant && variant.get_type_string() == "s")
        {
            target = static_cast<Glib::Variant<Glib::ustring> const &>(variant).get();
        }
    }

    return target;
}
