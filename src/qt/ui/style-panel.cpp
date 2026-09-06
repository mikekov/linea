// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * AppearancePanel — combined fill, stroke, opacity and blend mode editor
 */

#include "style-panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QSizePolicy>
#include <QStackedWidget>

#include "actions/actions-tools.h"
#include "color-preview.h"
#include "colors/utils.h"
#include "dash-selector.h"
#include "document-undo.h"
#include "document.h"
#include "icon-widget.h"
#include "marker-combo-box.h"
#include "number-edit.h"
#include "object/sp-gradient.h"
#include "object/sp-item.h"
#include "object/sp-namedview.h"
#include "object/sp-radial-gradient.h"
#include "object/sp-stop.h"
#include "paint-selector.h"
#include "pattern-manager.h"
#include "popup-menu.h"
#include "props/binder.h"
#include "props/property-def.h"
#include "spin-scale.h"
#include "stroke-options.h"
#include "ui/tools/marker-tool.h"
#include "ui/widget/paint-enums.h"
#include "ui_style-panel.h"
#include "unit-tracker.h"
#include "util-string/context-string.h"
#include "util/expression-evaluator.h"
#include "util/paint-item-ops.h"
#include "util/style-utils.h"
#include "util/units.h"

using Inkscape::Colors::Color;
using Inkscape::UI::Widget::PaintDerivedMode;
using Inkscape::UI::Widget::PaintMode;
using Inkscape::Util::Quantity;
using Inkscape::Util::UnitTable;
using Linea::Util::new_css_attr;
using Linea::Util::PaintEditDelegate;

namespace Linea::UI {

// --- PaintButton (paint preview button) ---

class StylePanel::PaintButton : public QPushButton {
public:
    PaintButton(QWidget* parent = nullptr);

    void setPreview(const Linea::mixed_property<Linea::PaintProp>& paint, const Linea::mixed_property<double>& opacity);

    void setFlatColor(const Color& color);

private:
    ColorPreview* _colorPreview = nullptr;
    IconWidget* _paintIcon = nullptr;
    QStackedWidget* _previewStack = nullptr;
    QWidget* _colorPage = nullptr;
};

StylePanel::PaintButton::PaintButton(QWidget* parent)
    : QPushButton(parent) {
    _colorPreview = new ColorPreview(0x808080ff, this);
    _colorPreview->setStyle(ColorPreview::Simple);
    _colorPreview->setFrame(true);
    _colorPreview->setBorderRadius(0);
    _colorPreview->setCheckerboardTileSize(4);
    _colorPreview->setMinimumSize(16, 12);
    _colorPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _previewStack = new QStackedWidget(this);

    _colorPage = new QWidget(_previewStack);
    auto colorLayout = new QHBoxLayout(_colorPage);
    colorLayout->setContentsMargins(5, 5, 5, 5);
    colorLayout->addWidget(_colorPreview);
    _previewStack->addWidget(_colorPage);
    _colorPage->setMinimumSize(26, 22);

    _paintIcon = new IconWidget(_previewStack);
    _paintIcon->setIconSize(QSize(16, 16));
    _previewStack->addWidget(_paintIcon);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_previewStack);
    setLayout(layout);
}

void StylePanel::PaintButton::setFlatColor(const Color& color) {
    _previewStack->setCurrentWidget(_colorPage);
    _colorPreview->setVisible(true);
    _colorPreview->setGradient({});
    _colorPreview->setRgba32(color.toRGBA());
    _colorPreview->setIndicator(ColorPreview::None);
}

