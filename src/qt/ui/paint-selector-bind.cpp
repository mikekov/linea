// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Shared wiring between a PaintSelector widget and a Props::Editor.
 */

#include "paint-selector-bind.h"

#include "colors/color.h"
#include "document-undo.h"
#include "paint-selector.h"
#include "props/editor.h"
#include "style.h"
#include "ui/widget/paint-enums.h"
#include "util-string/context-string.h"
#include "util/paint-item-ops.h"

using Inkscape::Colors::Color;
using Inkscape::UI::Widget::PaintDerivedMode;
using Linea::Util::new_css_attr;
using Linea::Util::PaintEditDelegate;

namespace Linea::UI {

boost::intrusive_ptr<SPCSSAttr> makePaintCss(bool fill, const Inkscape::Colors::Color& color) {
    auto c = color;
    c.enableOpacity(false);
    auto css = new_css_attr();
    sp_repr_css_set_property_string(css.get(), fill ? "fill" : "stroke", sp_color_to_css(c));
    sp_repr_css_set_property_double(css.get(), fill ? "fill-opacity" : "stroke-opacity", color.getOpacity());
    return css;
}

void syncPaintSelector(PaintSelector* selector, const Linea::mixed_property<Linea::PaintProp>& paint,
                       const Linea::mixed_property<SPWindRule>& fillRule, bool fill) {
    if (!selector) return;

    if (paint.is_mixed()) {
        selector->showPlaceholder(QObject::tr("Mixed"), true);
    } else {
        selector->updateFromPaintProps(paint.value());
    }

    if (fill && fillRule.is_single()) {
        selector->setFillRule(fillRule.value() == SP_WIND_RULE_NONZERO ? FillRule::NonZero : FillRule::EvenOdd);
    }
}

std::vector<QMetaObject::Connection> bindPaintSelectorToEditor(PaintSelector* selector, Linea::Props::Editor* editor,
                                                               bool fill, QObject* context) {
    if (!selector || !editor || !context) return {};

    std::vector<QMetaObject::Connection> conns;

    conns.push_back(
        QObject::connect(selector, &PaintSelector::flatColorChanged, context, [editor, fill](const Color& color) {
            editor->apply(PaintEditDelegate::CssOp{makePaintCss(fill, color)}, fill ? "change-fill" : "change-stroke",
                          fill ? RC_("Undo", "Set fill color") : RC_("Undo", "Set stroke color"));
        }));

    conns.push_back(QObject::connect(
        selector, &PaintSelector::gradientChanged, context, [editor, fill](SPGradient* vector, SPGradientType type) {
            editor->apply(PaintEditDelegate::GradientOp{vector, type, fill},
                          fill ? "fill-gradient-change" : "stroke-gradient-change",
                          fill ? RC_("Undo", "Set gradient on fill") : RC_("Undo", "Set gradient on stroke"));
        }));

    conns.push_back(QObject::connect(
        selector, &PaintSelector::patternChanged, context,
        [editor, fill](SPPattern* pattern, std::optional<Color> color, const QString& label,
                       const Geom::Affine& transform, const Geom::Point& offset, bool uniform, const Geom::Scale& gap) {
            editor->apply(PaintEditDelegate::PatternOp{pattern, fill, color, label.toStdString(), transform, offset,
                                                       uniform, gap},
                          fill ? "fill-pattern-change" : "stroke-pattern-change",
                          fill ? RC_("Undo", "Set pattern on fill") : RC_("Undo", "Set pattern on stroke"));
        }));

    conns.push_back(QObject::connect(
        selector, &PaintSelector::hatchChanged, context,
        [editor, fill](SPHatch* hatch, std::optional<Color> color, const QString& label, const Geom::Affine& transform,
                       const Geom::Point& offset, double pitch, double rotation, double thickness) {
            editor->apply(PaintEditDelegate::HatchOp{hatch, fill, color, label.toStdString(), transform, offset, pitch,
                                                     rotation, thickness},
                          fill ? "fill-pattern-change" : "stroke-pattern-change",
                          fill ? RC_("Undo", "Set pattern on fill") : RC_("Undo", "Set pattern on stroke"));
        }));

    conns.push_back(QObject::connect(selector, &PaintSelector::meshChanged, context, [editor, fill](SPGradient* mesh) {
        editor->apply(PaintEditDelegate::MeshOp{mesh, fill}, fill ? "fill-mesh-change" : "stroke-mesh-change",
                      fill ? RC_("Undo", "Set mesh on fill") : RC_("Undo", "Set mesh on stroke"));
    }));

    conns.push_back(QObject::connect(
        selector, &PaintSelector::swatchChanged, context,
        [editor, fill](SPGradient* swatch, Inkscape::UI::EditOperation op, SPGradient* replacement,
                       std::optional<Color> color, QString label) {
            editor->apply(PaintEditDelegate::SwatchOp{swatch, op, replacement, color, label.toStdString(), fill},
                          fill ? "fill-swatch-change" : "stroke-swatch-change",
                          fill ? RC_("Undo", "Set swatch on fill") : RC_("Undo", "Set swatch on stroke"));
        }));

    if (fill) {
        conns.push_back(QObject::connect(selector, &PaintSelector::fillRuleChanged, context, [editor](FillRule rule) {
            auto css = new_css_attr();
            sp_repr_css_set_property(css.get(), "fill-rule", rule == FillRule::EvenOdd ? "evenodd" : "nonzero");
            editor->apply(PaintEditDelegate::CssOp{css}, "change-fill-rule", RC_("Undo", "Change fill rule"));
        }));
    }

    conns.push_back(
        QObject::connect(selector, &PaintSelector::inheritModeChanged, context, [editor, fill](PaintDerivedMode mode) {
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

    return conns;
}

} // namespace Linea::UI
