// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Separator widget for extensions
 *//*
 * Authors:
 *   Patrick Storz <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2019 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INK_EXTENSION_WIDGET_SEPARATOR_H
#define SEEN_INK_EXTENSION_WIDGET_SEPARATOR_H

#include "widget.h"

#include <glibmm/ustring.h>

class QWidget;

namespace Inkscape {
namespace Xml {
	class Node;
}

namespace Extension {

/** \brief  A separator widget */
class WidgetSeparator : public InxWidget {
public:
    WidgetSeparator(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext);

    QWidget* get_widget(sigc::signal<void ()>* changeSignal) override;
};

}  /* namespace Extension */
}  /* namespace Inkscape */

#endif /* SEEN_INK_EXTENSION_WIDGET_SEPARATOR_H */