void StylePanel::PaintButton::setPreview(const Linea::mixed_property<Linea::PaintProp>& paint,
                                         const Linea::mixed_property<double>& opacity) {
    Q_UNUSED(opacity)

    _previewStack->setCurrentWidget(_colorPage);

    if (paint.is_mixed()) {
        _colorPreview->setVisible(true);
        _colorPreview->setGradient({});
        _colorPreview->setImage({});
        _colorPreview->setIndicator(ColorPreview::MixedContent);
        return;
    }

    _colorPreview->setVisible(false);
    auto& p = paint.value();

    if (p.mode == PaintMode::None) return;

    if (p.mode == PaintMode::Derived) {
        const auto icon_name = get_paint_mode_icon(PaintMode::Derived);
        _paintIcon->setIcon(QIcon(":/icons/" + icon_name));
        _previewStack->setCurrentWidget(_paintIcon);
        return;
    }

    _colorPreview->setVisible(true);

    if (p.mode == PaintMode::Solid && p.color) {
        _colorPreview->setGradient({});
        _colorPreview->setRgba32(p.color->toRGBA());
        _colorPreview->setIndicator(ColorPreview::None);
    } else if (p.mode == PaintMode::CssSwatch && p.color) {
        _colorPreview->setGradient({});
        _colorPreview->setRgba32(p.color->toRGBA());
        _colorPreview->setIndicator(ColorPreview::Swatch);
    } else if (p.mode == PaintMode::LegacySwatch && p.server) {
        // Swatch: single flat color from the first stop
        auto swatch = cast<SPGradient>(p.server);
        Colors::Color color(0);
        if (swatch) {
            if (auto vect = swatch->getVector()) {
                if (auto stop = vect->getFirstStop()) {
                    color = stop->getColor();
                }
            }
        }
        _colorPreview->setGradient({});
        _colorPreview->setRgba32(color.toRGBA());
        _colorPreview->setIndicator(ColorPreview::Swatch);
    } else if (p.mode == PaintMode::Gradient && p.server) {
        auto grad = cast<SPGradient>(p.server);
        std::vector<ColorPreview::GradientStop> stops;
        if (grad) {
            grad->ensureVector();
            for (auto& stop : grad->vector.stops) {
                if (stop.color.has_value()) {
                    double a = 1.0;
                    auto c = stop.color->toRGBA(a);
                    stops.push_back(
                        {stop.offset, SP_RGBA32_R_F(c), SP_RGBA32_G_F(c), SP_RGBA32_B_F(c), SP_RGBA32_A_F(c)});
                }
            }
        }
        _colorPreview->setGradient(std::move(stops));
        _colorPreview->setIndicator(is<SPRadialGradient>(p.server) ? ColorPreview::RadialGradient
                                                                   : ColorPreview::LinearGradient);
    } else if ((p.mode == PaintMode::Pattern || p.mode == PaintMode::Hatch) && p.server) {
        constexpr unsigned int BG_WHITE = 0xffffffff;
        int ph = _colorPreview->height();
        double dpr = _colorPreview->devicePixelRatioF();
        auto image = Inkscape::PatternManager::get().get_preview(p.server, 200, ph, BG_WHITE, dpr);
        _colorPreview->setImage(std::move(image));
        _colorPreview->setIndicator(ColorPreview::None);
    } else if (p.mode == PaintMode::Mesh) {
        const auto icon_name = get_paint_mode_icon(PaintMode::Mesh);
        _paintIcon->setIcon(QIcon(":/icons/" + icon_name));
        _previewStack->setCurrentWidget(_paintIcon);
    } else {
        assert(false);
    }
}

// --- PaintStrip helpers ---

void StylePanel::PaintStrip::setFlatColor(const Inkscape::Colors::Color& color) {
    auto c = color;
    c.enableOpacity(false);
    auto css = new_css_attr();
    sp_repr_css_set_property_string(css.get(), isFill ? "fill" : "stroke", c.toString(false));
    sp_repr_css_set_property_double(css.get(), isFill ? "fill-opacity" : "stroke-opacity", color.getOpacity());

    if (paintBtn) paintBtn->setFlatColor(color);
}

void StylePanel::PaintStrip::setPreviewFromPaint(const Linea::mixed_property<Linea::PaintProp>& paint,
                                                 const Linea::mixed_property<double>& opacity) {
    if (paintBtn) paintBtn->setPreview(paint, opacity);
}

