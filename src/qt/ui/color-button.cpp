// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ColorButton — push button that shows a color swatch and presents a popup color picker.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "color-button.h"

#include <QColorDialog>
#include <QEvent>
#include <QPainter>

#include "qt/util/drawing-utils.h"

namespace Linea::UI {

ColorButton::ColorButton(QWidget* parent)
    : QPushButton(parent)
    , _color(Inkscape::Colors::Color(0))
    , _committedColor(Inkscape::Colors::Color(0))
    , _alpha(true)
{
    init();
}

ColorButton::ColorButton(const Inkscape::Colors::Color& color, bool alpha, QWidget* parent)
    : QPushButton(parent)
    , _color(color)
    , _committedColor(color)
    , _alpha(alpha)
{
    init();
}

void ColorButton::init() {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // setFixedHeight(30);

    connect(this, &QPushButton::clicked, this, &ColorButton::openPopup);
}

ColorButton::~ColorButton() {
    closePopup();
}

void ColorButton::setColor(const Inkscape::Colors::Color& color) {
    _color = color;
    _committedColor = color;
    update();
}

void ColorButton::paintEvent(QPaintEvent* event) {
    // Standard button chrome (border, background, focus ring, etc.)
    QPushButton::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRect(contentsRect());

    const int margin = 6;
    const QRect r = contentsRect().adjusted(margin, margin, -margin, -margin);
    if (r.isEmpty()) return;

    const int half = r.left() + r.width() / 2;
    auto qc = toQColor(_color);

    // Left half: fully opaque color
    QColor opaque = qc;
    opaque.setAlphaF(1.0);
    p.fillRect(QRect(r.left(), r.top(), half - r.left(), r.height()), opaque);

    // Right half: checkerboard under semi-transparent color
    QRect rightHalf(half, r.top(), r.right() - half, r.height());
    QBrush brush(checkerPattern(palette().color(QPalette::Window), 5));
    p.setBrushOrigin(rightHalf.topLeft());
    p.fillRect(rightHalf, brush);
    p.fillRect(rightHalf, qc);
    QPen pen(palette().mid(), 1, Qt::SolidLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);
}

void ColorButton::openPopup() {
    if (_dialog) return;

    _committedColor = _color;

    _dialog = new QColorDialog(toQColor(_color), this);
    _dialog->setWindowFlags(Qt::Popup);
    _dialog->setOption(QColorDialog::NoButtons);
    _dialog->setOption(QColorDialog::ShowAlphaChannel, _alpha);

    connect(_dialog, &QColorDialog::currentColorChanged, this, [this](const QColor& qc) {
        _color = fromQColor(qc);
        update();
        Q_EMIT colorChanging(_color);
    });

    _dialog->installEventFilter(this);
    _dialog->move(mapToGlobal(QPoint(0, height())));
    // _dialog->show();
    _dialog->exec();
}

bool ColorButton::eventFilter(QObject* obj, QEvent* event) {
    if (obj == _dialog && event->type() == QEvent::Hide) {
        if (!(_color == _committedColor)) {
            _committedColor = _color;
            Q_EMIT colorChanged(_color);
        }
        _dialog->deleteLater();
        _dialog = nullptr;
        return false;
    }
    return QPushButton::eventFilter(obj, event);
}

void ColorButton::closePopup() {
    if (_dialog) {
        _dialog->removeEventFilter(this);
        _dialog->hide();
        _dialog->deleteLater();
        _dialog = nullptr;
    }
}

QColor ColorButton::toQColor(const Inkscape::Colors::Color& color) const {
    auto rgba = color.toRGBA();
    return QColor(SP_RGBA32_R_U(rgba), SP_RGBA32_G_U(rgba),
                  SP_RGBA32_B_U(rgba), SP_RGBA32_A_U(rgba));
}

Inkscape::Colors::Color ColorButton::fromQColor(const QColor& qc) const {
    return Inkscape::Colors::Color(Inkscape::Colors::Space::Type::RGB,
        {qc.redF(), qc.greenF(), qc.blueF(), qc.alphaF()});
}

} // namespace Linea::UI
