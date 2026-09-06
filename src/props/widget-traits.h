// SPDX-License-Identifier: GPL-2.0-or-later
//
// WidgetTraits: per-widget-type adapter used by Binder. Mixed/unset
// presentation is handled here ONCE per widget class, instead of once per
// property as in the old updateFromStyleState blocks.
//
// A specialization provides:
//   using value_type = T;
//   static void push(W*, const mixed_property<T>&);       // model -> widget
//   static QMetaObject::Connection
//          on_changed(W*, std::function<void(T)>);        // widget -> model
//
// on_changed returns the Qt connection so Binder can sever it on destruction
// (widgets typically outlive a Binder when the desktop changes).
//
// Widgets with bespoke presentation (paint buttons, marker combos) skip
// traits and bind with a custom lambda via Binder::bindField.

#ifndef LINEA_PROPS_WIDGET_TRAITS_H
#define LINEA_PROPS_WIDGET_TRAITS_H

#include <concepts>
#include <functional>
#include <string>

#include <QComboBox>
#include <QEvent>
#include <QPushButton>
#include <QSignalBlocker>

#include <QLineEdit>
#include <QPlainTextEdit>

#include "qt/ui/blend-mode-combo.h"
#include "qt/ui/icon-combobox.h"
#include "qt/ui/number-edit.h"
#include "qt/ui/radio-button-group.h"
#include "qt/ui/radio-toggle.h"
#include "qt/ui/spin-scale.h"
#include "qt/ui/unit-tracker.h"
#include "props/accessors.h"      // UnitValue
#include "util/mixed-property.h"
#include "util/text-utils.h"

namespace Linea::Props {

using Linea::mixed_property;

// Event-filter helper for widgets that lack an editingFinished signal
// (e.g. QPlainTextEdit). Fires the callback on focus-out. The filter is
// parented to the watched widget so it's deleted when the widget is.
class FocusOutCommitFilter : public QObject {
public:
    using Callback = std::function<void()>;
    FocusOutCommitFilter(QObject* parent, Callback cb)
        : QObject(parent), _cb(std::move(cb)) {}

protected:
    bool eventFilter(QObject* /*watched*/, QEvent* event) override {
        if (event->type() == QEvent::FocusOut && _cb) {
            _cb();
        }
        return false;
    }
private:
    Callback _cb;
};

template <typename W>
struct WidgetTraits;  // primary template intentionally undefined

template <typename W, typename T>
concept BindableWidget = requires(W* w, const mixed_property<T>& v, std::function<void(T)> f) {
    requires std::same_as<typename WidgetTraits<W>::value_type, T>;
    WidgetTraits<W>::push(w, v);
    { WidgetTraits<W>::on_changed(w, f) } -> std::same_as<QMetaObject::Connection>;
};

// --- NumberEdit --------------------------------------------------------------

template <>
struct WidgetTraits<UI::NumberEdit> {
    using value_type = double;

    static void push(UI::NumberEdit* w, const mixed_property<double>& v) {
        if (v.is_single()) {
            w->setMixedMode(false);
            w->setValue(v.value());
        } else {
            w->setMixedMode(true);  // mixed or unset -> placeholder
        }
    }

    static QMetaObject::Connection on_changed(UI::NumberEdit* w, std::function<void(double)> fn) {
        return QObject::connect(w, &UI::NumberEdit::valueChanged, w, std::move(fn));
    }
};

// --- UnitEdit (unit-aware NumberEdit wrapper) --------------------------------
//
// UnitEdit adapts a NumberEdit + UnitTracker pair to the Binder, converting
// between the model's UnitValue and the widget's display double. The strategy
// enum selects how the conversion works:
//
//   ConvertFromPx — model is always px; convert to the tracker's display unit
//                   on push, back to px on write. (font-size)
//   PreserveUnit  — model carries its own unit; sync the tracker to it on push,
//                   display the raw value. Write packs {value, tracker unit}.
//                   (line-height with em/ex/%/px)
//
// Usage:
//   binder.bind(Props::font_size,
//               UnitEdit{_ui->fontSize, _tracker_fs, UnitStrategy::ConvertFromPx});
//   binder.bind(Props::line_height,
//               UnitEdit{_ui->lineHeight, _tracker_lh, UnitStrategy::PreserveUnit});

enum class UnitStrategy {
    ConvertFromPx,  // model speaks px; convert to/from display unit
    PreserveUnit,   // model carries its own unit; sync tracker, no conversion
};

struct UnitEdit {
    UI::NumberEdit* widget;
    UI::UnitTracker* tracker;
    UnitStrategy strategy;
};

template <>
struct WidgetTraits<UnitEdit> {
    using value_type = UnitValue;