void StylePanel::PaintStrip::setPaintFromProps(const Linea::mixed_property<Linea::PaintProp>& paint,
                                               const Linea::mixed_property<double>& /*opacity*/,
                                               const Linea::mixed_property<SPWindRule>& fillRule) {
    if (!selector) return;

    if (paint.is_mixed()) {
        selector->showPlaceholder(QObject::tr("Mixed"), true);
    } else {
        selector->updateFromPaintProps(paint.value());
    }

    if (isFill && fillRule.is_single()) {
        selector->setFillRule(fillRule.value() == SP_WIND_RULE_NONZERO ? FillRule::NonZero : FillRule::EvenOdd);
    }

    // Alpha is no longer set here — it's bound directly via
    // binder.bind(Props::fill_opacity / stroke_opacity, alpha) which handles
    // the percent scaling through NumberEdit::setFactor(100).
}

// --- PaintStrip binding ---

void StylePanel::PaintStrip::bind(Props::Binder& binder) {
    auto editor = binder.editor();
    bool fill = isFill;

    // ---- Read: model → PaintButton preview + PaintSelector ----
    binder.bindField(fill ? Props::Field::fill : Props::Field::stroke, [this, fill](const Props::SelectionState& s) {
        auto& paint = fill ? s.style.fill : s.style.stroke;
        auto& opacity = fill ? s.style.fill_opacity : s.style.stroke_opacity;

        setPaintFromProps(paint, opacity, fill ? s.style.fill_rule : mixed_property<SPWindRule>{});
        setPreviewFromPaint(paint, opacity);
    });

    // Fill rule has its own field — update the PaintSelector icon when it changes.
    if (fill) {
        binder.bindField(Props::Field::fill_rule, [this](const Props::SelectionState& s) {
            if (s.style.fill_rule.is_single()) {
                selector->setFillRule(s.style.fill_rule.value() == SP_WIND_RULE_NONZERO ? FillRule::NonZero
                                                                                        : FillRule::EvenOdd);
            }
        });
    }

    // ---- Write: PaintSelector signals → editor->apply(Op) ----

    binder.track(
        connect(selector, &PaintSelector::flatColorChanged, paintBtn, [this, editor, fill](const Color& color) {
            auto c = color;
            c.enableOpacity(false);
            auto css = new_css_attr();
            sp_repr_css_set_property_string(css.get(), fill ? "fill" : "stroke", c.toString(false));
            sp_repr_css_set_property_double(css.get(), fill ? "fill-opacity" : "stroke-opacity", color.getOpacity());
            editor->apply(PaintEditDelegate::CssOp{css}, fill ? "change-fill" : "change-stroke",
                          fill ? RC_("Undo", "Set fill color") : RC_("Undo", "Set stroke color"));
            if (paintBtn) paintBtn->setFlatColor(color);
        }));

    binder.track(connect(
        selector, &PaintSelector::gradientChanged, paintBtn, [editor, fill](SPGradient* vector, SPGradientType type) {
            editor->apply(PaintEditDelegate::GradientOp{vector, type, fill},
                          fill ? "fill-gradient-change" : "stroke-gradient-change",
                          fill ? RC_("Undo", "Set gradient on fill") : RC_("Undo", "Set gradient on stroke"));
        }));

    binder.track(connect(
        selector, &PaintSelector::patternChanged, paintBtn,
        [editor, fill](SPPattern* pattern, std::optional<Color> color, const QString& label,
                       const Geom::Affine& transform, const Geom::Point& offset, bool uniform, const Geom::Scale& gap) {
            editor->apply(PaintEditDelegate::PatternOp{pattern, fill, color, label.toStdString(), transform, offset,
                                                       uniform, gap},
                          fill ? "fill-pattern-change" : "stroke-pattern-change",
                          fill ? RC_("Undo", "Set pattern on fill") : RC_("Undo", "Set pattern on stroke"));
        }));

    binder.track(connect(
        selector, &PaintSelector::hatchChanged, paintBtn,
        [editor, fill](SPHatch* hatch, std::optional<Color> color, const QString& label, const Geom::Affine& transform,
                       const Geom::Point& offset, double pitch, double rotation, double thickness) {
            editor->apply(PaintEditDelegate::HatchOp{hatch, fill, color, label.toStdString(), transform, offset, pitch,
                                                     rotation, thickness},
                          fill ? "fill-pattern-change" : "stroke-pattern-change",
                          fill ? RC_("Undo", "Set pattern on fill") : RC_("Undo", "Set pattern on stroke"));
        }));

    binder.track(connect(selector, &PaintSelector::meshChanged, paintBtn, [editor, fill](SPGradient* mesh) {
        editor->apply(PaintEditDelegate::MeshOp{mesh, fill}, fill ? "fill-mesh-change" : "stroke-mesh-change",
                      fill ? RC_("Undo", "Set mesh on fill") : RC_("Undo", "Set mesh on stroke"));
    }));

    binder.track(connect(selector, &PaintSelector::swatchChanged, paintBtn,
                         [editor, fill](SPGradient* swatch, Inkscape::UI::EditOperation op, SPGradient* replacement,
                                        std::optional<Color> color, QString label) {
                             editor->apply(
                                 PaintEditDelegate::SwatchOp{swatch, op, replacement, color, label.toStdString(), fill},
                                 fill ? "fill-swatch-change" : "stroke-swatch-change",
                                 fill ? RC_("Undo", "Set swatch on fill") : RC_("Undo", "Set swatch on stroke"));
                         }));

    // Fill rule (fill strip only)
    if (fill) {
        binder.track(connect(selector, &PaintSelector::fillRuleChanged, paintBtn, [editor](FillRule rule) {
            auto css = new_css_attr();
            sp_repr_css_set_property(css.get(), "fill-rule", rule == FillRule::EvenOdd ? "evenodd" : "nonzero");
            editor->apply(PaintEditDelegate::CssOp{css}, "change-fill-rule", RC_("Undo", "Change fill rule"));
        }));
    }

    // Inherit/derived mode
    binder.track(connect(selector, &PaintSelector::inheritModeChanged, paintBtn, [editor, fill](PaintDerivedMode mode) {
        auto css = new_css_attr();
        auto attr = fill ? "fill" : "stroke";
        switch (mode) {
            case PaintDerivedMode::Unset:
                sp_repr_css_unset_property(css.get(), attr);
                break;
            case PaintDerivedMode::Inherit:
                sp_repr_css_set_property(css.get(), attr, "inherit");
                break;
            case PaintDerivedMode::ContextFill:
                sp_repr_css_set_property(css.get(), attr, "context-fill");
                break;
            case PaintDerivedMode::ContextStroke:
                sp_repr_css_set_property(css.get(), attr, "context-stroke");
                break;
            case PaintDerivedMode::CurrentColor:
                sp_repr_css_set_property(css.get(), attr, "currentColor");
                break;
        }
        editor->apply(PaintEditDelegate::CssOp{css}, fill ? "inherit-fill" : "inherit-stroke",
                      fill ? RC_("Undo", "Inherit fill") : RC_("Undo", "Inherit stroke"));
    }));

    // ---- Write: Add button (set default flat color) ----
    binder.track(connect(addBtn, &QPushButton::clicked, paintBtn, [this, editor, fill](bool) {
        Color color(0x909090ff);
        auto c = color;
        c.enableOpacity(false);
        auto css = new_css_attr();
        sp_repr_css_set_property_string(css.get(), fill ? "fill" : "stroke", c.toString(false));
        sp_repr_css_set_property_double(css.get(), fill ? "fill-opacity" : "stroke-opacity", color.getOpacity());
        editor->apply(PaintEditDelegate::CssOp{css}, fill ? "change-fill" : "change-stroke",
                      fill ? RC_("Undo", "Set fill color") : RC_("Undo", "Set stroke color"));
        if (paintBtn) paintBtn->setFlatColor(color);
    }));

    // ---- Write: Clear button (set paint to none) ----
    binder.track(connect(clearBtn, &QPushButton::clicked, paintBtn, [editor, fill](bool) {
        auto css = new_css_attr();
        if (fill) {
            sp_repr_css_set_property(css.get(), "fill", "none");
            sp_repr_css_unset_property(css.get(), "fill-opacity");
        } else {
            for (auto attr : {"stroke", "stroke-opacity", "stroke-width", "stroke-miterlimit", "stroke-linejoin",
                              "stroke-linecap", "stroke-dashoffset", "stroke-dasharray"}) {
                sp_repr_css_unset_property(css.get(), attr);
            }
            sp_repr_css_set_property(css.get(), "stroke", "none");
        }
        editor->apply(PaintEditDelegate::CssOp{css}, fill ? "remove-fill" : "remove-stroke",
                      fill ? RC_("Undo", "Remove fill") : RC_("Undo", "Remove stroke"));
    }));
}

