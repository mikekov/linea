// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Feature-support predicates: whether a given feature is enabled, supported
 * or active in the current editing context. These drive visibility and
 * enablement of feature controls (toolbar buttons, menu items, panels).
 *
 * Today a Selection is the only input these checks need — the desktop,
 * document and preferences are all reachable through it — but that is an
 * implementation detail, not part of the contract.
 */

#ifndef LINEA_FEATURE_SUPPORT_H
#define LINEA_FEATURE_SUPPORT_H

namespace Inkscape {
class LayerManager;
class Selection;
}

namespace Linea {

namespace Props {
class Binder;
struct SelectionState;
}

/** The shape builder can work here: two or more items are selected (the
 * builder extracts pathvectors, so any shape-like item — including
 * groups — contributes). */
bool shape_builder_supported(Inkscape::Selection* sel);

/** The shape-builder tool is the active tool. Drives the toolbar's
 * cancel-shape-builder button. */
bool shape_builder_active(const Props::SelectionState& state);

/** Boolean path operations (union, difference, ...) apply: at least two
 * shape-like items are selected. Canonical rule — shared by the appearance
 * panel's path-operations section. */
bool path_operations_supported(const Props::SelectionState& state);

/** Convenience overload reading the binder's current state. */
bool path_operations_supported(Props::Binder* binder);

/** Clip/mask operations (set, inverse, release, clip group) apply: at least
 * two items are selected — the topmost becomes the mask or clipping path. */
//TODO: This is not accurate - release doesn't need two items, it needs a *specific* item
bool mask_operations_supported(const Props::SelectionState& state);

/** Editing is taking place inside a group: the layer manager's current
 * layer is a plain (non-layer) group. Single source of truth for the
 * isolation state — drives both the translucency effect
 * (SPDesktop::updateTranslucencyGroups) and the toolbar's exit button. */
bool group_isolation_active(Inkscape::LayerManager& layer_manager);

} // namespace Linea

#endif // LINEA_FEATURE_SUPPORT_H
