// SPDX-License-Identifier: GPL-2.0-or-later
//
// Binder: declarative glue between SelectionStateModel, widgets and an Editor.
// Replaces both the hand-written updateFromStyleState bodies (model -> UI)
// and the per-widget bindSource lambdas (UI -> document).
//
// Panel setup reads like configuration:
//
//   RectangleWidget::attach(Binder& b) {
//       b.bind(Props::rect_rx, _rxEdit);
//       b.bind(Props::rect_ry, _ryEdit);
//       b.visibleWhen(this, [](const SelectionState& s) {
//           const auto& c = s.element.count;
//           return c.items > 0 && c.rectangles == c.items;
//       });
//   }

#ifndef LINEA_PROPS_BINDER_H
#define LINEA_PROPS_BINDER_H

#include <QPointer>
#include <QStackedWidget>
#include <QWidget>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

#include "props/conditions.h"
#include "props/editor.h"
#include "props/selection-state-model.h"
#include "props/widget-traits.h"

namespace Linea::Props {

class Binder {
public:
    // Binder does not own model/editor; typical setup is one Binder per panel,
    // sharing the desktop's model and the panel group's Editor.
    Binder(SelectionStateModel* model, Editor* editor);
    ~Binder();

    // Two-way binding through WidgetTraits<W>.
    template <typename T, typename W>
        requires BindableWidget<W, T>
    void bind(const PropertyDef<T>& def, W* widget) {
        // model -> widget (runs only when this property's delta bit is set)
        _bindings.emplace_back(def.field, true,
                               [widget, &def](const SelectionState& s) { WidgetTraits<W>::push(widget, def.get(s)); });
        // widget -> document
        _widgetConns.emplace_back(WidgetTraits<W>::on_changed(widget, [this, &def](T value) {
            if (!_updating.pending() && !_editor->busy()) {
                _echoField = def.field; // remember who triggered the change, so we can skip it in the next update
                _editor->set(def, std::move(value));
            }
        }));
        // initial sync
        auto scoped = _updating.block();
        _bindings.back().push(_model->state());
    }

    // Unit-aware binding: UnitEdit is a value type (not a widget pointer),
    // so it gets a dedicated overload that captures it by value.
    void bind(const PropertyDef<UnitValue>& def, UnitEdit edit);

    Editor* editor() const { return _editor; }

    // Track an externally made Qt connection whose lambda captures this
    // Binder's Editor (e.g. buttons calling editor()->set() directly);
    // it is severed when the Binder is destroyed.
    void track(QMetaObject::Connection conn) { _widgetConns.emplace_back(std::move(conn)); }

    // Escape hatch for bespoke widgets (paint buttons, marker combos) and
    // read-only displays: refresh callback runs when `field`'s bit is dirty.
    // Writes for such widgets call editor->set()/apply() directly.
    void bindField(Field field, std::function<void(const SelectionState&)> refresh);

    // Visibility / enablement driven by the full selection snapshot;
    // re-evaluated on every state change (any dirty bit), so a predicate can
    // react to composition (counts) and to property values alike.
    //
    // Accepts any Cond::Condition — a Cond:: expression tree (fully inlined
    // inside the stored std::function) or a raw lambda (backward compatible).
    using StatePredicate = std::function<bool(const SelectionState&)>;
    template <Cond::Condition C>
    void visibleWhen(QWidget* widget, C cond) {
        _rules.emplace_back(VisibilityRule{QPointer<QWidget>(widget), StatePredicate(cond)});
        applyRule(_rules.back(), _model->state());
    }
    template <Cond::Condition C>
    void enableWhen(QWidget* widget, C cond) {
        _rules.emplace_back(EnablementRule{QPointer<QWidget>(widget), StatePredicate(cond)});
        applyRule(_rules.back(), _model->state());
    }

    // Stacked-widget page switching: evaluates the (condition, widget) pairs
    // in order and switches the stack to the first matching widget. If none
    // match, switches to a blank page (added automatically at index 0) and
    // hide the stack.
    //
    //   b.switchTo(_toolStack, {
    //       {Cond::hasSelection && Cond::toolIs<TOOLS_SELECT>, _selectWidget},
    //       {Cond::hasSelection && Cond::toolIs<TOOLS_NODES>,   _nodeWidget},
    //       {Cond::toolIs<TOOLS_ZOOM>,                          _zoomWidget},
    //   });
    struct SwitchCase {
        StatePredicate pred;
        QWidget* widget;
    };
    void switchTo(QStackedWidget* stack, std::vector<SwitchCase> cases);

private:
    struct Binding {
        Field field;
        bool suppress_echo = false; // true for bind() (WidgetTraits), false for bindField()
        std::function<void(const SelectionState&)> push;
    };
    struct VisibilityRule {
        QPointer<QWidget> target;
        StatePredicate cond;
    };
    struct EnablementRule {
        QPointer<QWidget> target;
        StatePredicate cond;
    };
    struct SwitchRule {
        QPointer<QStackedWidget> stack;
        QPointer<QWidget> blank;
        std::vector<SwitchCase> cases;
    };
    using Rule = std::variant<VisibilityRule, EnablementRule, SwitchRule>;

    void onStateChanged(const SelectionState& state, const SelectionDelta& delta, unsigned origin_tags);
    static void applyRule(const Rule& rule, const SelectionState& state);
    void applyRules(const SelectionState& state);

    SelectionStateModel* _model = nullptr;
    Editor* _editor = nullptr;
    std::optional<Field> _echoField;
    std::vector<Binding> _bindings;
    std::vector<Rule> _rules;
    std::vector<QMetaObject::Connection> _widgetConns;
    OperationBlocker _updating;
    sigc::scoped_connection _stateConn;
};

} // namespace Linea::Props

#endif // LINEA_PROPS_BINDER_H
