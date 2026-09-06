#ifndef LINEA_ACTION_REGISTRY_H
#define LINEA_ACTION_REGISTRY_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <QAction>
#include <QObject>
#include "action-meta.h"
#include "linea-window.h"
#include "ui/shortcut-manager.h"


class QAction;
class LineaWindow;

// Forward declarations
enum class ActionScope;
struct ActionDef;
struct ActionMeta;
struct ActionParamMeta;
struct ActionGroup;

// Action registry for UI discovery
class ActionRegistry : public QObject {
    Q_OBJECT
public:
    static ActionRegistry& get();

    void registerGroup(const ActionGroup& group);
    std::vector<const ActionGroup*> allGroups() const;
    const ActionGroup* findGroup(const char* id) const;

    // For tree view: returns all action metadata with their group
    struct Entry { const ActionGroup* group; const ActionMeta* meta; };
    std::vector<Entry> allActionsBySection() const;

    // Create and register actions (templates avoid std::function overhead)
    template<typename Meta, typename Callback>
    QAction* createAction(const Meta& meta, Callback callback);

    template<typename Callback, typename StateQuery>
    QAction* createBoolAction(const BoolActionMeta& meta, Callback callback, StateQuery state_query, bool initial = false);

    // create a toggle action that makes buttons show one of two different icons
    template<typename Callback, typename StateQuery>
    QAction* createToggleAction(const ActionParamMeta& meta, Callback callback, StateQuery state_query,
                                const char* checked_icon, bool initial = false);

    // Apply dual-label swapping to a bool action (called by createBoolAction).
    void setupDualLabel(QAction* action, const char* checked_label);

    // Sync all stateful actions (those with state query callbacks).
    // Updates checked states via setChecked — fires toggled (so buttons
    // update) but NOT triggered (so callbacks with side effects don't run).
    void syncAllActions();

    // get action ID; throws if not found
    QAction* action(const std::string& id) const;

    bool hasAction(const std::string& id) const;

    ActionRegistry(const ActionRegistry&) = delete;
    ActionRegistry& operator = (const ActionRegistry&) = delete;
private:
    ActionRegistry() = default;
    ~ActionRegistry() = default;

    void registerAction(const std::string& id, QAction* action);
    QAction* createActionBase(const char* id, const char* label, const char* icon_name, const char* tooltip);

    std::vector<const ActionGroup*> _groups;
    std::unordered_map<std::string, QAction*> _actionMap;
    std::unordered_map<QAction*, std::function<bool()>> _stateQueries;
};

template<typename Meta, typename Callback>
QAction* ActionRegistry::createAction(const Meta& meta, Callback callback) {
    auto action = createActionBase(meta.id, meta.label, meta.icon_name, meta.tooltip);
    QObject::connect(action, &QAction::triggered, this, [callback]() { callback(); });
    registerAction(meta.id, action);
    return action;
}

template<typename Callback, typename StateQuery>
QAction* ActionRegistry::createBoolAction(const BoolActionMeta& meta, Callback callback, StateQuery state_query, bool initial) {
    auto action = createActionBase(meta.id, meta.label, meta.icon_name, meta.tooltip);
    action->setCheckable(true);
    action->setChecked(initial);
    // Side-effect callback on triggered (user activation only), NOT toggled.
    // setChecked from syncAllActions fires toggled (buttons update) but not
    // triggered (no side-effect re-entry). After the callback runs, re-query
    // this action's own state and correct its checked state if the callback
    // didn't actually change the underlying state. Mutually-exclusive groups
    // are handled by QActionGroup (Qt unchecks the others automatically).
    QObject::connect(action, &QAction::triggered, this, [action, callback, state_query](bool checked) {
        callback(checked);
        action->setChecked(state_query());
    });
    if (meta.checked_label) {
        setupDualLabel(action, meta.checked_label);
    }
    registerAction(meta.id, action);
    _stateQueries[action] = state_query;
    return action;
}

template<typename Callback, typename StateQuery>
QAction* ActionRegistry::createToggleAction(const ActionParamMeta& meta,
                                             Callback callback, StateQuery state_query, const char* checked_icon,
                                             bool initial) {
    BoolActionMeta boolMeta = { meta.id, meta.label, meta.icon_name, meta.tooltip, nullptr };
    auto action = createBoolAction(boolMeta, callback, state_query, initial);
    action->setProperty("iconToggle", true);
    auto unchecked = action->icon();
    auto checked = QIcon(QString(":/icons/%1").arg(checked_icon));
    QObject::connect(action, &QAction::toggled, action, [action, unchecked, checked](bool on) {
        action->setIcon(on ? checked : unchecked);
    });
    if (initial) {
        action->setIcon(checked);
    }
    return action;
}

#endif
