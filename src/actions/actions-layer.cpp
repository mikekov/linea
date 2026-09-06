// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 *  Actions for Layers.
 *
 * These all require a window. To do: remove this requirement.
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-layer.h"

#include <array>
#include <giomm.h>
#include <glibmm/i18n.h>

#include "action-registry.h"
#include "actions-helper.h"
#include "actions/action-meta.h"
#include "desktop.h"
#include "document-undo.h"
#include "document.h"
// #include "inkscape-application.h"
#include "layer-manager.h"
#include "linea-window.h"
#include "message-stack.h"
#include "object/sp-root.h"
#include "selection.h"
// #include "ui/dialog/layer-properties.h"
#include "ui/icon-names.h"

/*
 * A layer is a group <g> element with a special Inkscape attribute (Inkscape:groupMode) set to
 * "layer". It is typically directly placed in the <svg> element but it is also possible to put
 * inside any other layer (a "layer" inside a normal group is considered a group). The GUI tracks
 * which is the "Current" layer. The "Current" layer is set when a new selection initiated
 * (i.e. when not adding objects to a previous selection), when it is chosen in the "Layers and
 * Objects" dialog, when using the previous/next layer menu items, and when moving objects to
 * adjacent layers.
 */

namespace {

void xlayer_new(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    (void)dt;

    // New Layer
    // QT TODO: Inkscape::UI::Dialog::LayerPropertiesDialog::showCreate(dt, dt->layerManager().currentLayer());
    // warning
    g_warning("layer_new not implemented");
}

void add_new_layer(LineaWindow* win, LayerRelativePosition pos) {
    auto desktop = win->get_desktop();
    if (!desktop) return;
    auto document = desktop->getDocument();
    auto current_layer = desktop->layerManager().currentLayer();
    auto new_layer = Inkscape::create_layer(document->getRoot(), current_layer, pos);
    desktop->layerManager().renameLayer(new_layer, current_layer->label(), true);
    desktop->getSelection()->clear();
    desktop->layerManager().setCurrentLayer(new_layer);
    Inkscape::DocumentUndo::done(document, RC_("Undo", "Add layer"), INKSCAPE_ICON("layer-new"));
    desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("New layer created."));
}

void layer_new_above(LineaWindow* wnd, LayerRelativePosition pos) {
    add_new_layer(wnd, Inkscape::LPOS_ABOVE);
}

void layer_new(LineaWindow* wnd) {
    layer_new_above(wnd, Inkscape::LPOS_ABOVE);
}

void layer_new_below(LineaWindow* wnd, LayerRelativePosition pos) {
    add_new_layer(wnd, Inkscape::LPOS_BELOW);
}

void layer_new_child(LineaWindow* wnd, LayerRelativePosition pos) {
    add_new_layer(wnd, Inkscape::LPOS_CHILD);
}

void layer_new_above_action(LineaWindow* win) { layer_new_above(win, Inkscape::LPOS_ABOVE); }
void layer_new_below_action(LineaWindow* win) { layer_new_below(win, Inkscape::LPOS_BELOW); }
void layer_new_child_action(LineaWindow* win) { layer_new_child(win, Inkscape::LPOS_CHILD); }

void layer_duplicate(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    if (!dt->layerManager().isRoot()) {
        dt->getSelection()->duplicate(true, true); // This requires the selection to be a layer!
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Duplicate layer"),
                                     INKSCAPE_ICON("layer-duplicate"));
        dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Duplicated layer."));

    } else {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    }
}

