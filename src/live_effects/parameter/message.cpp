// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm/i18n.h>

#include <QLabel>
#include <QGroupBox>
#include <QHBoxLayout>

#include <utility>

#include "message.h"
#include "live_effects/effect.h"

namespace Inkscape {

namespace LivePathEffect {

MessageParam::MessageParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const gchar * default_message, Glib::ustring  legend, 
                      Qt::Alignment halign, Qt::Alignment valign, double marginstart, double marginend)
    : Parameter(label, tip, key, wr, effect),
      defmessage(default_message),
      _legend(std::move(legend)),
      _halign(halign),
      _valign(valign),
      _marginstart(marginstart),
      _marginend(marginend)
{
    if (_legend == Glib::ustring("Use Label")) {
        _legend.clear();
    }
    _label  = nullptr;
    _min_height = -1;
}

void
MessageParam::param_set_default()
{
    // do nothing
}

void 
MessageParam::param_update_default(const gchar * default_message)
{
    defmessage = default_message;
}

bool MessageParam::param_readSVGValue(const gchar *strvalue)
{
    if (g_strcmp0(strvalue, "")) {
        param_setValue(strvalue);
    } else {
        // do nothing if the strvalue is empty, stick to default value
    }
    return true;
}

Glib::ustring
MessageParam::param_getSVGValue() const
{
    return "";  // we dorn want to store messages in the SVG we store in LPE volatile
    // variables and get content with
    // param_getDefaultSVGValue() instead
}

Glib::ustring
MessageParam::param_getDefaultSVGValue() const
{
    return defmessage;
}

void
MessageParam::param_set_min_height(int height)
{
    _min_height = height;
    if (_label) {
        _label->setMinimumHeight(_min_height);
    }
}


QWidget*
MessageParam::param_newWidget()
{
    auto frame = new QGroupBox(QString::fromStdString(_legend.raw()));
    frame->setContentsMargins(static_cast<int>(_marginstart), 0,
                              static_cast<int>(_marginend), 0);

    _label = new QLabel("<small><i>" + QString::fromStdString(defmessage.raw()) + "</i></small>");
    _label->setWordWrap(true);
    _label->setAlignment(_halign | _valign);
    _label->setContentsMargins(static_cast<int>(_marginstart), 0,
                               static_cast<int>(_marginend), 0);
    if (_min_height > 0) {
        _label->setMinimumHeight(_min_height);
    }

    auto layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_label);
    frame->setLayout(layout);

    return frame;
}

void MessageParam::param_setValue(const gchar *strvalue)
{
    if (g_strcmp0(strvalue, defmessage.c_str())) {
        param_effect->refresh_widgets = true;
    }
    defmessage = strvalue;
}

} /* namespace LivePathEffect */

} /* namespace Inkscape */
