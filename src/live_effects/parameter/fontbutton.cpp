// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "fontbutton.h"

#include <glibmm/i18n.h>

#include <QFontDialog>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

#include "libnrtype/font-qt-bridge.h"
#include "live_effects/effect.h"
#include "svg/stringstream.h"


namespace Inkscape {

namespace LivePathEffect {

FontButtonParam::FontButtonParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const Glib::ustring default_value )
    : Parameter(label, tip, key, wr, effect),
      value(default_value),
      defvalue(default_value)
{
}

void
FontButtonParam::param_set_default()
{
    param_setValue(defvalue);
}

void 
FontButtonParam::param_update_default(const gchar * default_value)
{
    defvalue = Glib::ustring(default_value);
}

bool
FontButtonParam::param_readSVGValue(const gchar * strvalue)
{
    Inkscape::SVGOStringStream os;
    os << strvalue;
    param_setValue((Glib::ustring)os.str());
    return true;
}

Glib::ustring
FontButtonParam::param_getSVGValue() const
{
    return value.c_str();
}

Glib::ustring
FontButtonParam::param_getDefaultSVGValue() const
{
    return defvalue;
}



QWidget*
FontButtonParam::param_newWidget()
{
    auto* btn = new QPushButton();
    btn->setText(QString::fromStdString(value.raw()));
    btn->setToolTip(QString::fromStdString(param_tooltip.raw()));

    QObject::connect(btn, &QPushButton::clicked, [this, btn]() {
        bool ok = false;
        auto current = Linea::pango_string_to_qfont(value);
        auto chosen = QFontDialog::getFont(&ok, current, btn);
        if (!ok) return;
        auto pango_str = Linea::qfont_to_pango_string(chosen);
        param_setValue(pango_str);
        param_write_to_repr(param_getSVGValue().c_str());
        btn->setText(QString::fromStdString(pango_str.raw()));
    });

    return btn;
}

void
FontButtonParam::param_setValue(const Glib::ustring newvalue)
{
    if (value != newvalue) {
        param_effect->refresh_widgets = true;
    }
    value = newvalue;
}


} /* namespace LivePathEffect */

} /* namespace Inkscape */