void layer_delete(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;
    auto root = dt->layerManager().currentRoot();

    if (!dt->layerManager().isRoot()) {
        dt->getSelection()->clear();
        SPObject* old_layer = dt->layerManager().currentLayer();
        SPObject* old_parent = old_layer->parent;
        SPObject* old_parent_parent = (old_parent != nullptr) ? old_parent->parent : nullptr;

        SPObject* survivor = Inkscape::previous_layer(root, old_layer);
        if (survivor != nullptr && survivor->parent == old_layer) {
            while (survivor != nullptr && survivor->parent != old_parent && survivor->parent != old_parent_parent) {
                survivor = Inkscape::previous_layer(root, survivor);
            }
        }

        if (survivor == nullptr || (survivor->parent != old_parent && survivor->parent != old_layer)) {
            survivor = Inkscape::next_layer(root, old_layer);
            while (survivor != nullptr && survivor != old_parent && survivor->parent != old_parent) {
                survivor = Inkscape::next_layer(root, survivor);
            }
        }

        // Deleting the old layer before switching layers is a hack to trigger the
        // listeners of the deletion event (as happens when old_layer is deleted using the
        // xml editor).  See
        // http://sourceforge.net/tracker/index.php?func=detail&aid=1339397&group_id=93438&atid=604306
        //
        old_layer->deleteObject();

        if (survivor) {
            dt->layerManager().setCurrentLayer(survivor);
        }

        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Delete layer"), INKSCAPE_ICON("layer-delete"));
        dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Deleted layer."));

    } else {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    }
}

void layer_rename(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;
    (void)dt;

    // Rename Layer
    // QT TODO: Inkscape::UI::Dialog::LayerPropertiesDialog::showRename(dt, dt->layerManager().currentLayer());
    g_warning("layer_rename not implemented");
}

void layer_hide_all(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;
    dt->layerManager().toggleHideAllLayers(true);
    Inkscape::DocumentUndo::maybeDone(dt->getDocument(), "layer:hideall", RC_("Undo", "Hide all layers"), "");
}

void layer_unhide_all(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;
    dt->layerManager().toggleHideAllLayers(false);
    Inkscape::DocumentUndo::maybeDone(dt->getDocument(), "layer:showall", RC_("Undo", "Show all layers"), "");
}

void layer_hide_toggle(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    auto layer = dt->layerManager().currentLayer();

    if (!layer || dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    } else {
        layer->setHidden(!layer->isHidden());
    }
}

void layer_hide_toggle_others(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    auto layer = dt->layerManager().currentLayer();

    if (!layer || dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    } else {
        dt->layerManager().toggleLayerSolo(layer); // Weird name!
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Hide other layers"), "");
    }
}

void layer_lock_all(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    dt->layerManager().toggleLockAllLayers(true);
    Inkscape::DocumentUndo::maybeDone(dt->getDocument(), "layer:lockall", RC_("Undo", "Lock all layers"), "");
}

void layer_unlock_all(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    dt->layerManager().toggleLockAllLayers(false);
    Inkscape::DocumentUndo::maybeDone(dt->getDocument(), "layer:unlockall", RC_("Undo", "Unlock all layers"), "");
}

void layer_lock_toggle(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    auto layer = dt->layerManager().currentLayer();

    if (!layer || dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    } else {
        layer->setLocked(!layer->isLocked());
    }
}

void layer_lock_toggle_others(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;
    auto layer = dt->layerManager().currentLayer();

    if (!layer || dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    } else {
        dt->layerManager().toggleLockOtherLayers(layer);
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Lock other layers"), "");
    }
}

void layer_previous(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    SPObject* next = Inkscape::next_layer(dt->layerManager().currentRoot(), dt->layerManager().currentLayer());
    if (next) {
        dt->layerManager().setCurrentLayer(next);
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Switch to next layer"),
                                     INKSCAPE_ICON("layer-previous"));
        dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Switched to next layer."));
    } else {
        dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot go past last layer."));
    }
}

void layer_next(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    SPObject* prev = Inkscape::previous_layer(dt->layerManager().currentRoot(), dt->layerManager().currentLayer());
    if (prev) {
        dt->layerManager().setCurrentLayer(prev);
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Switch to previous layer"),
                                     INKSCAPE_ICON("layer-next"));
        dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Switched to previous layer."));
    } else {
        dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot go before first layer."));
    }
}

void selection_move_to_layer_above(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    // Layer Rise
    dt->getSelection()->toNextLayer();
}

void selection_move_to_layer_below(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    // Layer Lower
    dt->getSelection()->toPrevLayer();
}

void selection_move_to_layer(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;
    (void)dt;

    // Selection move to layer
    // QT TODO: Inkscape::UI::Dialog::LayerPropertiesDialog::showMove(dt, dt->layerManager().currentLayer());
    g_warning("selection_move_to_layer not implemented");
}

