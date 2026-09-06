// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#include "element-properties.h"

#include "live_effects/effect-enum.h"
#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpeobject-reference.h"
#include "object/sp-ellipse.h"
#include "object/sp-flowtext.h"
#include "object/sp-image.h"
#include "object/sp-item-group.h"
#include "object/sp-item.h"
#include "object/sp-line.h"
#include "object/sp-path.h"
#include "object/sp-rect.h"
#include "object/sp-star.h"
#include "util/text-utils.h"

namespace Linea {

namespace detail {

const LivePathEffectObject* find_lpeffect(SPLPEItem* item, ::Inkscape::LivePathEffect::EffectType etype) {
    if (!item) return nullptr;

    auto lpe = item->getFirstPathEffectOfType(etype);
    if (!lpe) return nullptr;

    return lpe->getLPEObj();
}

std::optional<double> get_number(SPItem* item, const char* attribute) {
    if (!item) return {};

    auto val = item->getAttribute(attribute);
    if (!val) return {};

    return item->getRepr()->getAttributeDouble(attribute);
}

void merge_item_properties(ElementState& props, SPItem* item) {
    if (!item) return;

    props.count.items++;

    // description/title
    {
        const char* title = item->title();
        const char* desc = item->desc();
        props.title.merge(QString(title ? title : ""));
        props.description.merge(QString(desc ? desc : ""));
    }

    props.locked.merge(!item->isSensitive());

    if (auto rect = cast<SPRect>(item)) {
        props.count.rectangles++;
        // props.rectangles.set(props.rectangles.value() + 1);
        // props.rect_rx.merge(rect->getVisibleRx());   maybe?
        props.rect_rx.merge(rect->rx.value);
        props.rect_ry.merge(rect->ry.value);
        auto lpe = find_lpeffect(rect, Inkscape::LivePathEffect::FILLET_CHAMFER);
        props.rect_round_corners.set(rect->rx.value > 0 || rect->ry.value > 0 || lpe);
    }
    else if (auto ellipse = cast<SPGenericEllipse>(item)) {
        props.count.ellipses++;
        // props.ellipse_cx.merge(ell->getVisibleCx());   maybe?
        props.ellipse_cx.merge(ellipse->cx.value);
        props.ellipse_cy.merge(ellipse->cy.value);
        // props.ellipse_rx.merge(ell->getVisibleRx());   maybe?
        props.ellipse_rx.merge(ellipse->rx.value);
        props.ellipse_ry.merge(ellipse->ry.value);
        props.ellipse_start_angle.merge(ellipse->start);
        props.ellipse_end_angle.merge(ellipse->end);
    }
    else if (auto star = cast<SPStar>(item)) {
        if (star->flatsided) {
            props.count.polygons++;
            // props.polygons.set(props.polygons.value() + 1);
        } else {
            props.count.stars++;
            // props.stars.set(props.stars.value() + 1);
        }
        props.star_sides.merge(star->sides);
        props.star_flatsided.set(star->flatsided);
        double r1 = get_number(star, "sodipodi:r1").value_or(0.5);
        double r2 = get_number(star, "sodipodi:r2").value_or(0.5);
        props.star_r1.merge(r1);
        props.star_r2.merge(r2);
        props.star_rounded.merge(star->rounded);
        props.star_randomized.merge(star->randomized);
    }
    else if (auto image = cast<SPImage>(item)) {
        props.count.images++;
        // TODO: handle image properties
    }
    else if (auto path = cast<SPPath>(item)) {
        props.count.paths++;
        // TODO: handle path properties
    }
    else if (auto line = cast<SPLine>(item)) {
        props.count.lines++;
        // TODO: handle line properties
    }
    else if (auto group = cast<SPGroup>(item)) {
        if (group->isLayer()) {
            props.count.layers++;
        }
        else {
            //todo: handle mask helpers
            props.count.groups++;
            // TODO: handle group properties
        }
    }
    else {
        if (is_textual_item(item)) {
            props.count.textual++;
        }
        if (is<SPFlowtext>(item)) {
            props.count.flowtext++;
        }

        // more types to handle...
    }

}

} // namespace detail

} // namespace Linea
