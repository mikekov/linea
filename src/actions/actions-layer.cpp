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
#include "linea-application.h"
#include "layer-manager.h"
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

void xlayer_new(SPDesktop* dt) {
    (void)dt;

    // New Layer
    // QT TODO: Inkscape::UI::Dialog::LayerPropertiesDialog::showCreate(dt, dt->layerManager().currentLayer());
    // warning
    g_warning("layer_new not implemented");
}

void add_new_layer(SPDesktop* desktop, LayerRelativePosition pos) {
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

void layer_new_above(SPDesktop* desktop, LayerRelativePosition pos) {
    add_new_layer(desktop, Inkscape::LPOS_ABOVE);
}

void layer_new(SPDesktop* desktop) {
    layer_new_above(desktop, Inkscape::LPOS_ABOVE);
}

void layer_new_below(SPDesktop* desktop, LayerRelativePosition pos) {
    add_new_layer(desktop, Inkscape::LPOS_BELOW);
}

void layer_new_child(SPDesktop* desktop, LayerRelativePosition pos) {
    add_new_layer(desktop, Inkscape::LPOS_CHILD);
}

void layer_new_above_action(SPDesktop* desktop) { layer_new_above(desktop, Inkscape::LPOS_ABOVE); }
void layer_new_below_action(SPDesktop* desktop) { layer_new_below(desktop, Inkscape::LPOS_BELOW); }
void layer_new_child_action(SPDesktop* desktop) { layer_new_child(desktop, Inkscape::LPOS_CHILD); }

void layer_duplicate(SPDesktop* desktop) {
    if (!desktop) return;

    if (!desktop->layerManager().isRoot()) {
        desktop->getSelection()->duplicate(true, true); // This requires the selection to be a layer!
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Duplicate layer"),
                                     INKSCAPE_ICON("layer-duplicate"));
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Duplicated layer."));

    } else {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    }
}

void layer_delete(SPDesktop* desktop) {
    if (!desktop) return;
    auto root = desktop->layerManager().currentRoot();

    if (!desktop->layerManager().isRoot()) {
        desktop->getSelection()->clear();
        SPObject* old_layer = desktop->layerManager().currentLayer();
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
            desktop->layerManager().setCurrentLayer(survivor);
        }

        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Delete layer"), INKSCAPE_ICON("layer-delete"));
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Deleted layer."));

    } else {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    }
}

void layer_rename(SPDesktop* desktop) {
    if (!desktop) return;
    (void)desktop;

    // Rename Layer
    // QT TODO: Inkscape::UI::Dialog::LayerPropertiesDialog::showRename(dt, dt->layerManager().currentLayer());
    g_warning("layer_rename not implemented");
}

void layer_hide_all(SPDesktop* desktop) {
    if (!desktop) return;
    desktop->layerManager().toggleHideAllLayers(true);
    Inkscape::DocumentUndo::maybeDone(desktop->getDocument(), "layer:hideall", RC_("Undo", "Hide all layers"), "");
}

void layer_unhide_all(SPDesktop* desktop) {
    if (!desktop) return;
    desktop->layerManager().toggleHideAllLayers(false);
    Inkscape::DocumentUndo::maybeDone(desktop->getDocument(), "layer:showall", RC_("Undo", "Show all layers"), "");
}

void layer_hide_toggle(SPDesktop* desktop) {
    auto layer = desktop->layerManager().currentLayer();

    if (!layer || desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    } else {
        layer->setHidden(!layer->isHidden());
    }
}

void layer_hide_toggle_others(SPDesktop* desktop) {
    auto layer = desktop->layerManager().currentLayer();

    if (!layer || desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    } else {
        desktop->layerManager().toggleLayerSolo(layer); // Weird name!
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Hide other layers"), "");
    }
}

void layer_lock_all(SPDesktop* desktop) {
    if (!desktop) return;
    desktop->layerManager().toggleLockAllLayers(true);
    Inkscape::DocumentUndo::maybeDone(desktop->getDocument(), "layer:lockall", RC_("Undo", "Lock all layers"), "");
}

void layer_unlock_all(SPDesktop* desktop) {
    if (!desktop) return;
    desktop->layerManager().toggleLockAllLayers(false);
    Inkscape::DocumentUndo::maybeDone(desktop->getDocument(), "layer:unlockall", RC_("Undo", "Unlock all layers"), "");
}

void layer_lock_toggle(SPDesktop* desktop) {
    auto layer = desktop->layerManager().currentLayer();

    if (!layer || desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    } else {
        layer->setLocked(!layer->isLocked());
    }
}

void layer_lock_toggle_others(SPDesktop* desktop) {
    if (!desktop) return;
    auto layer = desktop->layerManager().currentLayer();

    if (!layer || desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
    } else {
        desktop->layerManager().toggleLockOtherLayers(layer);
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Lock other layers"), "");
    }
}