    static void push(const UnitEdit& w, const mixed_property<UnitValue>& v) {
        if (!v.is_single()) {
            w.widget->setMixedMode(true);
            return;
        }
        auto& prop = v.value();
        if (w.strategy == UnitStrategy::PreserveUnit) {
            w.tracker->setActiveUnitByAbbr(sp_style_get_css_unit_string(prop.unit));
        }
        double display = prop.value;
        if (w.strategy == UnitStrategy::ConvertFromPx) {
            if (auto* u = w.tracker->getActiveUnit()) {
                display = Inkscape::Util::Quantity::convert(prop.value, "px", u);
            }
        }
        w.widget->setMixedMode(false);
        w.widget->setValue(display);
    }

    static QMetaObject::Connection on_changed(const UnitEdit& w, std::function<void(UnitValue)> fn) {
        return QObject::connect(w.widget, &UI::NumberEdit::valueChanged, w.widget,
            [tracker = w.tracker, strategy = w.strategy, fn = std::move(fn)](double display) {
                auto* u = tracker->getActiveUnit();
                int css_unit = u ? Linea::unit_to_css_unit(u) : SP_CSS_UNIT_PX;
                if (strategy == UnitStrategy::ConvertFromPx) {
                    double px = u ? Inkscape::Util::Quantity::convert(display, u, "px") : display;
                    fn({px, SP_CSS_UNIT_PX});
                } else {
                    fn({display, css_unit});
                }
            });
    }
};

// --- RadioToggle -------------------------------------------------------------

template <>
struct WidgetTraits<UI::RadioToggle> {
    using value_type = int;

    static void push(UI::RadioToggle* w, const mixed_property<int>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            w->setValue(v.value());
        } else {
            w->setMixed(true);
        }
    }

    static QMetaObject::Connection on_changed(UI::RadioToggle* w, std::function<void(int)> fn) {
        return QObject::connect(w, &UI::RadioToggle::valueChanged, w, std::move(fn));
    }
};

// --- RadioButtonGroup --------------------------------------------------------

template <>
struct WidgetTraits<UI::RadioButtonGroup> {
    using value_type = int;

    static void push(UI::RadioButtonGroup* w, const mixed_property<int>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            w->setValue(v.value());
        } else {
            w->setMixed(true);
        }
    }

    static QMetaObject::Connection on_changed(UI::RadioButtonGroup* w, std::function<void(int)> fn) {
        return QObject::connect(w, &UI::RadioButtonGroup::valueChanged, w, std::move(fn));
    }
};

// --- SpinScale ---------------------------------------------------------------

template <>
struct WidgetTraits<UI::SpinScale> {
    using value_type = double;

    static void push(UI::SpinScale* w, const mixed_property<double>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            w->setMixedMode(false);
            w->setValue(v.value());
        } else {
            w->setMixedMode(true);
        }
    }

    static QMetaObject::Connection on_changed(UI::SpinScale* w, std::function<void(double)> fn) {
        return QObject::connect(w, &UI::SpinScale::valueChanged, w, std::move(fn));
    }
};

// --- IconComboBox ------------------------------------------------------------

template <>
struct WidgetTraits<UI::IconComboBox> {
    using value_type = int;

    static void push(UI::IconComboBox* w, const mixed_property<int>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            w->setActiveById(v.value());
        } else {
            w->setActiveById(-1);  // mixed/unset
        }
    }

    static QMetaObject::Connection on_changed(UI::IconComboBox* w, std::function<void(int)> fn) {
        return QObject::connect(w, &UI::IconComboBox::currentChanged, w, std::move(fn));
    }
};

// --- BlendModeCombo ----------------------------------------------------------

template <>
struct WidgetTraits<UI::BlendModeCombo> {
    using value_type = SPBlendMode;

