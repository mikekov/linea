// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Helper object for showing selected items
 *
 * Authors:
 *   bulia byak <bulia@users.sf.net>
 *   Carl Hetherington <inkscape@carlh.net>
 *   Abhishek Sharma
 *
 * Copyright (C) 2004 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "selcue.h"

#include <memory>

#include "desktop.h"
#include "display/control/canvas-item-ctrl.h"
#include "display/control/canvas-item-guideline.h"
#include "display/control/canvas-item-rect.h"
#include "object/sp-flowtext.h"
#include "object/sp-text.h"
#include "selection.h"
#include "text-editing.h"

namespace Inkscape {

SelCue::BoundingBoxPrefsObserver::BoundingBoxPrefsObserver(SelCue &sel_cue)
    : Observer("/tools/bounding_box")
    , _sel_cue(sel_cue)
{}

void SelCue::BoundingBoxPrefsObserver::notify(Preferences::Entry const &val)
{
    _sel_cue._boundingBoxPrefsChanged(static_cast<int>(val.getBool()));
}

SelCue::SelCue(SPDesktop *desktop)
    : _desktop(desktop)
    , _bounding_box_prefs_observer(*this)
{
    _selection = _desktop->getSelection();

    _sel_changed_connection = _selection->connectChanged(sigc::hide(sigc::mem_fun(*this, &SelCue::_newItemBboxes)));

    {
        void (SelCue::*modifiedSignal)() = &SelCue::_updateItemBboxes;
        _sel_modified_connection =
            _selection->connectModified(sigc::hide(sigc::hide(sigc::mem_fun(*this, modifiedSignal))));
    }

    Preferences *prefs = Preferences::get();
    _updateItemBboxes(prefs);
    prefs->addObserver(_bounding_box_prefs_observer);
}

SelCue::~SelCue()
{
    _sel_changed_connection.disconnect();
    _sel_modified_connection.disconnect();
}

void SelCue::_updateItemBboxes()
{
    _updateItemBboxes(Preferences::get());
}

void SelCue::_updateItemBboxes(Preferences *prefs)
{
    gint mode = prefs->getInt("/options/selcue/value", MARK);
    if (mode == NONE) {
        return;
    }

    g_return_if_fail(_selection != nullptr);

    int prefs_bbox = prefs->getBool("/tools/bounding_box");

    _updateItemBboxes(mode, prefs_bbox);
}

SelCue::ItemCue::ItemCue(SPDesktop* desktop, int cue_mode, const Geom::Rect& b, bool visible)
    : mode(cue_mode)
{
    if (mode == MARK) {
        mark_bl = make_canvasitem<CanvasItemCtrl>(desktop->getCanvasControls(), CANVAS_ITEM_CTRL_TYPE_SELCUE,
                                                  Geom::Point(b.min().x(), b.max().y()));
        mark_tr = make_canvasitem<CanvasItemCtrl>(desktop->getCanvasControls(), CANVAS_ITEM_CTRL_TYPE_SELCUE_2,
                                                  Geom::Point(b.max().x(), b.min().y()));
        for (auto *ctrl : {mark_bl.get(), mark_tr.get()}) {
            ctrl->set_pickable(false);
            ctrl->lower_to_bottom(); // Just low enough to not get in the way of other draggable knots.
            ctrl->set_visible(visible);
        }
    } else if (mode == BBOX) {
        bbox = make_canvasitem<CanvasItemRect>(desktop->getCanvasControls(), b);
        bbox->set_stroke(0xffffffa0);
        bbox->set_shadow(0x0000c0a0, 1);
        bbox->set_dashed(true);
        bbox->set_inverted(false);
        bbox->set_pickable(false);
        bbox->lower_to_bottom(); // Just low enough to not get in the way of other draggable knots.
        bbox->set_visible(visible);
    }
}

void SelCue::ItemCue::update(const Geom::OptRect& b, bool visible) {
    bool const show = visible && b.has_value();

    if (mark_bl) {
        mark_bl->set_visible(show);
        if (b) mark_bl->set_position(Geom::Point(b->min().x(), b->max().y()));
    }
    if (mark_tr) {
        mark_tr->set_visible(show);
        if (b) mark_tr->set_position(Geom::Point(b->max().x(), b->min().y()));
    }
    if (bbox) {
        bbox->set_visible(show);
        if (b) bbox->set_rect(*b);
    }
}

void SelCue::_updateItemBboxes(gint mode, int prefs_bbox)
{
    auto items = _selection->items();
    if (_item_cues.size() != static_cast<size_t>(std::ranges::distance(items)) ||
        (!_item_cues.empty() && _item_cues.front().mode != mode)) {
        _newItemBboxes();
        return;
    }

    size_t i = 0;
    for (auto item : items) {
        Geom::OptRect const bbox = (prefs_bbox == 0) ? item->desktopVisualBounds() : item->desktopGeometricBounds();
        _item_cues[i++].update(bbox, _bboxes_visible);
    }

    _newItemLines();
    _newTextBaselines();
}

void SelCue::_newItemBboxes()
{
    _item_cues.clear();

    Preferences *prefs = Preferences::get();
    gint mode = prefs->getInt("/options/selcue/value", MARK);
    if (mode == NONE) {
        return;
    }

    g_return_if_fail(_selection != nullptr);

    int prefs_bbox = prefs->getBool("/tools/bounding_box");

    auto items = _selection->items();
    for (auto item : items) {
        if (auto bbox = (prefs_bbox == 0) ? item->desktopVisualBounds() : item->desktopGeometricBounds()) {
            _item_cues.emplace_back(_desktop, mode, *bbox, _bboxes_visible);
        }
    }

    _newItemLines();
    _newTextBaselines();
}

/**
 * Create any required visual-only guide lines related to the selection.
 */
void SelCue::_newItemLines()
{
    _item_lines.clear();

    auto bbox = _selection->preferredBounds();

    // Show a set of lines where the anchor is.
    if (_selection->has_anchor && bbox) {
        auto anchor = Geom::Scale(_selection->anchor);
        auto point = bbox->min() + (bbox->dimensions() * anchor);
        for (bool horz : {false, true}) {
            auto line = make_canvasitem<CanvasItemGuideLine>(_desktop->getCanvasGuides(), "", point, Geom::Point(!horz, horz));
            line->lower_to_bottom();
            line->set_visible(true);
            line->set_stroke(0xddddaa11);
            line->set_inverted(true);
            _item_lines.emplace_back(std::move(line));
        }
    }
}

void SelCue::_newTextBaselines()
{
    _text_baselines.clear();

    auto items = _selection->items();
    for (auto item : items) {
        std::optional<Geom::Point> pt;
        if (auto text = cast<SPText>(item)) {
            pt = text->getBaselinePoint();
        } else if (auto flow = cast<SPFlowtext>(item)) {
            pt = flow->getBaselinePoint();
        }
        if (pt) {
            auto canvas_item = make_canvasitem<CanvasItemCtrl>(_desktop->getCanvasControls(), CANVAS_ITEM_CTRL_TYPE_SIZER, (*pt) * item->i2dt_affine());
            canvas_item->set_size(Inkscape::HandleSize::XTINY);
            canvas_item->lower_to_bottom();
            canvas_item->set_visible(true);
            _text_baselines.emplace_back(std::move(canvas_item));
        }
    }
}

void SelCue::_boundingBoxPrefsChanged(int prefs_bbox)
{
    Preferences *prefs = Preferences::get();
    gint mode = prefs->getInt("/options/selcue/value", MARK);
    if (mode == NONE) {
        return;
    }

    g_return_if_fail(_selection != nullptr);

    _updateItemBboxes(mode, prefs_bbox);
}

void SelCue::setBboxesVisible(bool visible)
{
    _bboxes_visible = visible;
    _updateItemBboxes();
}

} // namespace Inkscape
