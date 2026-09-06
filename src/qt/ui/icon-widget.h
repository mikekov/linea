// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * IconWidget — lightweight widget that displays a QIcon and re-renders
 * automatically when the pixmap cache is cleared (e.g. on theme switch).
 */

#ifndef LINEA_UI_ICON_WIDGET_H
#define LINEA_UI_ICON_WIDGET_H

#include <QIcon>
#include <QSize>
#include <QWidget>

namespace Linea::UI {

/**
 * A lightweight widget that displays a QIcon.
 *
 * Unlike a QLabel with a pixmap, this widget holds a QIcon and asks it to
 * paint itself on every paint event.  When the application palette changes
 * (which happens right after QPixmapCache::clear() during a theme switch),
 * the widget schedules a repaint so that the icon is re-fetched from its
 * source and rendered in the new theme colours.
 */
class IconWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize)

public:
    explicit IconWidget(QWidget* parent = nullptr);
    explicit IconWidget(const QIcon& icon, QWidget* parent = nullptr);
    explicit IconWidget(const QIcon& icon, const QSize& iconSize, QWidget* parent = nullptr);
    ~IconWidget() override = default;

    QIcon icon() const { return _icon; }
    void setIcon(const QIcon& icon);

    QSize iconSize() const { return _iconSize; }
    void setIconSize(const QSize& size);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    QIcon _icon;
    QSize _iconSize{16, 16};
};

} // namespace Linea::UI

#endif // LINEA_UI_ICON_WIDGET_H
