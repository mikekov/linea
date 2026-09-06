// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Description widget for extensions
 *//*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl> *
 *   Patrick Storz <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2005-2019 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INK_EXTENSION_WIDGET_LABEL_H
#define SEEN_INK_EXTENSION_WIDGET_LABEL_H

#include "widget.h"

#include <glibmm/ustring.h>

class QWidget;

namespace Inkscape {

namespace Xml {
class Node;
} // namespace Xml

namespace Extension {

/** \brief  A label widget */
class WidgetLabel : public InxWidget {
public:
    enum AppearanceMode {
        DEFAULT, HEADER, URL
    };

    WidgetLabel(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext);

    QWidget* get_widget(sigc::signal<void ()>* changeSignal) override;
private:
    /** \brief  Internal value. */
    Glib::ustring _value;

    /** appearance mode **/
    AppearanceMode _mode = DEFAULT;
};

} // namespace Extension
} // namespace Inkscape

#endif /* SEEN_INK_EXTENSION_WIDGET_LABEL_H */
