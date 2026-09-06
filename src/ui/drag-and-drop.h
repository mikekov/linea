// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_DRAG_AND_DROP_H
#define INKSCAPE_UI_DRAG_AND_DROP_H

/**
 * @file
 * Drag and drop of drawings onto canvas.
 */

/* Authors:
 *
 * Copyright (C) Tavmjong Bah 2019
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <string>
#include <vector>
#include <glibmm/ustring.h>

namespace Gtk { class Widget; }
class SPDesktop;
class SPDesktopWidget;
class SPDocument;
namespace Geom { class Point; }

struct DnDSymbol
{
    // symbol's ID; may be reused in different symbol sets
    Glib::ustring id;
    // symbol's unique key (across symbol sets known to Inkscape at runtime)
    std::string unique_key;
    // symbol's document
    SPDocument* document = nullptr;
};

#ifndef WITH_QT6
void ink_drag_setup(SPDesktopWidget *dtw, Gtk::Widget *widget);
#endif

bool ink_drop_files(SPDesktop* desktop, std::vector<std::string> const& paths, Geom::Point const& dt_pos);

#endif // INKSCAPE_UI_DRAG_AND_DROP_H
