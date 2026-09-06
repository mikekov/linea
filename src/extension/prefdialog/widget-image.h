// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Image widget for extensions
 *//*
 * Authors:
 *   Patrick Storz <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2019 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INK_EXTENSION_WIDGET_IMAGE_H
#define SEEN_INK_EXTENSION_WIDGET_IMAGE_H

#include "widget.h"

#include <string>

class QWidget;

namespace Inkscape {

namespace Xml {
class Node;
} // namespace Xml

namespace Extension {

/** \brief  A label widget */
class WidgetImage : public InxWidget {
public:
    WidgetImage(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext);

    QWidget* get_widget(sigc::signal<void ()>* changeSignal) override;

private:
    /** \brief  Path to image file (relative paths are relative to the .inx file location). */
    std::string _image_path;
    std::string _icon_name;

    /** desired width of image when rendered on screen (in px) */
    unsigned int _width = 0;
    /** desired height of image when rendered on screen (in px) */
    unsigned int _height = 0;
};

} // namespace Extension
} // namespace Inkscape

#endif /* SEEN_INK_EXTENSION_WIDGET_IMAGE_H */
