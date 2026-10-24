// SPDX-License-Identifier: GPL-2.0-or-later
//
// Stroke options widget — style, pattern, offset, join, cap, miter limit, markers, paint order.

#include "stroke-options.h"
#include "ui_stroke-options.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>

#include "actions/actions-tools.h"
#include "dash-selector.h"
#include "desktop.h"
#include "document.h"
#include "icon-widget.h"
#include "marker-combo-box.h"
#include "paint-order-widget.h"
#include "popup-menu.h"
#include "props/binder.h"
#include "props/property-def.h"
#include "radio-toggle.h"
#include "style.h"
#include "style-enums.h"
#include "style-internal.h"
#include "ui/tools/marker-tool.h"
#include "util-string/context-string.h"
#include "util/paint-item-ops.h"
#include "util/style-utils.h"

using Linea::Util::new_css_attr;
using Linea::Util::PaintEditDelegate;

namespace Linea::UI {

StrokeOptions::StrokeOptions(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::StrokeOptions>())
{
    _ui->setupUi(this);

    auto resetPolicy = _ui->resetStyle->sizePolicy();
    resetPolicy.setRetainSizeWhenHidden(true);
    _ui->resetStyle->setSizePolicy(resetPolicy);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    setupJoinCap();
    setupMarkers();
    setupPaintOrder();
    connectSignals();
}

StrokeOptions::~StrokeOptions() = default;

void StrokeOptions::setDocument(SPDocument* doc) {
    _document = doc;

    for (auto combo : {_markerStart, _markerMid, _markerEnd}) {
        if (combo) {
            combo->setDocument(doc);
        }
    }
}

void StrokeOptions::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

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

void StrokeOptions::setupMarkers() {
    _markerStart = new MarkerComboBox("marker-start", SP_MARKER_LOC_START, _ui->markersPlaceholder);
    _markerMid = new MarkerComboBox("marker-mid", SP_MARKER_LOC_MID, _ui->markersPlaceholder);
    _markerEnd = new MarkerComboBox("marker-end", SP_MARKER_LOC_END, _ui->markersPlaceholder);

    auto markersLayout = new QHBoxLayout(_ui->markersPlaceholder);
    markersLayout->setContentsMargins(0, 0, 0, 0);
    markersLayout->setSpacing(2);
    markersLayout->addWidget(_markerStart);
    markersLayout->addWidget(_markerMid);
    markersLayout->addWidget(_markerEnd);

    // Apply segment button classes so the three marker buttons look connected.
    _markerStart->button()->setProperty("class", "segment-first");
    _markerMid->button()->setProperty("class", "segment-middle");
    _markerEnd->button()->setProperty("class", "segment-last");

    for (auto combo : {_markerStart, _markerMid, _markerEnd}) {
        connect(combo, &MarkerComboBox::editRequested, this, [this, combo]() {
            if (!_desktop) return;
            set_active_tool(_desktop, "Marker");
            if (auto mt = dynamic_cast<Inkscape::UI::Tools::MarkerTool*>(_desktop->getTool())) {
                mt->editMarkerMode = combo->getLoc();
                mt->selection_changed(_desktop->getSelection());
            }
        });
    }
}

void StrokeOptions::setupPaintOrder() {
    _paintOrder = new PaintOrderWidget(this);
    _paintOrder->addOption(tr("Marker"), "paint-order-markers", tr("Arrows, markers and points"), SP_CSS_PAINT_ORDER_MARKER);
    _paintOrder->addOption(tr("Stroke"), "paint-order-stroke",  tr("The border line around the shape"), SP_CSS_PAINT_ORDER_STROKE);
    _paintOrder->addOption(tr("Fill"),   "paint-order-fill",    tr("The content of the shape"), SP_CSS_PAINT_ORDER_FILL);

    _orderPopup = new PopupMenu(this);
    _orderPopup->setContent(_paintOrder);

    connect(_ui->orderButton, &QPushButton::clicked, this, [this]() {
        _orderPopup->showBelowWidget(_ui->orderButton);
    });
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

void StrokeOptions::updatePatternEntry(const std::vector<double>& pattern) {
    if (!_ui->patternEdit->hasFocus()) {
        QSignalBlocker b(_ui->patternEdit);
        _ui->patternEdit->setText(DashSelector::formatDashPattern(pattern));
    }
}

void StrokeOptions::applyDash(Props::Editor* editor, const std::vector<double>& pattern, double offset) {
    if (!editor) return;
    editor->apply(PaintEditDelegate::DashOp{pattern, offset}, "set-dash-pattern",
                  RC_("Undo", "Set stroke dash pattern"));
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
    auto editor = binder.editor();

    // Stroke dash — read-only in the property table (StrokeDashProp), so
    // bindField for read + binder.track() + editor->apply(DashOp) for write.
    binder.bindField(Props::Field::stroke_dash, [this](const Props::SelectionState& s) {
        if (_update.pending()) return;
        auto scope(_update.block());
        if (s.style.stroke_dash.is_single() || s.style.stroke_dash.is_mixed()) {
            auto& dash = s.style.stroke_dash.value();
            _ui->dashSelector->setDashPattern(dash.dashes, dash.offset);
            if (s.style.stroke_dash.is_mixed()) {
                _ui->dashSelector->setPlaceholder(tr("Mixed"));
                _ui->offsetEdit->setMixedMode(true);
                updatePatternEntry({});
            } else {
                _ui->dashSelector->setPlaceholder(QString());
                _ui->offsetEdit->setMixedMode(false);
                _ui->offsetEdit->setValue(dash.dashes.empty() ? 0.0 : dash.offset);
                updatePatternEntry(dash.dashes);
            }
        } else {
            _ui->dashSelector->setPlaceholder(QString());
            _ui->dashSelector->setDashPattern({}, 0.0);
            _ui->offsetEdit->setMixedMode(false);
            _ui->offsetEdit->setValue(0.0);
            updatePatternEntry({});
        }
    });

    binder.track(connect(_ui->dashSelector, &DashSelector::dashChanged, this, [this, editor]() {
        if (_update.pending()) return;
        applyDash(editor, _ui->dashSelector->getDashPattern(), _ui->dashSelector->getOffset());
    }));

    binder.track(connect(_ui->resetStyle, &QPushButton::clicked, this, [this, editor]() {
        if (_update.pending()) return;
        applyDash(editor, {}, 0.0);
    }));

    binder.track(connect(_ui->offsetEdit, &NumberEdit::valueChanged, this, [this, editor](double value) {
        if (_update.pending()) return;
        auto scope(_update.block());
        auto const& pattern = _ui->dashSelector->getDashPattern();
        _ui->dashSelector->setPlaceholder(QString());
        _ui->dashSelector->setDashPattern(pattern, value);
        applyDash(editor, pattern, value);
    }));

    binder.track(connect(_ui->patternEdit, &QLineEdit::textChanged, this, [this, editor](const QString& text) {
        if (_update.pending()) return;
        auto pattern = DashSelector::parseDashPattern(text);
        if (pattern.empty()) return;
        double offset = _ui->offsetEdit->value();
        _ui->dashSelector->setPlaceholder(QString());
        _ui->dashSelector->setDashPattern(pattern, offset);
        applyDash(editor, pattern, offset);
    }));

    // Pattern, Offset, and Reset controls are visible only when a non-solid
    // stroke style is selected (or in mixed state).
    auto nonSolidDash = [](const Props::SelectionState& s) {
        const auto& d = s.style.stroke_dash;
        if (d.is_unset()) return false;
        if (d.is_mixed()) return true;
        return !d.value().dashes.empty();
    };
    binder.visibleWhen(_ui->resetStyle, nonSolidDash);
    binder.visibleWhen(_ui->titlePattern, nonSolidDash);
    binder.visibleWhen(_ui->patternEdit, nonSolidDash);
    binder.visibleWhen(_ui->offsetLabel, nonSolidDash);
    binder.visibleWhen(_ui->offsetEdit, nonSolidDash);

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
        if (s.style.stroke_linejoin.is_single() && s.style.stroke_linejoin.value() != SP_STROKE_LINEJOIN_MITER) {
            return false;
        }
        if (s.style.hairline.is_single() && s.style.hairline.value()) {
            return false;
        }
        return true;
    });

