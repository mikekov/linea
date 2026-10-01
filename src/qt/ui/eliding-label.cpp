// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ElidingLabel implementation.
 */

#include "eliding-label.h"

#include <QPainter>
#include <QResizeEvent>

#include "widget-utils.h"

namespace Linea::UI {

ElidingLabel::ElidingLabel(QWidget* parent)
    : QLabel(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void ElidingLabel::setOverflowMode(OverflowMode mode) {
    _mode = mode;
    if (_mode == OverflowMode::Elide) {
        updateElidedText();
    } else {
        setText(_fullText);
        update();
    }
}

void ElidingLabel::setFullText(const QString& text) {
    _fullText = text;
    if (_mode == OverflowMode::Elide) {
        updateElidedText();
    } else {
        setText(_fullText);
        update();
    }
}

const QString& ElidingLabel::fullText() const {
    return _fullText;
}

void ElidingLabel::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    if (_mode == OverflowMode::Elide) {
        updateElidedText();
    } else {
        update();
    }
}

void ElidingLabel::paintEvent(QPaintEvent* event) {
    if (_mode != OverflowMode::Fade) {
        QLabel::paintEvent(event);
        return;
    }

    auto metrics = fontMetrics();
    int textWidth = metrics.horizontalAdvance(_fullText);
    if (textWidth <= width()) {
        QLabel::paintEvent(event);
        return;
    }

    // Draw the full text left-aligned and clipped to the label rect,
    // with a horizontal alpha gradient fading out the right edge.
    QPainter p(this);
    paintFadedText(p, rect(), _fullText, palette().color(QPalette::WindowText),
                   Qt::AlignLeft | Qt::AlignVCenter);
}

void ElidingLabel::updateElidedText() {
    auto metrics = fontMetrics();
    setText(metrics.elidedText(_fullText, Qt::ElideRight, width()));
}

} // namespace Linea::UI
