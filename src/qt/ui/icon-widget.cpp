// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * IconWidget — lightweight widget that displays a QIcon and re-renders
 * automatically when the pixmap cache is cleared (e.g. on theme switch).
 */

#include "icon-widget.h"

#include <QEvent>
#include <QPainter>

namespace Linea::UI {

IconWidget::IconWidget(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

IconWidget::IconWidget(const QIcon& icon, QWidget* parent)
    : IconWidget(parent) {
    _icon = icon;
}

IconWidget::IconWidget(const QIcon& icon, const QSize& iconSize, QWidget* parent)
    : IconWidget(parent) {
    _icon = icon;
    _iconSize = iconSize;
}

void IconWidget::setIcon(const QIcon& icon) {
    _icon = icon;
    update();
}

void IconWidget::setIconSize(const QSize& size) {
    if (_iconSize == size) return;

    _iconSize = size;
    updateGeometry();
    update();
}

QSize IconWidget::sizeHint() const {
    return _iconSize;
}

void IconWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    if (_icon.isNull()) return;

    QPainter painter(this);
    // Centre the icon rect within the widget
    QRect iconRect(0, 0, _iconSize.width(), _iconSize.height());
    iconRect.moveCenter(rect().center());

    const auto mode = isEnabled() ? QIcon::Normal : QIcon::Disabled;
    const auto pixmap = _icon.pixmap(_iconSize, devicePixelRatioF(), mode, QIcon::Off);
    painter.drawPixmap(iconRect, pixmap);
}

void IconWidget::changeEvent(QEvent* event) {
    // QPixmapCache::clear() is called during theme switches; Qt follows it
    // with ApplicationPaletteChange.  Trigger a repaint so the icon is
    // re-rendered from its source with the new theme colours.
    if (event->type() == QEvent::ApplicationPaletteChange) {
        update();
    }
    QWidget::changeEvent(event);
}

} // namespace Linea::UI
