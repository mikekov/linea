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

#include "widget-spacer.h"

#include <QWidget>
#include <QSizePolicy>

#include "xml/node.h"
#include "extension/extension.h"

namespace Inkscape::Extension {

WidgetSpacer::WidgetSpacer(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxWidget(xml, ext)
{
    // get size
    const char *size = xml->attribute("size");
    if (size) {
        _size = strtol(size, nullptr, 0);
        if (_size == 0) {
            if (!strcmp(size, "expand")) {
                _expand = true;
            } else {
                g_warning("Invalid value ('%s') for size spacer in extension '%s'", size, _extension->get_id());
            }
        }
    }
}

/** \brief  Create a label for the description */
QWidget *WidgetSpacer::get_widget(sigc::signal<void ()> *changeSignal)
{
    if (_hidden) {
        return nullptr;
    }

    auto const spacer = new QWidget();
    if (_expand) {
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    } else {
        spacer->setFixedSize(_size, _size);
    }

    return spacer;
}

} // namespace Inkscape::Extension
