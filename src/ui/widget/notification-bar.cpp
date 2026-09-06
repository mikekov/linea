// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * NotificationBar widget implementation.
 */

#include "notification-bar.h"

#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#include "qt/ui/icon-widget.h"
#include "ui_notification-bar.h"

namespace Linea::UI {

ShadowOpacityEffect::ShadowOpacityEffect(QObject* parent)
    : QGraphicsDropShadowEffect(parent) {
    setOffset(0, 1);
    setBlurRadius(16);
    setColor(QColor(0, 0, 0, 90));
}

void ShadowOpacityEffect::setOpacity(qreal opacity) {
    _opacity = opacity;
    update();
}

void ShadowOpacityEffect::draw(QPainter* painter) {
    painter->setOpacity(_opacity);
    QGraphicsDropShadowEffect::draw(painter);
    painter->setOpacity(1.0);
}

NotificationBar::NotificationBar(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::NotificationBar>()) {
    _ui->setupUi(this);

    _ui->containerWidget->setProperty("class", "overlay-panel");
    _ui->containerWidget->setProperty("rounded", true);

    // Single combined effect: drop shadow + animatable opacity on one widget.
    _effect = new ShadowOpacityEffect(this);
    _effect->setOpacity(1.0);
    setGraphicsEffect(_effect);

    _fadeAnimation = new QPropertyAnimation(_effect, "opacity", this);
    _fadeAnimation->setDuration(_fadeDuration);
    _fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(_fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (_effect->opacity() == 0.0) {
            hide();
            Q_EMIT dismissed();
        }
    });

    _timer = new QTimer(this);
    _timer->setSingleShot(true);
    connect(_timer, &QTimer::timeout, this, &NotificationBar::fadeOut);

    connect(_ui->closeButton, &QToolButton::clicked, this, [this]() { fadeOut(); });
}

NotificationBar::~NotificationBar() = default;

void NotificationBar::setText(const QString& title, const QString& message) {
    _ui->titleLabel->setText(title);
    _ui->messageLabel->setText(message);
    // Hide labels that have no content so the layout collapses cleanly.
    _ui->titleLabel->setVisible(!title.isEmpty());
    _ui->messageLabel->setVisible(!message.isEmpty());
}

void NotificationBar::setIcon(const QIcon& icon) {
    _ui->iconLabel->setIcon(icon);
    _ui->iconLabel->setVisible(!icon.isNull());
}

void NotificationBar::setSeverity(const QString& severity) {
    _ui->iconLabel->setProperty("severity", severity);
    // QSS background-color requires a styled-background widget.
    _ui->iconLabel->setAttribute(Qt::WA_StyledBackground, !severity.isEmpty());
    _ui->iconLabel->style()->unpolish(_ui->iconLabel);
    _ui->iconLabel->style()->polish(_ui->iconLabel);
    _ui->iconLabel->update();
}

void NotificationBar::setTimeout(int msecs) {
    _timeout = msecs;
}

int NotificationBar::timeout() const {
    return _timeout;
}

void NotificationBar::setFadeDuration(int msecs) {
    _fadeDuration = msecs;
    _fadeAnimation->setDuration(_fadeDuration);
}

int NotificationBar::fadeDuration() const {
    return _fadeDuration;
}

void NotificationBar::showBar() {
    resetOpacity();
    show();
    raise();
    if (_timeout > 0) {
        _timer->start(_timeout);
    }
}

void NotificationBar::fadeOut() {
    _timer->stop();
    _fadeAnimation->setStartValue(_effect->opacity());
    _fadeAnimation->setEndValue(0.0);
    _fadeAnimation->start();
}

void NotificationBar::resetOpacity() {
    _fadeAnimation->stop();
    _effect->setOpacity(1.0);
}

void NotificationBar::hideEvent(QHideEvent* event) {
    _timer->stop();
    QWidget::hideEvent(event);
}

} // namespace Linea::UI