    static void push(UI::BlendModeCombo* w, const mixed_property<SPBlendMode>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            w->setActiveById(v.value());
        } else {
            w->selectNone();  // mixed/unset
        }
    }

    static QMetaObject::Connection on_changed(UI::BlendModeCombo* w, std::function<void(SPBlendMode)> fn) {
        return QObject::connect(w, &UI::BlendModeCombo::modeChanged, w, std::move(fn));
    }
};

// --- QLineEdit -------------------------------------------------------------

template <>
struct WidgetTraits<QLineEdit> {
    using value_type = std::string;

    static void push(QLineEdit* w, const mixed_property<std::string>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            w->setText(QString::fromStdString(v.value()));
            w->setEnabled(true);
        } else if (v.is_mixed()) {
            w->setText("-");
            w->setEnabled(true);
        } else {
            w->clear();
            w->setEnabled(false);
        }
    }

    static QMetaObject::Connection on_changed(QLineEdit* w, std::function<void(std::string)> fn) {
        return QObject::connect(w, &QLineEdit::editingFinished, w, [w, fn = std::move(fn)]() {
            fn(w->text().toStdString());
        });
    }
};

// --- QPlainTextEdit ---------------------------------------------------------
// Like QLineEdit but for multiline text. Commits on focus-out since
// QPlainTextEdit has no editingFinished signal.

template <>
struct WidgetTraits<QPlainTextEdit> {
    using value_type = std::string;

    static void push(QPlainTextEdit* w, const mixed_property<std::string>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            w->setPlainText(QString::fromStdString(v.value()));
            w->setEnabled(true);
        } else if (v.is_mixed()) {
            w->setPlainText("-");
            w->setEnabled(true);
        } else {
            w->clear();
            w->setEnabled(false);
        }
    }

    static QMetaObject::Connection on_changed(QPlainTextEdit* w, std::function<void(std::string)> fn) {
        // Use a lambda capturing the last committed value so we only fire
        // on focus-out when the text actually changed.
        auto last = std::make_shared<QString>(w->toPlainText());
        w->installEventFilter(new FocusOutCommitFilter(w, [w, fn = std::move(fn), last]() {
            auto text = w->toPlainText();
            if (text != *last) {
                *last = text;
                fn(text.toStdString());
            }
        }));
        // Return a dummy connection; the filter owns the lifetime.
        return QMetaObject::Connection();
    }
};

// --- QComboBox (int-indexed enum/unit selector) ------------------------------
//
// Works for combos where each item maps to an int value via itemData.
// The caller sets up the items with setData(role, value) and the traits
// read/write via currentData(). Mixed/unset selects index -1.
//
// String-based combos (e.g. font family/style) should use bindField with
// a custom lambda, since their value_type differs.

template <>
struct WidgetTraits<QComboBox> {
    using value_type = int;

    static void push(QComboBox* w, const mixed_property<int>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            int index = w->findData(v.value());
            w->setCurrentIndex(index >= 0 ? index : -1);
        } else {
            w->setCurrentIndex(-1);
        }
    }

    static QMetaObject::Connection on_changed(QComboBox* w, std::function<void(int)> fn) {
        return QObject::connect(w, QOverload<int>::of(&QComboBox::currentIndexChanged), w,
            [w, fn = std::move(fn)](int) {
                fn(w->currentData().toInt());
            });
    }
};

// --- QPushButton (checkable toggle) ------------------------------------------

template <>
struct WidgetTraits<QPushButton> {
    using value_type = bool;

    static void push(QPushButton* w, const mixed_property<bool>& v) {
        const QSignalBlocker blocker(w);
        if (v.is_single()) {
            w->setChecked(v.value());
        } else {
            // mixed/unset: unchecked but visually indeterminate would be ideal;
            // Qt has no native tristate for QPushButton, so leave unchecked.
            w->setChecked(false);
        }
    }

    static QMetaObject::Connection on_changed(QPushButton* w, std::function<void(bool)> fn) {
        return QObject::connect(w, &QPushButton::toggled, w, std::move(fn));
    }
};

} // namespace Linea::Props

#endif // LINEA_PROPS_WIDGET_TRAITS_H
