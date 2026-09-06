// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Context menu
 *
 * Authors:
 *   Tavmjong Bah
 *
 * Copyright (C) 2022 Tavmjong Bah
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_CONTEXTMENU_H
#define SEEN_CONTEXTMENU_H
#include <QMenu>

class SPDesktop;
class SPDocument;
class SPObject;
class SPItem;

namespace Linea::UI {

/**
 * Implements the Inkscape context menu.
 */
class ContextMenu final : public QMenu
{
public:
    ContextMenu(SPDesktop *desktop, SPObject *object, std::vector<SPItem*> const &items_under_cursor, bool hide_layers_and_objects_menu_item = false);

private:
    // Used for unlock and unhide actions
    std::vector<SPItem *> items_under_cursor;
    void unhide_or_unlock(SPDocument* document, bool unhide);
};

} // namespace Linea::UI

#endif // SEEN_CONTEXT_MENU_H
