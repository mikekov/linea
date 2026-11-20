// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Feature-support predicates: whether a given feature is enabled, supported
 * or active in the current editing context.
 */

#include "feature-support.h"

#include "layer-manager.h"
#include "object/sp-item-group.h"
#include "props/binder.h"
#include "props/selection-state.h"
#include "selection.h"

namespace Linea {

bool shape_builder_supported(Inkscape::Selection* sel) {
    // The builder mosaics the whole selection into pathvectors (recursively,
    // groups included), so it needs at least two inputs to combine.
    return sel && sel->size() >= 2;
}

bool shape_builder_active(const Props::SelectionState& state) {
    return state.element.count.activeTool == TOOLS_BOOLEANS;
}

bool path_operations_supported(const Props::SelectionState& state) {
    const auto& c = state.element.count;
    // Path ops are an alternative to the shape-builder tool — unsupported
    // while it is the active tool.
    if (shape_builder_active(state)) return false;

    return c.paths + c.rectangles + c.ellipses + c.stars + c.polygon_paths + c.lines + c.textual >= 2;
}

bool mask_operations_supported(const Props::SelectionState& state) {
    const auto& c = state.element.count;
    // Same rule as path ops — the shape-builder tool owns the extra slot.
    if (shape_builder_active(state)) return false;

    // The topmost selection entry becomes the mask/clipping path — need a
    // source plus at least one target item.
    return c.items >= 2;
}

bool path_operations_supported(Props::Binder* binder) {
    const auto* state = binder ? binder->currentState() : nullptr;
    return state && path_operations_supported(*state);
}

bool group_isolation_active(Inkscape::LayerManager& layer_manager) {
    // Single source of truth for "the user is editing inside a group":
    // currentLayer is a plain (non-layer) group. Drives both the
    // translucency effect and the toolbar's exit-isolation button.
    auto layer = layer_manager.currentLayer();
    // currentLayer() is the root group when not inside anything — root is
    // a non-layer group too, so it must be excluded explicitly.
    return layer && !layer->isLayer() && !layer_manager.isRoot();
}

} // namespace Linea
