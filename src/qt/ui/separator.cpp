// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Separator — thin horizontal line widget with a style-driven color.
 */

#include "separator.h"

#include <QPainter>
#include <QStyleOption>

namespace Linea::UI {

namespace {
// Vertical size of the widget in logical pixels; the painted line itself is
// only 1 device pixel tall and centered within this height.
constexpr int LINE_HINT_HEIGHT = 1;
} // namespace

Separator::Separator(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);
}

void Separator::setOpacity(qreal opacity) {
    opacity = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(_opacity, opacity)) {
        return;
    }
    _opacity = opacity;
    update();
}

QSize Separator::sizeHint() const {
    const QMargins m = contentsMargins();
    return QSize(m.left() + m.right(), m.top() + m.bottom() + LINE_HINT_HEIGHT);
}

void Separator::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setOpacity(_opacity);

    // Paint within contentsRect() so setContentsMargins is honored: the
    // line is centered in the area left after the margins are applied.
    const QRect cr = contentsRect();
    if (cr.height() <= 0) {
        return;
    }

    // Draw a 1 logical pixel tall line centered in the contents rect.
    const int y = cr.y() + (cr.height() - LINE_HINT_HEIGHT) / 2;
    painter.fillRect(QRectF(cr.x(), y, cr.width(), LINE_HINT_HEIGHT),
                     opt.palette.color(QPalette::WindowText));
}

} // namespace Linea::UI
