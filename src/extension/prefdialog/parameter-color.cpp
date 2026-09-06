// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl>
 *   Christopher Brown <audiere@gmail.com>
 *   Jon A. Cruz <jon@joncruz.org>
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstdlib>
#include <string>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

#include "parameter-color.h"

#include "colors/manager.h"
#include "extension/extension.h"
#include "preferences.h"
#include "color-button.h"
#include "xml/node.h"

namespace Inkscape::Extension {

ParamColor::ParamColor(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
    , _colors(std::make_shared<Colors::ColorSet>())
{
    if (xml->firstChild()) {
        if (auto parsed = Colors::Color::parse(xml->firstChild()->content())) {
            _colors->set(*parsed);
        }
    }
    if (_colors->isEmpty()) {
        auto prefs = Preferences::get();
        _colors->set(prefs->getColor(pref_name()));
    }

    _color_changed = _colors->signal_changed.connect(sigc::mem_fun(*this, &ParamColor::_onColorChanged));

    // parse appearance
    if (_appearance) {
        if (!strcmp(_appearance, "colorbutton")) {
            _mode = COLOR_BUTTON;
        } else {
            g_warning("Invalid value ('%s') for appearance of parameter '%s' in extension '%s'",
                      _appearance, _name, _extension->get_id());
        }
    }
}

ParamColor::~ParamColor()
{
    _color_changed.disconnect();
}

QWidget* ParamColor::get_widget(sigc::signal<void ()>* changeSignal) {
    if (_hidden) {
        return nullptr;
    }

    if (changeSignal) {
        _changeSignal = std::make_unique<sigc::signal<void ()>>(*changeSignal);
    }

    auto const hbox = new QWidget();
    auto const layout = new QHBoxLayout(hbox);
    layout->setSpacing(GUI_PARAM_WIDGETS_SPACING);
    layout->setContentsMargins(0, 0, 0, 0);

    if (_mode == COLOR_BUTTON) {
        auto const label = new QLabel(QString::fromUtf8(_text));
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(label, 1);

        auto color = _colors->get();
        if (!color)
            color = Colors::Color::parse("magenta");
        _color_button = new Linea::UI::ColorButton(*color, true);
        layout->addWidget(_color_button, 0);

        QObject::connect(_color_button, &Linea::UI::ColorButton::colorChanged,
                         [this](Inkscape::Colors::Color const& c) { _onColorButtonChanged(c); });
    } else {
        // QT TODO: ColorNotebook not compiled in Qt port
        g_warning("ColorNotebook not compiled in Qt port");
        // Gtk::Widget *selector = Gtk::make_managed<Inkscape::UI::Widget::ColorNotebook>(_colors);
        // UI::pack_start(*hbox, *selector, true, true);
        // selector->set_visible(true);
    }

    return hbox;
}

void ParamColor::_onColorChanged()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (auto color = _colors->get()) {
        prefs->setColor(pref_name(), *color);

        if (_changeSignal)
            _changeSignal->emit();
    }
}

void ParamColor::_onColorButtonChanged(const Inkscape::Colors::Color& color) {
    set(color);
}

std::string ParamColor::value_to_string() const
{
    return get().toString(true);
}

void ParamColor::string_to_value(const std::string &in)
{
    // Parse color. If parsing fails, return black.
    static const auto black = Colors::Color(0, false);
    set(Colors::Color::parse(in).value_or(black));
}

} // namespace Inkscape::Extension
