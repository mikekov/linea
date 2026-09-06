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

#ifndef LINEA_UI_COLOR_BUTTON_H
#define LINEA_UI_COLOR_BUTTON_H

#include <QPushButton>
#include "colors/color.h"

class QColorDialog;

namespace Linea::UI {

/**
 * A button that displays a split color swatch (opaque | alpha over checkerboard)
 * and opens a popup color dialog on click.
 *
 * Usage:
 *   auto btn = new ColorButton(initialColor, true, parent);
 *   connect(btn, &ColorButton::colorChanging, this, &MyWidget::onLivePreview);
 *   connect(btn, &ColorButton::colorChanged,  this, &MyWidget::onColorCommit);
 */
class ColorButton : public QPushButton {
    Q_OBJECT

public:
    explicit ColorButton(QWidget* parent = nullptr);
    ColorButton(const Inkscape::Colors::Color& color, bool alpha = false, QWidget* parent = nullptr);
    ~ColorButton() override;

    const Inkscape::Colors::Color& color() const { return _color; }
    void setColor(const Inkscape::Colors::Color& color);

Q_SIGNALS:
    /// Emitted continuously while the user changes the color in the popup
    void colorChanging(const Inkscape::Colors::Color& color);

    /// Emitted once when the popup is dismissed (final committed color)
    void colorChanged(const Inkscape::Colors::Color& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void init();
    void openPopup();
    void closePopup();
    QColor toQColor(const Inkscape::Colors::Color& color) const;
    Inkscape::Colors::Color fromQColor(const QColor& qc) const;

    Inkscape::Colors::Color _color;
    Inkscape::Colors::Color _committedColor;
    bool _alpha;
    QColorDialog* _dialog = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_BUTTON_H