// --- AppearancePanel ---

StylePanel::StylePanel(unsigned int tag, QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::AppearancePanel>())
    , _tag(tag) {
    _ui->setupUi(this);

    // Keep the reset buttons' grid cells from collapsing when the buttons
    // are hidden by visibility rules — otherwise the adjacent controls
    // widen to fill the gap.
    auto retain = [](QWidget* w) {
        auto sp = w->sizePolicy();
        sp.setRetainSizeWhenHidden(true);
        w->setSizePolicy(sp);
    };
    retain(_ui->resetOpacity);
    retain(_ui->resetBlendBtn);

    construct();
}

StylePanel::~StylePanel() = default;

void StylePanel::construct() {
    setupPaintStrip(_fill, true);
    setupPaintStrip(_stroke, false);
    setupStrokeWidgets();
    setupMarkers();
    setupOpacity();
    setupBlendMode();
}

void StylePanel::setupPaintStrip(PaintStrip& strip, bool isFill) {
    strip.isFill = isFill;

    auto oldBtn = isFill ? _ui->fillButton : _ui->strokeButton;
    strip.paintBtn = new PaintButton(oldBtn->parentWidget());
    strip.paintBtn->setObjectName(oldBtn->objectName());
    strip.paintBtn->setToolTip(oldBtn->toolTip());
    strip.paintBtn->setSizePolicy(oldBtn->sizePolicy());
    if (auto layout = oldBtn->parentWidget()->layout()) {
        layout->replaceWidget(oldBtn, strip.paintBtn);
    }
    oldBtn->hide();

    strip.alpha = isFill ? _ui->fillAlpha : _ui->strokeAlpha;
    strip.addBtn = isFill ? _ui->fillAdd : _ui->strokeAdd;
    strip.clearBtn = isFill ? _ui->fillClear : _ui->strokeClear;
    strip.clearBtn->setVisible(false);

    // Create PaintSelector popup
    strip.selector = PaintSelector::create(false, isFill, true, this).release();
    strip.popup = new PopupMenu(this);
    strip.popup->setContent(strip.selector);

    connect(strip.paintBtn, &QPushButton::clicked, this, [&strip]() { strip.popup->showBelowWidget(strip.paintBtn); });
}

