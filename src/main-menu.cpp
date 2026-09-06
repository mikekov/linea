// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main menu bar construction
 */

#include "main-menu.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include "actions/action-registry.h"

namespace {

struct Item {
    const char* action = nullptr; // action id, null = separator or submenu
    const char* label = nullptr;  // non-null = submenu with this label
    std::initializer_list<Item> children;
};

void build(QMenu* menu, const std::initializer_list<Item>& items, bool iconsVisible = false) {
    for (const auto& item : items) {
        if (item.children.size() > 0) {
            build(menu->addMenu(item.label ? QObject::tr(item.label) : QString()), item.children, iconsVisible);
        } else if (item.action) {
            auto a = ActionRegistry::get().action(item.action);
            a->setIconVisibleInMenu(iconsVisible);
            menu->addAction(a);
        } else {
            menu->addSeparator();
        }
    }
}

} // namespace

void createMainMenu(QMenuBar* bar) {

    // clang-format off
    static auto file_menu = std::initializer_list<Item>{
        {"document-new"},
        {"document-open"},
        {"document-import"},
        {},
        {"document-save"},
        {"document-save-as"},
        {},
        {"document-revert"},
        {"document-cleanup"},
        {"document-close"},
    };

    build(bar->addMenu("&File"), file_menu);

    static auto edit_menu = std::initializer_list<Item>{
        {"undo"},
        {"redo"},
        {},
        {"cut"},
        {"copy"},
        {"paste"},
        {nullptr, "Paste...", {
             {"paste-in-place"},
             {"paste-on-page"},
             {},
             {"paste-style"},
         }},
        {},
        {"duplicate"},
        {nullptr, "Clone", {
             {"clone"},
             {"clone-unlink"},
             {"clone-unlink-recursively"},
             {"clone-link"},
             {"select-original"},
             {"clone-link-lpe"},
         }},
        {"selection-make-bitmap-copy"},
        {},
        {"delete"},
        {},
        {"select-all"},
        {"select-all-layers"},
        {nullptr, "Select Same", {
             {"select-same-fill-and-stroke"},
             {"select-same-fill"},
             {"select-same-stroke-color"},
             {"select-same-stroke-style"},
             {"select-same-object-type"},
         }},
        {"select-invert"},
        {"select-none"},
        // {},
        // {"page-fit-to-selection"},
    };

    build(bar->addMenu("&Edit"), edit_menu);

    static auto view_menu = std::initializer_list<Item>{
        {nullptr, "Zoom", {
            {"canvas-zoom-in"},
            {"canvas-zoom-out"},
            {},
            {"canvas-zoom-1-1"},
            {"canvas-zoom-1-2"},
            {"canvas-zoom-2-1"},
            {},
            {"canvas-zoom-selection"},
            {"canvas-zoom-drawing"},
            {"canvas-zoom-page"},
            {"canvas-zoom-page-width"},
            {"canvas-zoom-center-page"},
            {},
            {"canvas-snapshot-set"},
            {"canvas-snapshot-toggle"},
            {},
            {"canvas-zoom-prev"},
            {"canvas-zoom-next"},
        }},
        {nullptr, "Orientation", {
            {"canvas-rotate-cw"},
            {"canvas-rotate-ccw"},
            {"canvas-rotate-reset"},
            {"canvas-rotate-lock"},
            {},
            {"canvas-flip-horizontal"},
            {"canvas-flip-vertical"},
            {"canvas-flip-reset"},
        }},
        {nullptr, "Pixel preview", {
            {"canvas-pixel-preview-toggle"},
            {"canvas-pixel-preview-100"},
            {"canvas-pixel-preview-200"},
        }},
        {},
        {"canvas-display-mode-toggle"},
        {},
        {"snap-global-toggle"},
        {"show-grids"},
        {"show-all-guides"},
        {"toggle-panel-docking"},
        {"view-color-palette"},
        {"view-rulers"},
        // {"view-fullscreen"}, - fullscreen gets inserted automatically on macos
    };

    build(bar->addMenu("&View"), view_menu);

    static auto layer_menu = std::initializer_list<Item>{
        {"layer-new"},
        {"layer-rename"},
        {},
        {"layer-hide-toggle"},
        {"layer-lock-toggle"},
        {},
        {"layer-previous"},
        {"layer-next"},
        {},
        {"selection-move-to-layer-above"},
        {"selection-move-to-layer-below"},
        {"selection-move-to-layer"},
        {},
        {"layer-top"},
        {"layer-raise"},
        {"layer-lower"},
        {"layer-bottom"},
        {},
        {"layer-duplicate"},
        {"layer-delete"},
    };

    build(bar->addMenu("&Layer"), layer_menu);

    static auto object_menu = std::initializer_list<Item>{
        {"selection-group"},
        {"selection-ungroup"},
        {"selection-ungroup-pop"},
        {},
        {nullptr, "Clip", {
            {"object-set-clip"},
            {"object-set-inverse-clip"},
            {"object-release-clip"},
            {"object-set-clip-group"},
        }},
        {nullptr, "Mask", {
            {"object-set-mask"},
            {"object-set-inverse-mask"},
            {"object-release-mask"},
        }},
        {nullptr, "Pattern", {
            {"object-to-pattern"},
            {"pattern-to-object"},
        }},
        {},
        {"object-to-marker"},
        {"object-to-guides"},
        {},
        {"selection-top"},
        {"selection-raise"},
        {"selection-lower"},
        {"selection-bottom"},
        {},
        {"object-rotate-90-cw"},
        {"object-rotate-90-ccw"},
        {"object-flip-horizontal"},
        {"object-flip-vertical"},
        {},
        {"unhide-all"},
        {"unlock-all"},
    };

    build(bar->addMenu("&Object"), object_menu);

    static auto path_menu = std::initializer_list<Item>{
        {"object-to-path"},
        {"object-stroke-to-path"},
        {},
        {"path-union"},
        {"path-difference"},
        {"path-intersection"},
        {"path-exclusion"},
        {"path-division"},
        {"path-cut"},
        {},
        {"path-combine"},
        {"path-break-apart"},
        {"path-split"},
        {"path-fracture"},
        {"path-flatten"},
        {},
        {"path-inset"},
        {"path-outset"},
        {"path-offset-dynamic"},
        {"path-offset-linked"},
        {},
        {"path-fill-between-paths"},
        {},
        {"path-simplify"},
        {"path-reverse"},
        // {},
        // {"paste-path-effect"},
        // {"remove-path-effect"},
    };

    build(bar->addMenu("&Path"), path_menu, true);

    static auto plugins_menu = std::initializer_list<Item>{
        {"dialog-open-extension-gallery"},
        {"last-effect"},
        {"last-effect-pref"},
        {},
        {"dialog-open-filter-gallery"},
        {"dialog-open-filter-editor"},
    };

    build(bar->addMenu("&Plug-ins"), plugins_menu);

    static auto help_menu = std::initializer_list<Item>{
        {"about-linea"},
    };

    build(bar->addMenu("&Help"), help_menu);

    // clang-format on
}
