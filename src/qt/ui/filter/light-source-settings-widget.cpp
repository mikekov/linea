// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Qt editor for SVG filter light source children. */

#include "light-source-settings-widget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>

#include "filter-enums.h"
#include "number-edit.h"
#include "object/filters/distantlight.h"
#include "object/filters/pointlight.h"
#include "object/filters/sp-filter-primitive.h"
#include "object/filters/spotlight.h"
#include "spin-scale.h"
#include "svg-attribute-format.h"
#include "xml/repr.h"

namespace Linea::UI {

namespace {
constexpr double coordinate_min = -99999.0;
constexpr double coordinate_max = 99999.0;

QString number(double value) {
    return Filter::format_number(value);
}

} // namespace

LightSourceSettingsWidget::LightSourceSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , _form(new QGridLayout(this))
    , _source(new QComboBox(this)) {
    _form->setContentsMargins(0, 0, 0, 0);
    _form->setHorizontalSpacing(4);
    _form->setVerticalSpacing(4);
    _form->setColumnMinimumWidth(0, 220);
    _form->setColumnStretch(1, 1);

    for (unsigned int i = 0; i < LightSourceConverter._length; ++i) {
        const auto& item = LightSourceConverter.data(i);
        _source->addItem(tr(item.label.c_str()), static_cast<int>(item.id));
    }
    _form->addWidget(new QLabel(tr("Light Source:"), this), 0, 0);
    _form->addWidget(_source, 0, 1);
    connect(_source, qOverload<int>(&QComboBox::currentIndexChanged), this, &LightSourceSettingsWidget::changeSource);
    _source->setCurrentIndex(-1);
}

void LightSourceSettingsWidget::setPrimitive(SPFilterPrimitive* primitive) {
    if (_writing.pending()) return;
    if (_primitive == primitive) {
        load();
        return;
    }
    _primitive = primitive;
    load();
}

void LightSourceSettingsWidget::clearForm() {
    for (int index = _form->count() - 1; index >= 0; --index) {
        auto item = _form->itemAt(index);
        if (item->widget() == _source || index == 0) continue;
        item = _form->takeAt(index);
        delete item->widget();
        delete item;
    }
}

void LightSourceSettingsWidget::load() {
    auto loadingBlock = _loading.block();
    clearForm();
    int type = -1;
    auto child = _primitive ? _primitive->firstChild() : nullptr;
    if (dynamic_cast<SPFeDistantLight*>(child))
        type = LIGHT_DISTANT;
    else if (dynamic_cast<SPFePointLight*>(child))
        type = LIGHT_POINT;
    else if (dynamic_cast<SPFeSpotLight*>(child))
        type = LIGHT_SPOT;

    {
        QSignalBlocker blocker(_source);
        _source->setCurrentIndex(_source->findData(type));
    }

    int row = 1;
    auto add_scalar = [this, &row](const QString& label, const char* attribute, double value, double min, double max,
                                   int decimals, double step, const QString& tip = QString()) {
        auto edit = new SpinScale(this);
        edit->setRange(min, max);
        edit->setDecimals(decimals);
        edit->setSingleStep(step);
        edit->setValue(value);
        edit->setToolTip(tip);
        _form->addWidget(new QLabel(label + QLatin1Char(':'), this), row, 0);
        _form->addWidget(edit, row++, 1);
        connect(edit, &SpinScale::valueChanged, this, [this, attribute](double v) { writeAttribute(attribute, v); });
    };
    auto add_coordinates = [this, &row](const QString& label, const char* x_name, const char* y_name,
                                        const char* z_name, double x, double y, double z) {
        auto container = new QWidget(this);
        auto grid = new QGridLayout(container);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(2);
        const char* names[] = {x_name, y_name, z_name};
        const double values[] = {x, y, z};
        for (int i = 0; i < 3; ++i) {
            auto edit = new NumberEdit(container);
            edit->setLabel(QString::fromLatin1("XYZ").mid(i, 1));
            edit->setRange(coordinate_min, coordinate_max);
            edit->setDecimals(1);
            edit->setSingleStep(1);
            edit->setValue(values[i]);
            grid->addWidget(edit, 0, i);
            connect(edit, &NumberEdit::valueChanged, this,
                    [this, name = names[i]](double v) { writeAttribute(name, v); });
        }
        _form->addWidget(new QLabel(label + QLatin1Char(':'), this), row, 0);
        _form->addWidget(container, row++, 1);
    };

    if (type == LIGHT_DISTANT) {
        auto light = static_cast<SPFeDistantLight*>(child);
        add_scalar(tr("Azimuth"), "azimuth", light->azimuth, 0, 360, 1, 1,
                   tr("Direction angle on the XY plane, in degrees."));
        add_scalar(tr("Elevation"), "elevation", light->elevation, 0, 360, 1, 1,
                   tr("Direction angle on the YZ plane, in degrees."));
    } else if (type == LIGHT_POINT) {
        auto light = static_cast<SPFePointLight*>(child);
        add_coordinates(tr("Location"), "x", "y", "z", light->x, light->y, light->z);
    } else if (type == LIGHT_SPOT) {
        auto light = static_cast<SPFeSpotLight*>(child);
        add_coordinates(tr("Location"), "x", "y", "z", light->x, light->y, light->z);
        add_coordinates(tr("Points at"), "pointsAtX", "pointsAtY", "pointsAtZ", light->pointsAtX, light->pointsAtY,
                        light->pointsAtZ);
        add_scalar(tr("Specular Exponent"), "specularExponent", light->specularExponent, 0.1, 100, 1, 0.1);
        add_scalar(tr("Cone Angle"), "limitingConeAngle", light->limitingConeAngle, 0, 180, 0, 5);
    }
}

void LightSourceSettingsWidget::writeAttribute(const char* attribute, double value) {
    if (_loading.pending() || _writing.pending() || !_primitive || !_primitive->getRepr()) return;

    auto child = _primitive->firstChild();
    if (!child) return;
    auto writingBlock = _writing.block();
    const auto text = number(value).toUtf8();
    child->setAttributeOrRemoveIfEmpty(attribute, text.constData());
    Q_EMIT primitiveChanged(_primitive, QString::fromLatin1(attribute), number(value));
}

void LightSourceSettingsWidget::changeSource(int index) {
    if (_loading.pending() || _writing.pending() || !_primitive || !_primitive->getRepr()) return;

    auto writingBlock = _writing.block();
    auto old = _primitive->firstChild();
    if (old) sp_repr_unparent(old->getRepr());

    const int sourceId = index >= 0 ? _source->currentData().toInt() : -1;
    if (sourceId >= 0 && LightSourceConverter.is_valid_id(static_cast<LightSource>(sourceId))) {
        const auto key = LightSourceConverter.get_key(static_cast<LightSource>(sourceId));
        auto repr = _primitive->document->getReprDoc()->createElement(key.c_str());
        _primitive->getRepr()->appendChild(repr);
        Inkscape::GC::release(repr);
    }
    load();
    Q_EMIT primitiveChanged(
        _primitive, QStringLiteral("lightSource"),
        sourceId >= 0 ? QString::fromLatin1(LightSourceConverter.get_key(static_cast<LightSource>(sourceId)).c_str())
                      : QString());
}

} // namespace Linea::UI