// --- Stroke widgets ---

void StylePanel::setupStrokeWidgets() {
    // Unit tracker
    _unitTracker = new UnitTracker(Inkscape::Util::UNIT_TYPE_LINEAR, this);
    _unitCombo = _unitTracker->createUnitCombo(_ui->unitPlaceholder);
    auto unitLayout = new QHBoxLayout(_ui->unitPlaceholder);
    unitLayout->setContentsMargins(0, 0, 0, 0);
    unitLayout->addWidget(_unitCombo);

    // Stroke options popup
    _strokeOptions = new StrokeOptions(this);
    _strokeOptionsPopup = new PopupMenu(this);
    _strokeOptionsPopup->setContent(_strokeOptions);

    connect(_ui->strokeOptionsBtn, &QPushButton::clicked, this,
            [this]() { _strokeOptionsPopup->showBelowWidget(_ui->strokeOptionsBtn); });

    // Unit changed — convert displayed value to the new unit (no document write).
    connect(_unitTracker, &UnitTracker::unitChanged, this, [this](const Inkscape::Util::Unit* newUnit) {
        if (newUnit == _currentUnit) return;

        auto width = _ui->strokeWidth->value();
        if (_currentUnit) {
            width = Quantity::convert(width, _currentUnit, newUnit);
            QSignalBlocker b(_ui->strokeWidth);
            _ui->strokeWidth->setValue(width);
        }
        _currentUnit = newUnit;
    });

    // Track stroke widgets for show/hide
    _strokeWidgets = {_ui->strokeWidth,  _ui->unitPlaceholder, _ui->strokeOptionsBtn,
                      _ui->dashSelector, _ui->markersLabel,    _ui->markersPlaceholder};
}