void layer_top(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    if (dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    SPItem* layer = dt->layerManager().currentLayer();
    g_return_if_fail(layer != nullptr);
    SPObject* old_pos = layer->getNext();
    layer->raiseToTop();

    if (layer->getNext() != old_pos) {
        const char* message = g_strdup_printf(_("Raised layer <b>%s</b>."), layer->defaultLabel());
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Layer to top"), INKSCAPE_ICON("layer-top"));
        dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free((void*)message);

    } else {
        dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
    }
}

void layer_raise(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    if (dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    SPItem* layer = dt->layerManager().currentLayer();
    g_return_if_fail(layer != nullptr);

    SPObject* old_pos = layer->getNext();

    layer->raiseOne();

    if (layer->getNext() != old_pos) {
        const char* message = g_strdup_printf(_("Raised layer <b>%s</b>."), layer->defaultLabel());
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Raise layer"), INKSCAPE_ICON("layer-raise"));
        dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free((void*)message);

    } else {
        dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
    }
}

void layer_lower(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    if (dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    SPItem* layer = dt->layerManager().currentLayer();
    g_return_if_fail(layer != nullptr);
    SPObject* old_pos = layer->getNext();
    layer->lowerOne();

    if (layer->getNext() != old_pos) {
        const char* message = g_strdup_printf(_("Lowered layer <b>%s</b>."), layer->defaultLabel());
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Lower layer"), INKSCAPE_ICON("layer-lower"));
        dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free((void*)message);

    } else {
        dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
    }
}

void layer_bottom(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;

    if (dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    SPItem* layer = dt->layerManager().currentLayer();
    g_return_if_fail(layer != nullptr);
    SPObject* old_pos = layer->getNext();
    layer->lowerToBottom();

    if (layer->getNext() != old_pos) {
        const char* message = g_strdup_printf(_("Lowered layer <b>%s</b>."), layer->defaultLabel());
        Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Layer to bottom"), INKSCAPE_ICON("layer-bottom"));
        dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free((void*)message);

    } else {
        dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
    }
}

void layer_to_group(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) return;
    auto layer = dt->layerManager().currentLayer();

    if (!layer || dt->layerManager().isRoot()) {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    layer->setLayerMode(SPGroup::GROUP);
    layer->updateRepr(SP_OBJECT_WRITE_NO_CHILDREN | SP_OBJECT_WRITE_EXT);
    dt->getSelection()->set(layer);
    Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Layer to group"), INKSCAPE_ICON("dialog-objects"));
}

void layer_from_group(LineaWindow* win) {
    auto dt = win->get_desktop();
    if (!dt) return;
    auto selection = dt->getSelection();

    auto obj = selection->single();
    if (!obj) {
        show_output("layer_to_group: only one selected item allowed!");
        return;
    }

    if (auto group = cast<SPGroup>(obj)) {
        if (!group->isLayer()) {
            group->setLayerMode(SPGroup::LAYER);
            group->updateRepr(SP_OBJECT_WRITE_NO_CHILDREN | SP_OBJECT_WRITE_EXT);
            selection->set(group);
            Inkscape::DocumentUndo::done(dt->getDocument(), RC_("Undo", "Group to layer"),
                                         INKSCAPE_ICON("dialog-objects"));
        } else {
            dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Group already layer."));
        }
    } else {
        dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Selection is not a group."));
    }
}

// Does not change XML.
void group_enter(LineaWindow* win) {
    auto dt = win->get_desktop();
    if (!dt) return;
    auto selection = dt->getSelection();

    auto obj = selection->single();
    if (is<SPGroup>(obj)) {
        // Only one item and it is a group!
        dt->layerManager().setCurrentLayer(obj);
        selection->clear();
    }
}

// Does not change XML.
void group_exit(LineaWindow* win) {
    auto dt = win->get_desktop();
    if (!dt) return;
    auto selection = dt->getSelection();

    auto parent = dt->layerManager().currentLayer()->parent;
    dt->layerManager().setCurrentLayer(parent);

    auto obj = selection->single();
    if (obj && is<SPGroup>(obj->parent)) {
        // Only one item selected and the parent is a group!
        selection->set(obj->parent);
    } else {
        selection->clear();
    }
}

} // namespace

const Glib::ustring SECTION_LAYER = NC_("Action Section", "Layer");
const Glib::ustring SECTION_SELECT = NC_("Action Section", "Select");

static auto layer_action_defs = std::to_array<WindowActionDef>({
    // clang-format off
    {"layer-new",                       N_("Add Layer"),                        SECTION_LAYER,     N_("Create a new layer"),                          layer_new, "layer-new"},
    {"layer-new-above",                 N_("Add Layer Above"),                  SECTION_LAYER,     N_("Create a new layer above current"),            layer_new_above_action},
    {"layer-new-below",                 N_("Add Layer Below"),                  SECTION_LAYER,     N_("Create a new layer below current"),            layer_new_below_action},
    {"layer-new-child",                 N_("Add Layer as Child"),               SECTION_LAYER,     N_("Create a new layer as child of current"),      layer_new_child_action},
    {"layer-duplicate",                 N_("Duplicate Current Layer"),          SECTION_LAYER,     N_("Duplicate the current layer"),                 layer_duplicate},
    {"layer-delete",                    N_("Delete Current Layer"),             SECTION_LAYER,     N_("Delete the current layer"),                    layer_delete},
    {"layer-rename",                    N_("Rename Layer"),                     SECTION_LAYER,     N_("Rename the current layer"),                    layer_rename},

    {"layer-hide-toggle",               N_("Show/Hide Current Layer"),          SECTION_LAYER,     N_("Toggle visibility of current layer"),         layer_hide_toggle},
    {"layer-hide-toggle-others",        N_("Hide/Show Other Layers"),           SECTION_LAYER,     N_("Toggle visibility of other layers"),          layer_hide_toggle_others},
    {"layer-hide-all",                  N_("Hide All Layers"),                  SECTION_LAYER,     N_("Hide all layers"),                            layer_hide_all},
    {"layer-unhide-all",                N_("Show All Layers"),                  SECTION_LAYER,     N_("Show all layers"),                            layer_unhide_all},
    {"layer-lock-toggle",               N_("Lock/Unlock Current Layer"),        SECTION_LAYER,     N_("Toggle lock on current layer"),               layer_lock_toggle},
    {"layer-lock-toggle-others",        N_("Lock/Unlock Other Layers"),         SECTION_LAYER,     N_("Toggle lock on other layers"),                layer_lock_toggle_others},
    {"layer-lock-all",                  N_("Lock All Layers"),                  SECTION_LAYER,     N_("Lock all layers"),                            layer_lock_all},
    {"layer-unlock-all",                N_("Unlock All Layers"),                SECTION_LAYER,     N_("Unlock all layers"),                          layer_unlock_all},

    {"layer-previous",                  N_("Switch to Layer Above"),            SECTION_LAYER,     N_("Switch to the layer above the current"),       layer_previous},
    {"layer-next",                      N_("Switch to Layer Below"),            SECTION_LAYER,     N_("Switch to the layer below the current"),       layer_next},

    {"selection-move-to-layer-above",   N_("Move Selection to Layer Above"),    SECTION_LAYER,     N_("Move selection to the layer above the current"), selection_move_to_layer_above},
    {"selection-move-to-layer-below",   N_("Move Selection to Layer Below"),    SECTION_LAYER,     N_("Move selection to the layer below the current"), selection_move_to_layer_below},
    {"selection-move-to-layer",         N_("Move Selection to Layer..."),       SECTION_LAYER,     N_("Move selection to layer"),                      selection_move_to_layer},

    {"layer-top",                       N_("Layer to Top"),                     SECTION_LAYER,     N_("Raise the current layer to the top"),          layer_top},
    {"layer-raise",                     N_("Raise Layer"),                      SECTION_LAYER,     N_("Raise the current layer"),                       layer_raise, "move-up"},
    {"layer-lower",                     N_("Lower Layer"),                      SECTION_LAYER,     N_("Lower the current layer"),                       layer_lower, "move-down"},
    {"layer-bottom",                    N_("Layer to Bottom"),                  SECTION_LAYER,     N_("Lower the current layer to the bottom"),       layer_bottom},

    {"layer-to-group",                  N_("Layer to Group"),                   SECTION_LAYER,     N_("Convert the current layer to a group"),        layer_to_group},
    {"layer-from-group",                N_("Layer from Group"),                 SECTION_LAYER,     N_("Convert the group to a layer"),                  layer_from_group},

    //se use Layer technology even if they don't act on layers.
    {"selection-group-enter",           N_("Enter Group"),                      SECTION_SELECT,    N_("Enter group"),                                   group_enter},
    {"selection-group-exit",            N_("Exit Group"),                       SECTION_SELECT,    N_("Exit group"),                                    group_exit},
    // clang-format on
});

void add_actions_layer(LineaWindow* win) {
    auto& registry = ActionRegistry::get();

    for (auto& e : layer_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, win]() { fn(win); });
        win->addAction(a);
    }

#if 0
    // clang-format off
    win->add_action("layer-new",                            sigc::bind(sigc::ptr_fun(&layer_new), win));
    win->add_action("layer-new-above",                      sigc::bind(sigc::ptr_fun(&layer_new_above), win));
    win->add_action("layer-duplicate",                      sigc::bind(sigc::ptr_fun(&layer_duplicate), win));
    win->add_action("layer-delete",                         sigc::bind(sigc::ptr_fun(&layer_delete), win));
    win->add_action("layer-rename",                         sigc::bind(sigc::ptr_fun(&layer_rename), win));

    win->add_action("layer-hide-all",                       sigc::bind(sigc::ptr_fun(&layer_hide_all), win));
    win->add_action("layer-unhide-all",                     sigc::bind(sigc::ptr_fun(&layer_unhide_all), win));
    win->add_action("layer-hide-toggle",                    sigc::bind(sigc::ptr_fun(&layer_hide_toggle), win));
    win->add_action("layer-hide-toggle-others",             sigc::bind(sigc::ptr_fun(&layer_hide_toggle_others), win));

    win->add_action("layer-lock-all",                       sigc::bind(sigc::ptr_fun(&layer_lock_all), win));
    win->add_action("layer-unlock-all",                     sigc::bind(sigc::ptr_fun(&layer_unlock_all), win));
    win->add_action("layer-lock-toggle",                    sigc::bind(sigc::ptr_fun(&layer_lock_toggle), win));
    win->add_action("layer-lock-toggle-others",             sigc::bind(sigc::ptr_fun(&layer_lock_toggle_others), win));

    win->add_action("layer-previous",                       sigc::bind(sigc::ptr_fun(&layer_previous), win));
    win->add_action("layer-next",                           sigc::bind(sigc::ptr_fun(&layer_next), win));

    win->add_action("selection-move-to-layer-above",        sigc::bind(sigc::ptr_fun(&selection_move_to_layer_above), win));
    win->add_action("selection-move-to-layer-below",        sigc::bind(sigc::ptr_fun(&selection_move_to_layer_below), win));
    win->add_action("selection-move-to-layer",              sigc::bind(sigc::ptr_fun(&selection_move_to_layer), win));

    win->add_action("layer-top",                            sigc::bind(sigc::ptr_fun(&layer_top), win));
    win->add_action("layer-raise",                          sigc::bind(sigc::ptr_fun(&layer_raise), win));
    win->add_action("layer-lower",                          sigc::bind(sigc::ptr_fun(&layer_lower), win));
    win->add_action("layer-bottom",                         sigc::bind(sigc::ptr_fun(&layer_bottom), win));

    win->add_action("layer-to-group",                       sigc::bind(sigc::ptr_fun(&layer_to_group), win));
    win->add_action("layer-from-group",                     sigc::bind(sigc::ptr_fun(&layer_from_group), win));

    win->add_action("selection-group-enter",                sigc::bind(sigc::ptr_fun(&group_enter), win));
    win->add_action("selection-group-exit",                 sigc::bind(sigc::ptr_fun(&group_exit), win));
    // clang-format on

    auto app = InkscapeApplication::instance();
    if (!app) {
        show_output("add_actions_layer: no app!");
        return;
    }
    app->get_action_extra_data().add_data(raw_data_layer);
#endif
}
