// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Spacer widget for extensions
 *//*
 * Authors:
 *   Patrick Storz <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2019 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INK_EXTENSION_WIDGET_SPACER_H
#define SEEN_INK_EXTENSION_WIDGET_SPACER_H

#include "widget.h"

#include <glibmm/ustring.h>

class QWidget;

namespace Inkscape {

namespace Xml {
class Node;
} // namespace Xml

namespace Extension {

/** \brief  A separator widget */
class WidgetSpacer : public InxWidget {
public:
    WidgetSpacer(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext);

    QWidget* get_widget(sigc::signal<void ()>* changeSignal) override;

private:
    /** size of the spacer in px */
    int _size = GUI_BOX_MARGIN;

    /** should the spacer be flexible and expand? */
    bool _expand = false;
};

} // namespace Extension
} // namespace Inkscape

#endif /* SEEN_INK_EXTENSION_WIDGET_SPACER_H */