// --- Markers ---

void StylePanel::setupMarkers() {
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

// --- Opacity ---

void StylePanel::setupOpacity() {
    // Reset opacity to 100% — the Binder's valueChanged connection handles the write.
    connect(_ui->resetOpacity, &QPushButton::clicked, this, [this]() { _ui->opacity->setValue(1); });
}

// --- Blend mode ---

void StylePanel::setupBlendMode() {
    // Reset blend mode to normal — the Binder's modeChanged connection handles the write.
    connect(_ui->resetBlendBtn, &QPushButton::clicked, this,
            [this]() { _ui->blendCombo->setActiveById(SP_CSS_BLEND_NORMAL); });
}

// --- Public API ---

void StylePanel::setDocument(SPDocument* doc) {
    _document = doc;

    if (_fill.selector) _fill.selector->setDocument(doc);
    if (_stroke.selector) _stroke.selector->setDocument(doc);

    for (auto combo : {_markerStart, _markerMid, _markerEnd}) {
        if (combo) combo->setDocument(doc);
    }
}

void StylePanel::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;

    if (_fill.selector) _fill.selector->setDesktop(desktop);
    if (_stroke.selector) _stroke.selector->setDesktop(desktop);

    if (desktop && desktop->getNamedView()) {
        auto unit = desktop->getNamedView()->display_units;
        if (unit) {
            _unitTracker->setActiveUnit(unit);
            _currentUnit = unit;
        }
    }
}

// --- Declarative binding (property system) -----------------------------------

