// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_MESSAGE_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_MESSAGE_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Authors:
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <glib.h>
#include <QLabel>
#include "live_effects/parameter/parameter.h"

namespace Inkscape {

namespace LivePathEffect {

class MessageParam : public Parameter {
public:
    MessageParam( const Glib::ustring& label,
               const Glib::ustring& tip,
               const Glib::ustring& key,
               Inkscape::UI::Widget::Registry* wr,
               Effect* effect,
               const gchar * default_message = "Default message",
               Glib::ustring  legend = "Use Label",
               Qt::Alignment halign = Qt::AlignLeft,
               Qt::Alignment valign = Qt::AlignVCenter,
               double marginstart = 6,
               double marginend = 6);
    ~MessageParam() override = default;

    QWidget* param_newWidget() override;
    bool param_readSVGValue(const gchar * strvalue) override;
    void param_update_default(const gchar * default_value) override;
    Glib::ustring param_getSVGValue() const override;
    Glib::ustring param_getDefaultSVGValue() const override;

    void param_setValue(const gchar * message);

    void param_set_default() override;
    void param_set_min_height(int height);
    const gchar *  get_value() const { return defmessage.c_str(); };
    ParamType paramType() const override { return ParamType::MESSAGE; };
private:
    QLabel* _label;
    int _min_height;
    MessageParam(const MessageParam&) = delete;
    MessageParam& operator=(const MessageParam&) = delete;
    Glib::ustring defmessage;
    Glib::ustring _legend;
    Qt::Alignment _halign;
    Qt::Alignment _valign;
    double _marginstart;
    double _marginend;
};

} //namespace LivePathEffect

} //namespace Inkscape

#endif