    // Markers (start / mid / end) — read-only string fields; bindField for
    // read (URI → SPObject) + binder.track() + editor->apply(CssOp) for write.
    auto bindMarker = [this, &binder, editor](MarkerComboBox* combo, Props::Field field) {
        binder.bindField(field, [this, combo, field](const Props::SelectionState& s) {
            if (combo->inUpdate()) return;
            const auto prop = [&]() -> const mixed_property<std::string>* {
                switch (field) {
                    case Props::Field::marker_start:
                        return &s.style.marker_start;
                    case Props::Field::marker_mid:
                        return &s.style.marker_mid;
                    case Props::Field::marker_end:
                        return &s.style.marker_end;
                    default:
                        return nullptr;
                }
            }();
            if (!prop) return;

            SPObject* marker = nullptr;
            if (!prop->is_unset() && !prop->value().empty()) {
                marker = getMarkerObj(prop->value().c_str(), _document);
            }
            combo->setCurrent(marker);
        });

        binder.track(connect(combo, &MarkerComboBox::markerChanged, this, [this, combo, editor]() {
            if (!_document) return;
            auto [uri, id] = combo->getActiveMarkerUri();
            auto css = new_css_attr();
            sp_repr_css_set_property(css.get(), combo->getId().c_str(), uri.c_str());
            editor->apply(PaintEditDelegate::CssOp{css}, "marker-change", RC_("Undo", "Set marker"));
            if (auto marker = _document->getObjectById(id.c_str())) {
                combo->setCurrent(marker);
            }
        }));
    };
    bindMarker(_markerStart, Props::Field::marker_start);
    bindMarker(_markerMid, Props::Field::marker_mid);
    bindMarker(_markerEnd, Props::Field::marker_end);

    // Paint order — bespoke widget (reorderable drag stack, not an int toggle).
    // Read via bindField; write via editor->set() with the current SPIPaintOrder.
    binder.bindField(Props::Field::paint_order, [this](const Props::SelectionState& s) {
        if (_update.pending()) return;
        auto scope(_update.block());
        if (s.style.paint_order.is_single()) {
            bool has_markers = true; // TODO: this should be external input to the widget
            _paintOrder->setValue(const_cast<SPIPaintOrder&>(s.style.paint_order.value()), has_markers);

            auto layers = s.style.paint_order.value().get_layers();
            auto letter = [](SPPaintOrderLayer l) -> char {
                switch (l) {
                    case SP_CSS_PAINT_ORDER_STROKE:
                        return 's';
                    case SP_CSS_PAINT_ORDER_MARKER:
                        return 'm';
                    default:
                        return 'f';
                }
            };
            QString name =
                QString("paint-order-%1%2%3").arg(letter(layers[0])).arg(letter(layers[1])).arg(letter(layers[2]));
            _ui->orderIcon->setIcon(QIcon(QString(":/icons/") + name));
        }
    });
    binder.track(connect(_paintOrder, &PaintOrderWidget::orderChanged, this,
        [this, editor]() {
            if (_update.pending()) return;
            editor->set(Props::paint_order, _paintOrder->getValue());
        }));
}

} // namespace Linea::UI
