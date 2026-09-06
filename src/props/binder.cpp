// SPDX-License-Identifier: GPL-2.0-or-later

#include "props/binder.h"

namespace Linea::Props {

Binder::Binder(SelectionStateModel* model, Editor* editor)
    : _model(model)
    , _editor(editor) {
    _stateConn = _model->signalChanged().connect(sigc::mem_fun(*this, &Binder::onStateChanged));
}

Binder::~Binder() {
    // Widgets typically outlive the Binder (it is recreated on desktop
    // change); sever the widget -> editor connections so stale lambdas
    // capturing this Binder/Editor can never fire.
    for (auto& conn : _widgetConns) {
        QObject::disconnect(conn);
    }
}

void Binder::bindField(Field field, std::function<void(const SelectionState&)> refresh) {
    _bindings.emplace_back(field, false, std::move(refresh));
    auto scoped = _updating.block();
    _bindings.back().push(_model->state());
}

void Binder::switchTo(QStackedWidget* stack, std::vector<SwitchCase> cases) {
    // Reuse the blank fallback page if a prior switchTo on this stack
    // already created one; otherwise insert a new one at index 0.
    QWidget* blank = nullptr;
    for (int i = 0; i < stack->count(); ++i) {
        if (stack->widget(i)->property("_binderBlankPage").toBool()) {
            blank = stack->widget(i);
            break;
        }
    }
    if (!blank) {
        blank = new QWidget(stack);
        blank->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        blank->setProperty("_binderBlankPage", true);
        stack->insertWidget(0, blank);
    }

    // Ensure all case widgets are in the stack.
    for (auto& c : cases) {
        if (stack->indexOf(c.widget) < 0) {
            stack->addWidget(c.widget);
        }
    }

    _rules.emplace_back(SwitchRule{QPointer<QStackedWidget>(stack), QPointer<QWidget>(blank), std::move(cases)});

    // Apply immediately for the current state.
    applyRule(_rules.back(), _model->state());
}

void Binder::bind(const PropertyDef<UnitValue>& def, UnitEdit edit) {
    // model -> widget
    _bindings.emplace_back(def.field, true,
                           [edit, &def](const SelectionState& s) { WidgetTraits<UnitEdit>::push(edit, def.get(s)); });
    // widget -> document
    _widgetConns.emplace_back(WidgetTraits<UnitEdit>::on_changed(edit, [this, &def](UnitValue value) {
        if (!_updating.pending() && !_editor->busy()) {
            _echoField = def.field;
            _editor->set(def, std::move(value));
        }
    }));
    // initial sync
    auto scoped = _updating.block();
    _bindings.back().push(_model->state());
}

void Binder::onStateChanged(const SelectionState& state, const SelectionDelta& delta, unsigned origin_tags) {
    // Echo suppression: if this change originated from our own Editor, the
    // widgets already show the value the user just entered — re-pushing
    // mid-interaction causes caret jumps. Composition changes (counts) are
    // still applied.
    bool own_echo = (origin_tags & _editor->tag()) != 0;

    auto scoped = _updating.block();
    for (auto& b : _bindings) {
        if (delta.test(static_cast<size_t>(b.field))) {
            // Skip echo for the widget that triggered the change (avoids caret
            // jumps in interactive widgets), but still update display-only
            // bindField bindings (icons, previews) for the same field.
            if (own_echo && b.suppress_echo && _echoField == b.field) continue;

            b.push(state);
        }
    }
    // Rules may depend on any field, so re-evaluate on every change.
    applyRules(state);

    if (own_echo) {
        // clear the echo field so we don't suppress future changes
        _echoField.reset();
    }
}

void Binder::applyRule(const Rule& rule, const SelectionState& state) {
    std::visit(
        [&state](const auto& r) {
            using T = std::decay_t<decltype(r)>;
            if constexpr (std::is_same_v<T, VisibilityRule>) {
                if (r.target) r.target->setVisible(r.cond(state));
            } else if constexpr (std::is_same_v<T, EnablementRule>) {
                if (r.target) r.target->setEnabled(r.cond(state));
            } else if constexpr (std::is_same_v<T, SwitchRule>) {
                if (!r.stack) return;
                for (auto& c : r.cases) {
                    if (c.pred(state)) {
                        r.stack->setCurrentWidget(c.widget);
                        if (r.stack->isHidden()) r.stack->show();
                        return;
                    }
                }
                r.stack->setCurrentWidget(r.blank);
                if (!r.stack->isHidden()) r.stack->hide();
            }
        },
        rule);
}

void Binder::applyRules(const SelectionState& state) {
    for (auto& rule : _rules) {
        applyRule(rule, state);
    }
}

} // namespace Linea::Props
