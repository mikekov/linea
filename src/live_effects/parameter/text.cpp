// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Maximilian Albert 2008 <maximilian.albert@gmail.com>
 *
 * Authors:
 *   Maximilian Albert
 *   Johan Engelen
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "text.h"

#include <2geom/sbasis-geometric.h>
#include <glibmm/i18n.h>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

#include "inkscape.h"

#include "display/control/canvas-item-text.h"
#include "live_effects/effect.h"

namespace Inkscape {
namespace LivePathEffect {

TextParam::TextParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const Glib::ustring default_value )
    : Parameter(label, tip, key, wr, effect)
    , value(default_value)
    , defvalue(default_value)
{
    if (SPDesktop *desktop = SP_ACTIVE_DESKTOP) { // FIXME: we shouldn't use this!
        canvas_text = make_canvasitem<CanvasItemText>(desktop->getCanvasTemp(), Geom::Point(0, 0), default_value);
    }
}

TextParam::~TextParam() = default;

void
TextParam::param_set_default()
{
    param_setValue(defvalue);
}

void
TextParam::param_update_default(const gchar * default_value)
{
    defvalue = (Glib::ustring)default_value;
}

// This is a bit silly, we should have an option in the constructor to not create the canvas_text object.
void
TextParam::param_hide_canvas_text()
{
    canvas_text.reset();
}

void
TextParam::setPos(Geom::Point pos)
{
    if (canvas_text) {
        canvas_text->set_coord(pos);
    }
}

void
TextParam::setPosAndAnchor(const Geom::Piecewise<Geom::D2<Geom::SBasis> > &pwd2,
                           const double t, const double length, bool /*use_curvature*/)
{
    using namespace Geom;

    Piecewise<D2<SBasis> > pwd2_reparam = arc_length_parametrization(pwd2, 2 , 0.1);
    double t_reparam = pwd2_reparam.cuts.back() * t;
    Point pos = pwd2_reparam.valueAt(t_reparam);
    Point dir = unit_vector(derivative(pwd2_reparam).valueAt(t_reparam));
    Point n = -rot90(dir);
    double angle = Geom::angle_between(dir, Point(1,0));
    if (canvas_text) {
        canvas_text->set_coord(pos + n * length);
        canvas_text->set_anchor(Geom::Point(std::sin(angle), -std::cos(angle)));
    }
}

void
TextParam::setAnchor(double x_value, double y_value)
{
    anchor_x = x_value;
    anchor_y = y_value;
    if (canvas_text) {
        canvas_text->set_anchor(Geom::Point(anchor_x, anchor_y));
    }
}

bool
TextParam::param_readSVGValue(const gchar * strvalue)
{
    param_setValue(strvalue);
    return true;
}

Glib::ustring
TextParam::param_getSVGValue() const
{
    return value;
}

Glib::ustring
TextParam::param_getDefaultSVGValue() const
{
    return defvalue;
}

void
TextParam::setTextParam(QLineEdit *rsu)
{
    Glib::ustring str(rsu->text().toStdString());
    param_setValue(str);
    param_write_to_repr(str.c_str());
}

QWidget *
TextParam::param_newWidget()
{
    auto container = new QWidget();
    auto layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto rsu = new QLineEdit();
    rsu->setText(QString::fromStdString(value.raw()));
    rsu->setToolTip(QString::fromStdString(param_tooltip.raw()));
    layout->addWidget(rsu, 1);

    auto set = new QPushButton();
    set->setIcon(QIcon(":/icons/check-mark"));
    layout->addWidget(set);

    QObject::connect(set, &QPushButton::clicked, [this, rsu]() {
        setTextParam(rsu);
    });

    return container;
}

void TextParam::param_setValue(Glib::ustring newvalue)
{
    if (value != newvalue) {
        param_effect->refresh_widgets = true;
    }
    value = std::move(newvalue);
    if (canvas_text) {
        canvas_text->set_text(value);
    }
}

} /* namespace LivePathEffect */
} /* namespace Inkscape */
