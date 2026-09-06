// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl> *
 *   Jon A. Cruz <jon@joncruz.org>
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INK_EXTENSION_PARAMCOLOR_H
#define SEEN_INK_EXTENSION_PARAMCOLOR_H

#include <memory>
#include <string>
#include <sigc++/signal.h>
#include <sigc++/connection.h>

#include "colors/color.h"
#include "colors/color-set.h"
#include "parameter.h"

class QWidget;

namespace Linea::UI {
class ColorButton;
}

namespace Inkscape {

namespace XML {
class Node;
} // namespace XML

namespace Extension {

class ParamColor : public InxParameter {
public:
    enum AppearanceMode {
        DEFAULT, COLOR_BUTTON
    };

    ParamColor(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext);
    ~ParamColor() override;

    Colors::Color get() const { return _colors->getAverage(); } // copy
    void set(Colors::Color const &color) { _colors->set(color); }

    QWidget* get_widget(sigc::signal<void ()>* changeSignal) override;

    std::unique_ptr<sigc::signal<void ()>> _changeSignal;

    std::string value_to_string() const override;
    void string_to_value(const std::string &in) override;

private:
    void _onColorChanged();
    void _onColorButtonChanged(const Inkscape::Colors::Color& color);

    /** Internal value of this parameter */
    std::shared_ptr<Colors::ColorSet> _colors;

    sigc::connection _color_changed;

    Linea::UI::ColorButton* _color_button = nullptr;

    /** appearance mode **/
    AppearanceMode _mode = DEFAULT;
}; // class ParamColor

}  // namespace Extension

}  // namespace Inkscape

#endif // SEEN_INK_EXTENSION_PARAMCOLOR_H
