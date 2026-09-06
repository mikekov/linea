// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "colorpicker.h"

#include <glibmm/i18n.h>

#include <QHBoxLayout>
#include <QLabel>

#include "qt/ui/color-button.h"

class SPDocument;

namespace Inkscape {
namespace LivePathEffect {

ColorPickerParam::ColorPickerParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, std::optional<Colors::Color> default_color)
    : Parameter(label, tip, key, wr, effect),
      value(default_color),
      defvalue(default_color)
{
}

void
ColorPickerParam::param_set_default()
{
    param_setValue(defvalue);
}

void 
ColorPickerParam::param_update_default(const gchar * default_value)
{
    defvalue->set(default_value ? default_value : "");
}

bool
ColorPickerParam::param_readSVGValue(const gchar *val)
{
    param_setValue(Colors::Color::parse(val));
    return true;
}

Glib::ustring
ColorPickerParam::param_getSVGValue() const
{
    return value->toString();
}

Glib::ustring
ColorPickerParam::param_getDefaultSVGValue() const
{
    return defvalue->toString();
}

QWidget*
ColorPickerParam::param_newWidget()
{
    auto colorbtn = new Linea::UI::ColorButton();
    if (value) {
        colorbtn->setColor(*value);
    }

    QObject::connect(colorbtn, &Linea::UI::ColorButton::colorChanged, [this](const Colors::Color &c) {
        param_setValue(c);
        param_write_to_repr(param_getSVGValue().c_str());
    });

    return colorbtn;
}

void
ColorPickerParam::param_setValue(std::optional<Colors::Color> newvalue)
{
    value = newvalue;
}

} /* namespace LivePathEffect */
} /* namespace Inkscape */