void layer_previous(SPDesktop* desktop) {
    if (!desktop) return;

    SPObject* next = Inkscape::next_layer(desktop->layerManager().currentRoot(), desktop->layerManager().currentLayer());
    if (next) {
        desktop->layerManager().setCurrentLayer(next);
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Switch to next layer"),
                                     INKSCAPE_ICON("layer-previous"));
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Switched to next layer."));
    } else {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot go past last layer."));
    }
}

void layer_next(SPDesktop* desktop) {
    if (!desktop) return;

    SPObject* prev = Inkscape::previous_layer(desktop->layerManager().currentRoot(), desktop->layerManager().currentLayer());
    if (prev) {
        desktop->layerManager().setCurrentLayer(prev);
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Switch to previous layer"),
                                     INKSCAPE_ICON("layer-next"));
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Switched to previous layer."));
    } else {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot go before first layer."));
    }
}

void selection_move_to_layer_above(SPDesktop* desktop) {
    if (!desktop) return;

    // Layer Rise
    desktop->getSelection()->toNextLayer();
}

void selection_move_to_layer_below(SPDesktop* desktop) {
    if (!desktop) return;

    // Layer Lower
    desktop->getSelection()->toPrevLayer();
}

void selection_move_to_layer(SPDesktop* desktop) {
    if (!desktop) return;
    (void)desktop;

    // Selection move to layer
    // QT TODO: Inkscape::UI::Dialog::LayerPropertiesDialog::showMove(dt, dt->layerManager().currentLayer());
    g_warning("selection_move_to_layer not implemented");
}

void layer_top(SPDesktop* desktop) {
    if (!desktop) return;

    if (desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    SPItem* layer = desktop->layerManager().currentLayer();
    g_return_if_fail(layer != nullptr);
    SPObject* old_pos = layer->getNext();
    layer->raiseToTop();

    if (layer->getNext() != old_pos) {
        const char* message = g_strdup_printf(_("Raised layer <b>%s</b>."), layer->defaultLabel());
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Layer to top"), INKSCAPE_ICON("layer-top"));
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free((void*)message);

    } else {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
    }
}

void layer_raise(SPDesktop* desktop) {
    if (!desktop) return;

    if (desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    SPItem* layer = desktop->layerManager().currentLayer();
    g_return_if_fail(layer != nullptr);

    SPObject* old_pos = layer->getNext();

    layer->raiseOne();

    if (layer->getNext() != old_pos) {
        const char* message = g_strdup_printf(_("Raised layer <b>%s</b>."), layer->defaultLabel());
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Raise layer"), INKSCAPE_ICON("layer-raise"));
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free((void*)message);

    } else {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
    }
}

void layer_lower(SPDesktop* desktop) {
    if (!desktop) return;

    if (desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    SPItem* layer = desktop->layerManager().currentLayer();
    g_return_if_fail(layer != nullptr);
    SPObject* old_pos = layer->getNext();
    layer->lowerOne();

    if (layer->getNext() != old_pos) {
        const char* message = g_strdup_printf(_("Lowered layer <b>%s</b>."), layer->defaultLabel());
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Lower layer"), INKSCAPE_ICON("layer-lower"));
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free((void*)message);

    } else {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
    }
}

void layer_bottom(SPDesktop* desktop) {
    if (!desktop) return;

    if (desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    SPItem* layer = desktop->layerManager().currentLayer();
    g_return_if_fail(layer != nullptr);
    SPObject* old_pos = layer->getNext();
    layer->lowerToBottom();

    if (layer->getNext() != old_pos) {
        const char* message = g_strdup_printf(_("Lowered layer <b>%s</b>."), layer->defaultLabel());
        Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Layer to bottom"), INKSCAPE_ICON("layer-bottom"));
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free((void*)message);

    } else {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
    }
}

void layer_to_group(SPDesktop* desktop) {
    if (!desktop) return;
    auto layer = desktop->layerManager().currentLayer();

    if (!layer || desktop->layerManager().isRoot()) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
        return;
    }

    layer->setLayerMode(SPGroup::GROUP);
    layer->updateRepr(SP_OBJECT_WRITE_NO_CHILDREN | SP_OBJECT_WRITE_EXT);
    desktop->getSelection()->set(layer);
    Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Layer to group"), INKSCAPE_ICON("dialog-objects"));
}

void layer_from_group(SPDesktop* desktop) {
    if (!desktop) return;
    auto selection = desktop->getSelection();
    if (!selection) return;

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
            Inkscape::DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Group to layer"),
                                         INKSCAPE_ICON("dialog-objects"));
        } else {
            desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Group already layer."));
        }
    } else {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Selection is not a group."));
    }
}

// Does not change XML.
void group_enter(SPDesktop* desktop) {
    if (!desktop) return;
    auto selection = desktop->getSelection();

    auto obj = selection->single();
    if (is<SPGroup>(obj)) {
        // Only one item and it is a group!
        desktop->layerManager().setCurrentLayer(obj);
        selection->clear();
    }
}