void StylePanel::bind(Props::Binder& binder) {
    using namespace Props::Cond;

    // Opacity — SpinScale with percent scaling (model 0..1, UI 0..100).
    // setFactor(100) is already applied in setupOpacity().
    binder.bind(Props::opacity, _ui->opacity);

    // Opacity controls visible only when there's a selection.
    binder.visibleWhen(_ui->opacityLabel, hasSelection);
    binder.visibleWhen(_ui->opacity, hasSelection);

    // Reset-opacity button visible when opacity is not uniformly 100%
    // (covers single-with-different-value and mixed selections).
    binder.visibleWhen(_ui->resetOpacity, hasSelection && differsFrom(Props::opacity, 1.0));

    // Blend mode — BlendModeCombo has WidgetTraits<SPBlendMode>.
    binder.bind(Props::blend_mode, _ui->blendCombo);

    binder.visibleWhen(_ui->blendLabel, hasSelection);
    binder.visibleWhen(_ui->blendCombo, hasSelection);

    // Reset-blend button visible when blend mode is not uniformly Normal.
    binder.visibleWhen(_ui->resetBlendBtn, hasSelection && differsFrom(Props::blend_mode, SP_CSS_BLEND_NORMAL));

    // Fill / stroke paint strips — paint read via bindField, alpha via bind(),
    // PaintSelector/add/clear writes via editor->apply() held by track().
    _fill.alpha->setFactor(100);
    _stroke.alpha->setFactor(100);
    binder.bind(Props::fill_opacity, _fill.alpha);
    binder.bind(Props::stroke_opacity, _stroke.alpha);
    _fill.bind(binder);
    _stroke.bind(binder);

    // Stroke options (join, cap, miter, paint order) — StrokeOptions has its
    // own bind() that wires all four sub-properties.
    _strokeOptions->bind(binder);

    // Stroke option icons — each driven by its own field via bindField.
    binder.bindField(Props::Field::stroke_linejoin, [this](const Props::SelectionState& s) {
        if (s.style.stroke_linejoin.is_single()) {
            const char* name = "stroke-join-miter";
            switch (s.style.stroke_linejoin.value()) {
                case SP_STROKE_LINEJOIN_ROUND:
                    name = "stroke-join-round";
                    break;
                case SP_STROKE_LINEJOIN_BEVEL:
                    name = "stroke-join-bevel";
                    break;
            }
            _ui->joinIcon->setIcon(QIcon(QString(":/icons/") + name));
        }
    });
    binder.bindField(Props::Field::stroke_linecap, [this](const Props::SelectionState& s) {
        if (s.style.stroke_linecap.is_single()) {
            const char* name = "stroke-cap-butt";
            switch (s.style.stroke_linecap.value()) {
                case SP_STROKE_LINECAP_ROUND:
                    name = "stroke-cap-round";
                    break;
                case SP_STROKE_LINECAP_SQUARE:
                    name = "stroke-cap-square";
                    break;
            }
            _ui->capIcon->setIcon(QIcon(QString(":/icons/") + name));
        }
    });
    binder.bindField(Props::Field::paint_order, [this](const Props::SelectionState& s) {
        if (s.style.paint_order.is_single()) {
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

    // Stroke width — read-only in the property table (StrokeWidthProp), so
    // bindField for read + binder.track() + editor->apply(StrokeWidthOp) for
    // write. The model speaks px; the widget displays the active unit.
    auto editor = binder.editor();
    auto unit = [this]() { return _unitTracker->getActiveUnit(); };

    binder.bindField(Props::Field::stroke_width, [this](const Props::SelectionState& s) {
        QSignalBlocker b(_ui->strokeWidth);
        if (s.style.stroke_width.is_single()) {
            _ui->strokeWidth->setMixedMode(false);
            double px = s.style.stroke_width.value().value;
            if (auto u = _unitTracker->getActiveUnit()) {
                _ui->strokeWidth->setValue(Quantity::convert(px, "px", u));
            }
        } else {
            _ui->strokeWidth->setMixedMode(true);
        }
    });

    binder.track(connect(_ui->strokeWidth, &NumberEdit::valueChanged, this, [this, editor, unit](double displayValue) {
        editor->apply(PaintEditDelegate::StrokeWidthOp{displayValue, false, unit()}, "set-stroke-width",
                      RC_("Undo", "Set stroke width"));
    }));

    // Stroke dash — read-only in the property table (StrokeDashProp), so
    // bindField for read + binder.track() + editor->apply(DashOp) for write.
    binder.bindField(Props::Field::stroke_dash, [this](const Props::SelectionState& s) {
        if (s.style.stroke_dash.is_single() || s.style.stroke_dash.is_mixed()) {
            auto& dash = s.style.stroke_dash.value();
            _ui->dashSelector->setDashPattern(dash.dashes, dash.offset);
            if (s.style.stroke_dash.is_mixed()) {
                _ui->dashSelector->setPlaceholder(tr("Mixed"));
            }
        } else {
            _ui->dashSelector->setDashPattern({}, 0.0);
        }
    });

    auto applyDash = [this, editor]() {
        editor->apply(PaintEditDelegate::DashOp{_ui->dashSelector->getDashPattern(), _ui->dashSelector->getOffset()},
                      "set-dash-pattern", RC_("Undo", "Set stroke dash pattern"));
    };
    binder.track(connect(_ui->dashSelector, &DashSelector::dashChanged, this, applyDash));
    binder.track(connect(_ui->dashSelector, &DashSelector::offsetChanged, this, applyDash));

    // Markers (start / mid / end) — read-only string fields; bindField for
    // read (URI → SPObject) + binder.track() + editor->apply(CssOp) for write.
    auto bindMarker = [this, &binder, editor](MarkerComboBox* combo, Props::Field field) {
        binder.bindField(field, [this, combo, field](const Props::SelectionState& s) {
            if (combo->inUpdate()) return;
            const auto* prop = [&]() -> const mixed_property<std::string>* {
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

    // ---- Visibility / enablement rules driven by property values ----

    // Fill/stroke sections are visible when the selection has at least one
    // non-image item (images have no fill or stroke to edit).
    auto editable = hasOtherThan<&Props::Counts::images>;

    // "Fill is defined" — at least one item has non-None fill paint.
    // In mixed mode, returns true (some items have fill).
    auto fillDefined = [](const Props::SelectionState& s) {
        const auto& p = s.style.fill;
        if (p.is_unset()) return false;
        if (p.is_mixed()) return true;
        return p.value().mode != PaintMode::None;
    };
    // "Fill is not defined" — all items have None fill. The add button only
    // shows when there's nothing to show in the paint controls; in mixed mode
    // the paint button is already visible, so add is hidden.
    auto fillNotDefined = [](const Props::SelectionState& s) {
        const auto& p = s.style.fill;
        if (p.is_unset()) return true;
        if (p.is_mixed()) return false;
        return p.value().mode == PaintMode::None;
    };

    binder.visibleWhen(_ui->fillLabel, editable);
    binder.visibleWhen(_fill.paintBtn, editable && fillDefined);
    binder.visibleWhen(_fill.alpha, editable && fillDefined);
    binder.visibleWhen(_fill.clearBtn, editable && fillDefined);
    binder.visibleWhen(_fill.addBtn, editable && fillNotDefined);

    // "Stroke is defined" — same structure as fill.
    auto strokeDefined = [](const Props::SelectionState& s) {
        const auto& p = s.style.stroke;
        if (p.is_unset()) return false;
        if (p.is_mixed()) return true;
        return p.value().mode != PaintMode::None;
    };
    auto strokeNotDefined = [](const Props::SelectionState& s) {
        const auto& p = s.style.stroke;
        if (p.is_unset()) return true;
        if (p.is_mixed()) return false;
        return p.value().mode == PaintMode::None;
    };

    binder.visibleWhen(_ui->strokeLabel, editable);
    binder.visibleWhen(_stroke.paintBtn, editable && strokeDefined);
    binder.visibleWhen(_stroke.alpha, editable && strokeDefined);
    binder.visibleWhen(_stroke.clearBtn, editable && strokeDefined);
    binder.visibleWhen(_stroke.addBtn, editable && strokeNotDefined);

    // Stroke attribute widgets: visible only when stroke is defined.
    for (auto w : _strokeWidgets) {
        binder.visibleWhen(w, editable && strokeDefined);
    }

    // Stroke width/dash/options/markers enabled when stroke is not uniformly
    // None and not uniformly hairline. Enabled in mixed mode (editing applies
    // the new value to all items).
    auto strokeEnabled = [](const Props::SelectionState& s) {
        if (s.style.stroke.is_single() && s.style.stroke.value().mode == PaintMode::None) return false;
        if (s.style.hairline.is_single() && s.style.hairline.value()) return false;
        return true;
    };
    binder.enableWhen(_ui->strokeWidth, hasSelection && strokeEnabled);
    binder.enableWhen(_ui->strokeOptionsBtn, hasSelection && strokeEnabled);
    binder.enableWhen(_ui->dashSelector, hasSelection && strokeEnabled);
    if (_ui->markersPlaceholder) binder.enableWhen(_ui->markersPlaceholder, hasSelection && strokeEnabled);
}

} // namespace Linea::UI
