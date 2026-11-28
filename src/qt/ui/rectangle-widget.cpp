// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RectangleWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "rectangle-widget.h"

#include <QMenu>
#include <QToolButton>

#include "ui_rectangle-widget.h"
#include "props/binder.h"

#include "document-undo.h"
#include "live_effects/effect.h"
#include "object/sp-lpe-item.h"
#include "util/cast.h"
#include "util-string/context-string.h"
#include "widget-utils.h"

namespace Linea::UI {

RectangleWidget::RectangleWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::RectangleWidget>()) {
    _ui->setupUi(this);
    fixColumnToolButtons(this);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

RectangleWidget::~RectangleWidget() = default;

void RectangleWidget::bind(Props::Binder& binder) {
    binder.bind(Props::rect_rx, _ui->rxEdit);
    binder.bind(Props::rect_ry, _ui->ryEdit);

    auto editor = binder.editor();

    // Corners menu: sharp removes the Corners LPE and zeroes both radii;
    // independent applies the LPE to every selected rect lacking it.
    auto menu = new QMenu(this);
    auto sharpAction = menu->addAction(
        QIcon(":/icons/rectangle-make-corners-sharp"), tr("Make corners sharp"));
    connect(sharpAction, &QAction::triggered, this, [editor] {
        if (auto desktop = editor->desktop()) {
            for (auto item : desktop->getSelection()->items()) {
                auto lpeitem = cast<SPLPEItem>(item);
                if (!lpeitem) continue;
                if (auto lpe = lpeitem->getFirstPathEffectOfType(Inkscape::LivePathEffect::FILLET_CHAMFER)) {
                    lpeitem->removePathEffect(lpe, false);
                }
            }
        }
        editor->set(Props::rect_rx, 0.0);
        editor->set(Props::rect_ry, 0.0);
    });
    auto roundedAction = menu->addAction(
        QIcon(":/icons/rectangle-make-corners-round"), tr("Make corners independent"));
    connect(roundedAction, &QAction::triggered, this, [editor] {
        auto desktop = editor->desktop();
        if (!desktop) return;
        auto doc = desktop->getDocument();
        for (auto item : desktop->getSelection()->items()) {
            auto lpeitem = cast<SPLPEItem>(item);
            if (lpeitem && !lpeitem->hasPathEffectOfType(Inkscape::LivePathEffect::FILLET_CHAMFER)) {
                Inkscape::LivePathEffect::Effect::createAndApply(
                    Inkscape::LivePathEffect::FILLET_CHAMFER, doc, item);
            }
        }
        Inkscape::DocumentUndo::done(doc, RC_("Undo", "Make corners independent"), "");
    });
    _ui->cornersMenuButton->setMenu(menu);

    // Sharp is enabled only when corners are currently rounded.
    binder.bindField(Props::Field::rect_round_corners, [sharpAction](const Props::SelectionState& s) {
        auto& rounded = s.element.rect_round_corners;
        sharpAction->setEnabled(rounded.is_mixed() || (rounded.is_single() && rounded.value()));
    });

    // Shown when the selection consists solely of rectangles.
    binder.visibleWhen(this, Props::Cond::allOf<&Props::Counts::rectangles>);
}

} // namespace Linea::UI
