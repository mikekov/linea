#ifndef LINEA_ACTION_REGISTRY_H
#define LINEA_ACTION_REGISTRY_H

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <QAction>
#include <QActionGroup>
#include <QObject>
#include "action-meta.h"
#include "linea-application.h"
#include "linea-window.h"
#include "ui/shortcut-manager.h"


class QAction;
class LineaWindow;

// Forward declarations
enum class ActionScope;
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

    // Register all actions defined by a span of ActionSpec<Context>
    template <typename Context>
    void registerActions(LineaApplication* app, std::span<const ActionSpec<Context>> entries, bool radioGroup = false);

    // Register all actions defined by a fixed-size array of ActionSpec<Context>
    template <typename Context, std::size_t N>
    void registerActions(LineaApplication* app, const std::array<ActionSpec<Context>, N>& entries,
                         bool radioGroup = false);

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

namespace details {

template <typename>
inline constexpr bool dependent_false_v = false;

template <typename Context>
Context* active_context(LineaApplication* app) {
    if constexpr (std::same_as<Context, LineaApplication>) {
        return app;
    } else if constexpr (std::same_as<Context, LineaWindow>) {
        return app->get_active_window();
    } else if constexpr (std::same_as<Context, SPDocument>) {
        return app->get_active_document();
    } else if constexpr (std::same_as<Context, Inkscape::Selection>) {
        return app->get_active_selection();
    } else if constexpr (std::same_as<Context, SPDesktop>) {
        return app->get_active_desktop();
    } else {
        static_assert(dependent_false_v<Context>, "Unsupported action context");
        return nullptr;
    }
}

} // namespace details

template <typename Context>
void ActionRegistry::registerActions(
    LineaApplication* app,
    std::span<const ActionSpec<Context>> entries,
    bool radioGroup) {

    assert(app);
    if (!app) return;

    auto wnd = app->get_active_window();
    assert(wnd);
    if (!wnd) return;

    auto group = radioGroup ? new QActionGroup(wnd) : nullptr;

    for (const auto& entry : entries) {
        if (entry.state) {
            auto state_query = [app, state = entry.state]() {
                if (auto context = details::active_context<Context>(app)) {
                    return state(context);
                }
                return false;
            };
            auto initial = state_query();
            auto action = createBoolAction(
                {entry.id,
                 entry.label,
                 entry.tooltip,
                 entry.icon_name,
                 entry.checked_label},

                [app, fn = entry.callback](bool) {
                    if (auto context = details::active_context<Context>(app)) {
                        fn(context);
                    }
                },

                state_query,
                initial);

            wnd->addAction(action);
            if (group) group->addAction(action);
        } else {
            auto action = createAction(
                entry,
                [app, fn = entry.callback]() {
                    if (auto context = details::active_context<Context>(app)) {
                        fn(context);
                    }
                });

            wnd->addAction(action);
            if (group) group->addAction(action);
        }
    }
}

template <typename Context, std::size_t N>
void ActionRegistry::registerActions(
    LineaApplication* app,
    const std::array<ActionSpec<Context>, N>& entries,
    bool radioGroup) {
    registerActions<Context>(app, std::span<const ActionSpec<Context>>(entries), radioGroup);
}

#endif
