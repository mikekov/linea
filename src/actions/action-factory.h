#ifndef LINEA_ACTION_FACTORY_H
#define LINEA_ACTION_FACTORY_H

#include <QAction>
#include "action-meta.h"

namespace ActionFactory {

// Create action with metadata only. Caller wires the callback manually.
QAction* create(const ActionMeta& meta, QObject* parent);

// Create and wire a simple triggered callback
template<typename Func>
QAction* create(const ActionMeta& meta, QObject* parent, Func&& callback)
{
    QAction* action = create(meta, parent);
    QObject::connect(action, &QAction::triggered, action, std::forward<Func>(callback));
    return action;
}

// Create and wire a bool toggle callback
template<typename Func>
QAction* createBool(const ActionMeta& meta, QObject* parent, Func&& callback, bool initial = false)
{
    QAction* action = create(meta, parent);
    action->setCheckable(true);
    action->setChecked(initial);
    QObject::connect(action, &QAction::toggled, action, std::forward<Func>(callback));
    return action;
}

}

#endif
