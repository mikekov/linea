// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ScaleBar — Simple scale widget that shows the range in discrete blocks (Qt version).
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_SCALE_BAR_H
#define LINEA_UI_SCALE_BAR_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QMouseEvent;
class QWheelEvent;
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * Simple scale widget that shows the range in discrete blocks.
 *
 * Supports mouse click and drag to set value, scroll wheel to adjust,
 * and customizable block count and block height.
 */
class ScaleBar : public QWidget {
    Q_OBJECT

public:
    explicit ScaleBar(QWidget* parent = nullptr);
    ~ScaleBar() override = default;

    void setMinimum(double min);
    void setMaximum(double max);
    void setValue(double value);
    void setRange(double min, double max);

    double minimum() const { return _minimum; }
    double maximum() const { return _maximum; }
    double value() const { return _value; }

    void setMaxBlockCount(int n);
    void setBlockHeight(int height);

    void setMixedMode(bool mixed);

Q_SIGNALS:
    void valueChanged(double value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void changeEvent(QEvent* event) override;
    QSize sizeHint() const override;

private:
    void drawScale(QPainter* painter, bool stripped);
    void setValueFromPosition(double x);
    QColor getSelectedColor() const;
    QColor getUnselectedColor() const;
    QBrush createStripeBrush(const QColor& color);

    double _minimum = 0.0;
    double _maximum = 100.0;
    double _value = 0.0;

    int _blockCount = 0;
    int _blockHeight = 10;
    bool _mixedMode = false;

    bool _isDragging = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_SCALE_BAR_H