// Does not change XML.
void group_exit(SPDesktop* desktop) {
    if (!desktop) return;
    auto selection = desktop->getSelection();

    auto parent = desktop->layerManager().currentLayer()->parent;
    desktop->layerManager().setCurrentLayer(parent);

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

static auto layer_action_defs = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    {"layer-new",                       N_("Add Layer"),                        SECTION_LAYER,     N_("Create a new layer"),                          "layer-new", layer_new},
    {"layer-new-above",                 N_("Add Layer Above"),                  SECTION_LAYER,     N_("Create a new layer above current"),            nullptr, layer_new_above_action},
    {"layer-new-below",                 N_("Add Layer Below"),                  SECTION_LAYER,     N_("Create a new layer below current"),            nullptr, layer_new_below_action},
    {"layer-new-child",                 N_("Add Layer as Child"),               SECTION_LAYER,     N_("Create a new layer as child of current"),      nullptr, layer_new_child_action},
    {"layer-duplicate",                 N_("Duplicate Current Layer"),          SECTION_LAYER,     N_("Duplicate the current layer"),                 nullptr, layer_duplicate},
    {"layer-delete",                    N_("Delete Current Layer"),             SECTION_LAYER,     N_("Delete the current layer"),                    nullptr, layer_delete},
    {"layer-rename",                    N_("Rename Layer"),                     SECTION_LAYER,     N_("Rename the current layer"),                    nullptr, layer_rename},

    {"layer-hide-toggle",               N_("Show/Hide Current Layer"),          SECTION_LAYER,     N_("Toggle visibility of current layer"),         nullptr, layer_hide_toggle},
    {"layer-hide-toggle-others",        N_("Hide/Show Other Layers"),           SECTION_LAYER,     N_("Toggle visibility of other layers"),          nullptr, layer_hide_toggle_others},
    {"layer-hide-all",                  N_("Hide All Layers"),                  SECTION_LAYER,     N_("Hide all layers"),                            nullptr, layer_hide_all},
    {"layer-unhide-all",                N_("Show All Layers"),                  SECTION_LAYER,     N_("Show all layers"),                            nullptr, layer_unhide_all},
    {"layer-lock-toggle",               N_("Lock/Unlock Current Layer"),        SECTION_LAYER,     N_("Toggle lock on current layer"),               nullptr, layer_lock_toggle},
    {"layer-lock-toggle-others",        N_("Lock/Unlock Other Layers"),         SECTION_LAYER,     N_("Toggle lock on other layers"),                nullptr, layer_lock_toggle_others},
    {"layer-lock-all",                  N_("Lock All Layers"),                  SECTION_LAYER,     N_("Lock all layers"),                            nullptr, layer_lock_all},
    {"layer-unlock-all",                N_("Unlock All Layers"),                SECTION_LAYER,     N_("Unlock all layers"),                          nullptr, layer_unlock_all},

    {"layer-previous",                  N_("Switch to Layer Above"),            SECTION_LAYER,     N_("Switch to the layer above the current"),       nullptr, layer_previous},
    {"layer-next",                      N_("Switch to Layer Below"),            SECTION_LAYER,     N_("Switch to the layer below the current"),       nullptr, layer_next},

    {"selection-move-to-layer-above",   N_("Move Selection to Layer Above"),    SECTION_LAYER,     N_("Move selection to the layer above the current"), nullptr, selection_move_to_layer_above},
    {"selection-move-to-layer-below",   N_("Move Selection to Layer Below"),    SECTION_LAYER,     N_("Move selection to the layer below the current"), nullptr, selection_move_to_layer_below},
    {"selection-move-to-layer",         N_("Move Selection to Layer..."),       SECTION_LAYER,     N_("Move selection to layer"),                      nullptr, selection_move_to_layer},

    {"layer-top",                       N_("Layer to Top"),                     SECTION_LAYER,     N_("Raise the current layer to the top"),          nullptr, layer_top},
    {"layer-raise",                     N_("Raise Layer"),                      SECTION_LAYER,     N_("Raise the current layer"),                       "move-up", layer_raise},
    {"layer-lower",                     N_("Lower Layer"),                      SECTION_LAYER,     N_("Lower the current layer"),                       "move-down", layer_lower},
    {"layer-bottom",                    N_("Layer to Bottom"),                  SECTION_LAYER,     N_("Lower the current layer to the bottom"),       nullptr, layer_bottom},

    {"layer-to-group",                  N_("Layer to Group"),                   SECTION_LAYER,     N_("Convert the current layer to a group"),        nullptr, layer_to_group},
    {"layer-from-group",                N_("Layer from Group"),                 SECTION_LAYER,     N_("Convert the group to a layer"),                  nullptr, layer_from_group},

    //se use Layer technology even if they don't act on layers.
    {"selection-group-enter",           N_("Enter Group"),                      SECTION_SELECT,    N_("Enter group"),                                   nullptr, group_enter},
    {"selection-group-exit",            N_("Exit Group"),                       SECTION_SELECT,    N_("Exit group"),                                    nullptr, group_exit},
    // clang-format on
});

void add_actions_layer(LineaApplication* app) {
    ActionRegistry::get().registerActions(app, layer_action_defs);

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
