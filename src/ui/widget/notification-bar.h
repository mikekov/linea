// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * NotificationBar — a transient notification widget with fade-out support.
 */

#ifndef LINEA_UI_WIDGET_NOTIFICATION_BAR_H
#define LINEA_UI_WIDGET_NOTIFICATION_BAR_H

#include <QGraphicsDropShadowEffect>
#include <QHideEvent>
#include <QPropertyAnimation>
#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class NotificationBar;
}
QT_END_NAMESPACE

namespace Linea::UI {

/// Combined drop-shadow + opacity effect. QGraphicsDropShadowEffect already
/// renders the source into a pixmap; we intercept draw() to apply an opacity
/// modifier to the painter before the source is composited. This avoids the
/// "one painter at a time" error from nesting two separate effects.
class ShadowOpacityEffect : public QGraphicsDropShadowEffect {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit ShadowOpacityEffect(QObject* parent = nullptr);

    qreal opacity() const { return _opacity; }
    void setOpacity(qreal opacity);

protected:
    void draw(QPainter* painter) override;

private:
    qreal _opacity = 1.0;
};

/**
 * A notification bar widget that displays two lines of rich text and
 * can fade out automatically or on demand.
 *
 * The widget uses a grid layout with an optional icon (left), two rich-text
 * lines (title + message), and a close button (right). Call setText() to
 * set the two lines, then show(). The widget fades itself out after a
 * configurable timeout via a ShadowOpacityEffect animation.
 */
class NotificationBar : public QWidget {
    Q_OBJECT

public:
    explicit NotificationBar(QWidget* parent = nullptr);
    ~NotificationBar() override;

    /// Set the two lines of rich text. Either may be empty.
    void setText(const QString& title, const QString& message);

    /// Set the icon shown to the left of the text.
    void setIcon(const QIcon& icon);

    /// Set the severity level, which drives the icon's background color.
    /// Valid values: "error", "warning", "info", "success". Pass an empty
    /// string to clear.
    void setSeverity(const QString& severity);

    /// Set how long the bar stays visible before fading out (milliseconds).
    /// A duration <= 0 disables auto-fade. Default is 4000 ms.
    void setTimeout(int msecs);
    int timeout() const;

    /// Set the fade-out animation duration in milliseconds. Default is 300.
    void setFadeDuration(int msecs);
    int fadeDuration() const;

    /// Show the bar (opaque) and start the auto-fade timer if enabled.
    void showBar();

    /// Immediately start fading out, then hide.
    void fadeOut();

Q_SIGNALS:
    void dismissed();

protected:
    void hideEvent(QHideEvent* event) override;

private:
    void resetOpacity();

    std::unique_ptr<Ui::NotificationBar> _ui;
    ShadowOpacityEffect* _effect = nullptr;
    QPropertyAnimation* _fadeAnimation = nullptr;
    QTimer* _timer = nullptr;
    int _timeout = 5000;
    int _fadeDuration = 300;
};

} // namespace Linea::UI

#endif // LINEA_UI_WIDGET_NOTIFICATION_BAR_H
