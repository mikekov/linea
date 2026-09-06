#include "action-registry.h"
#include "action-meta.h"
#include "ui/shortcut-manager.h"
#include <QAction>
#include <QActionGroup>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "linea-window.h"

ActionRegistry& ActionRegistry::get() {
    static ActionRegistry instance;
    return instance;
}

void ActionRegistry::registerGroup(const ActionGroup& group) {
    _groups.push_back(&group);
}

void ActionRegistry::registerAction(const std::string& id, QAction* action) {
    auto it = _actionMap.find(id);
    if (it != _actionMap.end()) {
        // Action already exists
        throw std::runtime_error("Action already exists: " + id);
    }
    _actionMap[id] = action;
}

QAction* ActionRegistry::createActionBase(const char* id, const char* label, const char* icon_name,
                                          const char* tooltip) {
    auto action = new QAction(label, this);
    action->setObjectName(id);
    if (icon_name) {
        action->setIcon(QIcon(":/icons/" + QString::fromUtf8(icon_name)));
    }
    if (tooltip) {
        action->setToolTip(tooltip);
    }
    ShortcutManager::instance().applyTo(action, QString::fromUtf8(id));
    return action;
}

std::vector<const ActionGroup*> ActionRegistry::allGroups() const {
    return _groups;
}

const ActionGroup* ActionRegistry::findGroup(const char* id) const {
    for (const auto group : _groups) {
        if (group->id && strcmp(group->id, id) == 0) {
            return group;
        }
    }
    return nullptr;
}

std::vector<ActionRegistry::Entry> ActionRegistry::allActionsBySection() const {
    std::vector<Entry> result;
    for (const auto group : _groups) {
        for (const auto& def : group->actions) {
            result.push_back({group, &def.meta});
        }
    }
    return result;
}

QAction* ActionRegistry::action(const std::string& id) const {
    auto it = _actionMap.find(id);
    if (it == _actionMap.end()) {
        std::cerr << "Action not found: " << id << std::endl;
        throw std::runtime_error("Action not found: " + id);
    }
    return it->second;
}

bool ActionRegistry::hasAction(const std::string& id) const {
    return _actionMap.find(id) != _actionMap.end();
}

void ActionRegistry::syncAllActions() {
    for (auto& [action, state_query] : _stateQueries) {
        if (action->isCheckable()) {
            action->setChecked(state_query());
        }
    }
}

void ActionRegistry::setupDualLabel(QAction* action, const char* checked_label) {
    auto unchecked_text = action->text();
    auto checked_text = QString::fromUtf8(checked_label);
    QObject::connect(action, &QAction::toggled, action, [action, unchecked_text, checked_text](bool on) {
        action->setText(on ? checked_text : unchecked_text);
    });
    action->setText(action->isChecked() ? checked_text : unchecked_text);
}
