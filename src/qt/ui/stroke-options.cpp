// SPDX-License-Identifier: GPL-2.0-or-later
//
// Stroke options widget (Qt version) — join, cap, miter limit, paint order.

#include "stroke-options.h"
#include "ui_stroke-options.h"

#include <QIcon>
#include <QSignalBlocker>

#include "paint-order-widget.h"
#include "props/binder.h"
#include "props/property-def.h"
#include "radio-toggle.h"
#include "style.h"
#include "style-enums.h"
#include "style-internal.h"
#include "util/style-utils.h"

namespace Linea::UI {

StrokeOptions::StrokeOptions(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::StrokeOptions>())
{
    _ui->setupUi(this);
    setupJoinCap();
    setupPaintOrder();
    connectSignals();
}

StrokeOptions::~StrokeOptions() = default;

void StrokeOptions::setupJoinCap() {
    // Icons and tooltips are set in the .ui file; only the enum→value mapping
    // is set here, where the symbolic constants are available.
    _joinToggle = _ui->joinToggle;
    _joinToggle->setButtonValues({
        SP_STROKE_LINEJOIN_BEVEL,
        SP_STROKE_LINEJOIN_ROUND,
        SP_STROKE_LINEJOIN_MITER,
    });

    _capToggle = _ui->capToggle;
    _capToggle->setButtonValues({
        SP_STROKE_LINECAP_BUTT,
        SP_STROKE_LINECAP_ROUND,
        SP_STROKE_LINECAP_SQUARE,
    });
}

void StrokeOptions::setupPaintOrder() {
    _paintOrder = _ui->paintOrder;
    _paintOrder->addOption(tr("Marker"), "paint-order-markers", tr("Arrows, markers and points"), SP_CSS_PAINT_ORDER_MARKER);
    _paintOrder->addOption(tr("Stroke"), "paint-order-stroke",  tr("The border line around the shape"), SP_CSS_PAINT_ORDER_STROKE);
    _paintOrder->addOption(tr("Fill"),   "paint-order-fill",    tr("The content of the shape"), SP_CSS_PAINT_ORDER_FILL);
}

void StrokeOptions::connectSignals() {
    // Join — RadioToggle emits the enum value; convert to keyword for the
    // old-style signal (used by StylePanel until it calls bind()).
    connect(_joinToggle, &RadioToggle::valueChanged, this, [this](int v) {
        if (_update.pending()) return;
        setMiterEnabled(v == SP_STROKE_LINEJOIN_MITER);
        const char* kw = "miter";
        if (v == SP_STROKE_LINEJOIN_ROUND) kw = "round";
        else if (v == SP_STROKE_LINEJOIN_BEVEL) kw = "bevel";
        Q_EMIT joinChanged(kw);
    });

    // Cap
    connect(_capToggle, &RadioToggle::valueChanged, this, [this](int v) {
        if (_update.pending()) return;
        const char* kw = "butt";
        if (v == SP_STROKE_LINECAP_ROUND) kw = "round";
        else if (v == SP_STROKE_LINECAP_SQUARE) kw = "square";
        Q_EMIT capChanged(kw);
    });

    // Miter limit
    connect(_ui->miterLimit, &NumberEdit::valueChanged, this, [this](double value) {
        if (_update.pending()) return;
        Q_EMIT miterChanged(value);
    });

    // Paint order
    connect(_paintOrder, &PaintOrderWidget::orderChanged, this, [this]() {
        if (_update.pending()) return;
        auto po = _paintOrder->getValue();
        auto value = po.get_value();
        Q_EMIT orderChanged(value.c_str());
    });
}

void StrokeOptions::setMiterEnabled(bool enabled) {
    _ui->miterLimit->setEnabled(enabled);
}

void StrokeOptions::updateWidgets(SPStyle& style) {
    auto scope(_update.block());

    _ui->miterLimit->setValue(style.stroke_miterlimit.value);

    _joinToggle->setValue(static_cast<int>(style.stroke_linejoin.value));
    setMiterEnabled(style.stroke_linejoin.value == SP_STROKE_LINEJOIN_MITER
                    && !style.stroke_extensions.hairline);

    _capToggle->setValue(static_cast<int>(style.stroke_linecap.value));

    SPIPaintOrder order;
    order.read(style.paint_order.set ? style.paint_order.value : "normal");
    bool has_markers = true; // TODO: detect from style
    _paintOrder->setValue(order, has_markers);
}

void StrokeOptions::updateWidgets(const Linea::PresentationState& props) {
    auto scope(_update.block());

    // miter limit
    if (props.stroke_miterlimit.is_single()) {
        _ui->miterLimit->setValue(props.stroke_miterlimit.value());
    }

    // line join
    if (props.stroke_linejoin.is_single()) {
        _joinToggle->setMixed(false);
        _joinToggle->setValue(props.stroke_linejoin.value());
        bool hairline = props.hairline.is_single() && props.hairline.value();
        setMiterEnabled(props.stroke_linejoin.value() == SP_STROKE_LINEJOIN_MITER
                        && !hairline && !props.stroke_miterlimit.is_mixed());
    } else {
        _joinToggle->setMixed(true);
    }

    // line cap
    if (props.stroke_linecap.is_single()) {
        _capToggle->setMixed(false);
        _capToggle->setValue(props.stroke_linecap.value());
    } else {
        _capToggle->setMixed(true);
    }

    // paint order — only update for uniform selections
    if (props.paint_order.is_single()) {
        bool has_markers = true; // TODO: detect from style
        _paintOrder->setValue(props.paint_order.value(), has_markers);
    }
}

void StrokeOptions::bind(Props::Binder& binder) {
    // Join and cap — direct two-way binding through WidgetTraits<RadioToggle>.
    // RadioToggle emits the enum value via valueChanged(int); the property
    // table stores int, so binder.bind() handles both directions.
    binder.bind(Props::stroke_linejoin, _joinToggle);
    binder.bind(Props::stroke_linecap,  _capToggle);

    // Miter limit — NumberEdit already has WidgetTraits<double>.
    binder.bind(Props::stroke_miterlimit, _ui->miterLimit);

    // Miter field is enabled when join is not uniformly non-miter and not
    // uniformly hairline. Enabled in mixed mode (editing applies the new
    // value to all items).
    binder.enableWhen(_ui->miterLimit, [](const Props::SelectionState& s) {
        if (s.style.stroke_linejoin.is_single() && s.style.stroke_linejoin.value() != SP_STROKE_LINEJOIN_MITER)
            return false;
        if (s.style.hairline.is_single() && s.style.hairline.value())
            return false;
        return true;
    });

    // Paint order — bespoke widget (reorderable drag stack, not an int toggle).
    // Read via bindField; write via editor->set() with the current SPIPaintOrder.
    binder.bindField(Props::Field::paint_order, [this](const Props::SelectionState& s) {
        if (_update.pending()) return;
        auto scope(_update.block());
        if (s.style.paint_order.is_single()) {
            bool has_markers = true; // TODO: this should be external input to the widget
            _paintOrder->setValue(const_cast<SPIPaintOrder&>(s.style.paint_order.value()), has_markers);
        }
    });
    binder.track(connect(_paintOrder, &PaintOrderWidget::orderChanged, this,
        [this, editor = binder.editor()]() {
            if (_update.pending()) return;
            editor->set(Props::paint_order, _paintOrder->getValue());
        }));
}

} // namespace Linea::UI
