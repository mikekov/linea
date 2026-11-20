// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * OperationsPopup implementation.
 */

#include "operations-popup.h"

#include <QHideEvent>
#include <QToolButton>

#include "actions/action-registry.h"

namespace Linea::UI {

OperationsPopup::OperationsPopup(QWidget* parent)
    : QWidget(parent) {
    // Shared tile/separator styling lives in styles.qss under this class —
    // any popup with a button-tile grid can opt in with the same class.
    setProperty("class", "tile-popup");
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void OperationsPopup::initTiles() {
    // Shared tile styling — QSS qproperty-* can't express enum/QSize values
    auto buttons = findChildren<QToolButton*>();
    ensurePolished();
    int tileWidth = 0;
    for (auto button : buttons) {
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize({24, 24});
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        tileWidth = std::max(tileWidth, button->sizeHint().width());
    }
    // Uniform tile size — grid columns otherwise size to their widest label
    for (auto button : buttons) {
        button->setMinimumWidth(tileWidth);
    }

    // Auto-connect via the "actionName" dynamic property
    auto& registry = ActionRegistry::get();
    for (auto button : buttons) {
        auto actionName = button->property("actionName").toString();
        if (actionName.isEmpty()) continue;
        connect(button, &QToolButton::clicked, this, [this, &registry, actionName] {
            registry.action(actionName.toUtf8().constData())->trigger();
            // The popup lives inside a QMenu — dismiss it after the op runs.
            if (auto top = window()) {
                top->hide();
            }
        });
    }
}

void OperationsPopup::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    // The menu hides under the cursor, so tiles never get a Leave event and
    // their autoRaise hover state sticks — clear it or a used button repaints
    // highlighted on next open.
    for (auto button : findChildren<QToolButton*>()) {
        button->setAttribute(Qt::WA_UnderMouse, false);
        button->setDown(false);
        button->update();
    }
}

} // namespace Linea::UI
