// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Small shared helpers for Qt panel widgets.
 */

#include "widget-utils.h"

#include <QEvent>
#include <QLayout>
#include <QObject>
#include <QPointer>
#include <QWidget>

namespace Linea::UI {

namespace {

// Event filter installed on the leader widget; mirrors its Show/Hide state
// onto the follower. Parented to the follower so it dies with it.
class VisibilitySyncFilter : public QObject {
public:
    VisibilitySyncFilter(QWidget* follower, QWidget* leader)
        : QObject(follower)
        , _follower(follower)
        , _leader(leader) {
        leader->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* /*watched*/, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
            _follower->setVisible(_leader->isVisible());
        }
        return false;
    }

private:
    QWidget* _follower;
    QPointer<QWidget> _leader;
};

} // namespace

void applyHorizontalPadding(QWidget* widget, int left, int right) {
    if (!widget) return;
    if (auto layout = widget->layout()) {
        auto m = layout->contentsMargins();
        m.setLeft(left);
        m.setRight(right);
        layout->setContentsMargins(m);
    }
}

void syncVisibility(QWidget* follower, QWidget* leader) {
    if (!follower || !leader) return;
    follower->setVisible(leader->isVisible());
    new VisibilitySyncFilter(follower, leader);
}

} // namespace Linea::UI
