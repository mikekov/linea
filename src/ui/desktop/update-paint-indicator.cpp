// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Free functions for updating the paint indicator from desktop/selection state.
 */

#include "update-paint-indicator.h"

#include "desktop-style.h"
#include "desktop.h"
#include "object/sp-mesh-gradient.h"
#include "paint-indicator.h"
#include "pattern-manager.h"
#include "qt/ui/color-palette-widget.h"
#include "qt/util/drawing-utils.h"
#include "selection.h"
#include "ui/tools/tool-data.h"
#include "util/object-renderer.h"

#include <QColor>

using Inkscape::UI::Widget::PaintMode;

namespace Linea::UI {

namespace {

PaintIndicator::Part getFillStrokeOrder(const SPIPaintOrder& order) {
    // Normal/default order: stroke is the primary part
    if (!order.set || order.layer[0] == SP_CSS_PAINT_ORDER_NORMAL) {
        return PaintIndicator::Part::Stroke;
    }
    auto layers = order.get_layers();
    int fillIdx = -1, strokeIdx = -1;
    for (int i = 0; i < static_cast<int>(PAINT_ORDER_LAYERS); ++i) {
        if (layers[i] == SP_CSS_PAINT_ORDER_FILL) fillIdx = i;
        if (layers[i] == SP_CSS_PAINT_ORDER_STROKE) strokeIdx = i;
    }
    // Higher index = painted later = on top
    return strokeIdx > fillIdx ? PaintIndicator::Part::Stroke : PaintIndicator::Part::Fill;
}

void updatePaintIndicatorPart(PaintIndicator* p, const std::optional<Linea::PaintProp>& paint, bool isFill) {
    auto part = isFill ? PaintIndicator::Part::Fill : PaintIndicator::Part::Stroke;

    if (!paint) {
        // mixed paint or no paint
        p->setIndeterminate(part);
    } else if (paint->mode == PaintMode::None) {
        p->setNone(part);
    } else if (paint->mode == PaintMode::Solid) {
        p->setColor(QColor::fromRgba(paint->color->toARGB()), part);
    } else if (paint->mode == PaintMode::CssSwatch) {
        p->setSwatchColor(QColor::fromRgba(paint->color->toARGB()), part);
    } else if (paint->mode == PaintMode::Gradient || paint->mode == PaintMode::LegacySwatch) {
        p->setGradient(toQLinearGradient(paint->gradient), part);
    } else if (paint->mode == PaintMode::Pattern || paint->mode == PaintMode::Hatch) {
        auto rect = p->indicatorSize();
        constexpr unsigned int BG_WHITE = 0xffffffff;
        auto image =
            Inkscape::PatternManager::get().get_preview(paint->server, rect.width(), rect.height(), BG_WHITE, p->devicePixelRatioF());
        p->setPattern(image, part);
    } else if (paint->mode == PaintMode::Mesh) {
        object_renderer renderer;
        auto rect = p->indicatorSize();
        if (auto mesh = paint->mesh) {
            auto image =
                renderer.render(*mesh, rect.width(), rect.height(), p->devicePixelRatioF(), object_renderer::options{});
            p->setPattern(image, part);
        } else {
            p->setIndeterminate(part);
        }
    } else if (paint->mode == PaintMode::Derived) {
        p->setInherited(part);
    } else {
        assert(false);
    }
}

} // namespace

void updatePaintIndicator(PaintIndicator* p, const Linea::mixed_property<Linea::PaintProp>& fill,
                          const Linea::mixed_property<Linea::PaintProp>& stroke, const SPIPaintOrder& order) {
    std::optional<Linea::PaintProp> fillPaint;
    if (fill.is_single()) {
        fillPaint = fill.value();
    }
    updatePaintIndicatorPart(p, fillPaint, true);

    std::optional<Linea::PaintProp> strokePaint;
    if (stroke.is_single()) {
        strokePaint = stroke.value();
    }
    updatePaintIndicatorPart(p, strokePaint, false);

    p->setTopSquare(getFillStrokeOrder(order));
}

void updatePaintIndicator(PaintIndicator* p, SPDesktop* desktop) {
    // if selection is not empty, don't change paint indicator
    if (!desktop || !desktop->getSelection()->isEmpty()) return;

    auto tool = desktop->getTool();

    // if current tool can draw new elements (it has its own style), then show its paint
    auto toolData = tool ? getToolData(tool->get_name()) : nullptr;
    if (toolData && toolData->ownStyle) {
        auto [fill, stroke, order] = sp_desktop_get_current_style_paints(desktop, tool);
        updatePaintIndicatorPart(p, fill, true);
        updatePaintIndicatorPart(p, stroke, false);
        p->setTopSquare(getFillStrokeOrder(order));
    } else {
        p->setIndeterminate(PaintIndicator::Part::Fill);
        p->setIndeterminate(PaintIndicator::Part::Stroke);
        p->setTopSquare(PaintIndicator::Part::Fill);
    }
}

void refreshPaintIndicator(PaintIndicator* p, ColorPaletteWidget* palette, SPDesktop* desktop,
                           const Linea::PresentationState& presentation) {
    if (!desktop) return;

    if (desktop->getSelection()->isEmpty()) {
        palette->updateCurrentColorIndicator({}, {});
        updatePaintIndicator(p, desktop);
    } else {
        palette->updateCurrentColorIndicator(presentation.fill, presentation.stroke);
        updatePaintIndicator(p, presentation.fill, presentation.stroke, presentation.paint_order.value());
    }
}

} // namespace Linea::UI
