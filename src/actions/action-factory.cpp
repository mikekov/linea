#include "action-factory.h"

namespace ActionFactory {

QAction* create(const ActionMeta& meta, QObject* parent) {
    QAction* action = new QAction(parent);
    action->setObjectName(meta.id);
    action->setProperty("actionId", meta.id);

    // Label and tooltip set by tr() at runtime
    action->setText(QString::fromUtf8(meta.label));
    if (meta.tooltip) {
        action->setToolTip(QString::fromUtf8(meta.tooltip));
    }

    if (meta.icon_name) {
        action->setIcon(QIcon(QString::fromUtf8(meta.icon_name)));
    }

    return action;
}

} // namespace ActionFactory
