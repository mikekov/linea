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

#include "scale-bar.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

namespace Linea::UI {

namespace {

constexpr int MIN_BLOCK_SIZE = 3;
constexpr int BLOCK_GAP = 1;

} // namespace

ScaleBar::ScaleBar(QWidget* parent)
    : QWidget(parent) {
    setObjectName("ScaleBar");
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(20); // Ensure minimum height
    setMouseTracking(true);
}

void ScaleBar::setMinimum(double min) {
    if (_minimum != min) {
        _minimum = min;
        update();
    }
}

void ScaleBar::setMaximum(double max) {
    if (_maximum != max) {
        _maximum = max;
        update();
    }
}

void ScaleBar::setValue(double value) {
    _mixedMode = false;
    value = qBound(_minimum, value, _maximum);
    if (!qFuzzyCompare(_value, value)) {
        _value = value;
        update();
        Q_EMIT valueChanged(_value);
    }
}

void ScaleBar::setRange(double min, double max) {
    setMinimum(min);
    setMaximum(max);
}

void ScaleBar::setMaxBlockCount(int n) {
    n = qBound(0, n, 1000);
    if (_blockCount != n) {
        _blockCount = n;
        update();
    }
}

void ScaleBar::setBlockHeight(int height) {
    if (_blockHeight != height) {
        _blockHeight = height;
        update();
    }
}

void ScaleBar::setMixedMode(bool mixed) {
    if (_mixedMode != mixed) {
        _mixedMode = mixed;
        update();
    }
}

void ScaleBar::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Force widget to have visible size
    if (height() < 1 || width() < 1) {
        return;
    }

    drawScale(&painter, _mixedMode);
}

void ScaleBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::EnabledChange) {
        update();
    }
    QWidget::changeEvent(event);
}

void ScaleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        _isDragging = true;
        setValueFromPosition(event->position().x());
    }
}

void ScaleBar::mouseMoveEvent(QMouseEvent* event) {
    if (_isDragging && (event->buttons() & Qt::LeftButton)) {
        setValueFromPosition(event->position().x());
    }
}

void ScaleBar::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        _isDragging = false;
    }
}

void ScaleBar::wheelEvent(QWheelEvent* event) {
    auto range = _maximum - _minimum;
    if (range <= 0) return;

    // growth direction: up or right
    auto delta = std::abs(event->angleDelta().x()) > std::abs(event->angleDelta().y()) ? -event->angleDelta().x()
                                                                                       : event->angleDelta().y();

    // Convert delta to value change
    delta *= range / 100.0;
    setValue(_value + delta);
}

QSize ScaleBar::sizeHint() const {
    return QSize(100, _blockHeight + 4);
}

QBrush ScaleBar::createStripeBrush(const QColor& color) {
    // Create a diagonal stripe pattern
    QPixmap stripePixmap(18, 18);
    stripePixmap.fill(Qt::transparent);

    QPainter stripePainter(&stripePixmap);
    stripePainter.setPen(QPen(color, 2.0));
    stripePainter.setRenderHint(QPainter::Antialiasing, true);

    // Draw diagonal lines
    for (int i = -9; i < 27; i += 6) {
        stripePainter.drawLine(i, 0, i + 18, 18);
    }

    stripePainter.end();
    return QBrush(stripePixmap);
}

void ScaleBar::drawScale(QPainter* painter, bool stripped) {
    const int width = this->width();
    const int height = this->height();

    if (width < MIN_BLOCK_SIZE || height < MIN_BLOCK_SIZE) return;

    auto range = _maximum - _minimum;
    if (range <= 0) return;

    auto selectedColor = getSelectedColor();
    auto unselectedColor = getUnselectedColor();

    auto padding = height > _blockHeight ? (height - _blockHeight) / 2 : 0;
    auto position = (_value - _minimum) / range;

    const int y = padding;
    const int blockHeight = height - 2 * padding;

    auto n = _blockCount;
    if (n > 1) {
        auto blockWidth = width / n - BLOCK_GAP;
        while (blockWidth < MIN_BLOCK_SIZE) {
            n /= 2;
            if (!n) return;
            blockWidth = width / n;
        }

        for (int i = 0; i < n; ++i) {
            double idx = i;
            auto x0 = idx * width / n;
            auto x1 = (idx + 1) * width / n;
            auto rect = QRectF(x0, y, x1 - x0 - 1, blockHeight);
            auto pos = (x0 + x1) / 2.0 / width;
            // draw blocks and block placeholders
            auto& color = position >= pos ? selectedColor : unselectedColor;

            if (stripped) {
                // Draw diagonal stripes
                painter->fillRect(rect, createStripeBrush(color));
            } else {
                painter->fillRect(rect, color);
            }
        }
    } else {
        // Continuous mode (n == 0 or n == 1)
        int x0 = 0;
        int len = width * position;
        // draw one block only
        if (position > 0) {
            // selected portion
            auto rect = QRect(x0, y, len, blockHeight);
            if (stripped) {
                painter->fillRect(rect, createStripeBrush(selectedColor));
            } else {
                painter->fillRect(rect, selectedColor);
            }
        }
        if (position < 1) {
            // gray bar
            auto rect = QRect(x0 + len, y, width - len, blockHeight);
            if (stripped) {
                painter->fillRect(rect, createStripeBrush(unselectedColor));
            } else {
                painter->fillRect(rect, unselectedColor);
            }
        }
    }
}

void ScaleBar::setValueFromPosition(double x) {
    x = qBound(0.0, x, static_cast<double>(width()));
    auto range = _maximum - _minimum;
    auto w = width();
    if (w <= 0) return;

    double value = x / w * range;
    if (_blockCount > 0) {
        double step = _blockCount > 1 ? range / _blockCount : 1.0;
        double mod = fmod(value, step);
        value -= mod;
        if (mod > step / 4) {
            value += step;
        }
    }

    setValue(value + _minimum);
}

QColor ScaleBar::getSelectedColor() const {
    auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    // When disabled, use a muted color instead of the accent/highlight
    return palette().color(group, QPalette::Accent);
}

QColor ScaleBar::getUnselectedColor() const {
    auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    return palette().color(group, QPalette::Mid);
}

} // namespace Linea::UI
