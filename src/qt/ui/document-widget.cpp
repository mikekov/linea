// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DocumentWidget — page size, viewbox, coordinate system, and scale settings.
 */

#include "document-widget.h"

#include <cmath>
#include <optional>
#include <QVBoxLayout>
#include <2geom/rect.h>
#include <2geom/transforms.h>

#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "object/sp-grid.h"
#include "object/sp-guide.h"
#include "object/sp-namedview.h"
#include "object/sp-root.h"
#include "page-manager.h"
#include "selection.h"
#include "svg/svg-length.h"
#include "util/units.h"

using Inkscape::DocumentUndo;
using Inkscape::Util::Quantity;
using Inkscape::Util::Unit;
using Inkscape::Util::UnitTable;

namespace Linea::UI {

namespace {

void set_namedview_bool(SPNamedView* nv, Inkscape::Util::Internal::ContextString operation, SPAttr key, bool on) {
    if (!nv || !nv->document) return;
    nv->change_bool_setting(key, on);
    nv->document->setModifiedSinceSave();
    DocumentUndo::done(nv->document, operation, "");
}

void set_document_dimensions(SPDocument* doc, double width, double height, const Unit* unit) {
    if (!doc) return;
    auto new_w = Quantity(width, unit);
    auto new_h = Quantity(height, unit);
    auto rect = Geom::Rect(Geom::Point(0, 0), Geom::Point(new_w.value("px"), new_h.value("px")));
    auto const old_height_q = doc->getHeight();
    doc->fitToRect(rect, false);

    // The origin for the user is in the lower left corner; this point should remain stationary when
    // changing the page size. The SVG's origin however is in the upper left corner, so we must compensate.
    if (!doc->yaxisdown()) {
        auto const vert_offset = Geom::Translate(Geom::Point(0, old_height_q.value("px") - new_h.value("px")));
        doc->getRoot()->translateChildItems(vert_offset);
    } else {
        // When yaxisdown is true, we need to translate just the guides.
        // See https://gitlab.com/inkscape/inkscape/-/issues/1230
        if (auto nv = doc->getNamedView()) {
            for (auto guide : nv->guides) {
                guide->moveto(guide->getPoint() * Geom::Translate(0, 0), true);
            }
        }
    }

    // Set width/height with the new units so the SVG attributes reflect the chosen unit (e.g., mm for A4).
    doc->setWidthAndHeight(new_w, new_h, true);

    DocumentUndo::done(doc, RC_("Undo", "Set page size"), "");
}

void set_document_viewbox_pos(SPDocument* doc, double x, double y) {
    if (!doc) return;

    auto box = doc->getViewBox();
    doc->setViewBox(Geom::Rect::from_xywh(x, y, box.width(), box.height()));
    DocumentUndo::done(doc, RC_("Undo", "Set viewbox position"), "");
}

void set_document_viewbox_size(SPDocument* doc, double width, double height) {
    if (!doc) return;

    auto box = doc->getViewBox();
    doc->setViewBox(Geom::Rect::from_xywh(box.min()[Geom::X], box.min()[Geom::Y], width, height));
    DocumentUndo::done(doc, RC_("Undo", "Set viewbox size"), "");
}

void set_document_scale_helper(SPDocument& doc, double scale) {
    if (scale <= 0) return;
    auto root = doc.getRoot();
    auto box = doc.getViewBox();
    doc.setViewBox(Geom::Rect::from_xywh(box.min()[Geom::X], box.min()[Geom::Y], root->width.value / scale,
                                         root->height.value / scale));
}

void set_document_scale(SPDocument* doc, double scale) {
    if (!doc || scale <= 0) return;

    set_document_scale_helper(*doc, scale);
    DocumentUndo::done(doc, RC_("Undo", "Set page scale"), "");
}

void set_content_scale(SPDocument* doc, double scale) {
    if (!doc || scale <= 0) return;

    auto old_scale = doc->getDocumentScale(false);
    auto delta = old_scale * Geom::Scale(scale).inverse();
    doc->scaleContentBy(delta);
    doc->getPageManager().scalePages(delta);
    if (auto nv = doc->getNamedView()) {
        for (auto grid : nv->grids) {
            grid->scale(delta);
        }
    }
}

std::optional<Geom::Scale> get_document_scale_helper(SPDocument& doc) {
    auto root = doc.getRoot();
    if (root && root->width._set && root->width.unit != SVGLength::PERCENT && root->height._set &&
        root->height.unit != SVGLength::PERCENT) {
        if (root->viewBox_set) {
            auto vw = root->viewBox.width();
            auto vh = root->viewBox.height();
            if (vw > 0 && vh > 0) return Geom::Scale(root->width.value / vw, root->height.value / vh);
        } else {
            auto w = root->width.computed;
            auto h = root->height.computed;
            if (w > 0 && h > 0) return Geom::Scale(root->width.value / w, root->height.value / h);
        }
    }
    return {};
}

} // anonymous namespace

// ---------------------------------------------------------------------------

DocumentWidget::DocumentWidget(QWidget* parent)
    : QWidget(parent)
    , _page(new PageProperties())
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_page->leftGrid());

    connect(_page, &PageProperties::checkToggled, this, &DocumentWidget::onCheckToggled);
    connect(_page, &PageProperties::dimensionChanged, this, &DocumentWidget::onDimensionChanged);
    connect(_page, &PageProperties::resizeToFit, this, &DocumentWidget::onResizeToFit);
}

void DocumentWidget::setDocument(SPDocument* document) {
    _document = document;
}

void DocumentWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void DocumentWidget::onCheckToggled(bool checked, PageProperties::Check element) {
    if (_update.pending() || !_document) return;

    auto scoped = _update.block();
    auto nv = _document->getNamedView();
    switch (element) {
        case PageProperties::Check::YAxisPointsDown:
            set_namedview_bool(nv, RC_("Undo", "Toggle system coordinate Y axis orientation"),
                               SPAttr::INKSCAPE_Y_AXIS_DOWN, checked);
            break;
        case PageProperties::Check::OriginCurrentPage:
            set_namedview_bool(nv, RC_("Undo", "Toggle system coordinate origin correction"),
                               SPAttr::INKSCAPE_ORIGIN_CORRECTION, checked);
            break;
        default:
            break;
    }
}

void DocumentWidget::onDimensionChanged(double x, double y, const Unit* unit, PageProperties::Dimension element) {
    if (_update.pending() || !_document) return;

    auto scoped = _update.block();
    switch (element) {
        case PageProperties::Dimension::PageTemplate:
        case PageProperties::Dimension::PageSize:
            set_document_dimensions(_document, x, y, unit);
            updateViewboxUi(_document);
            break;
        case PageProperties::Dimension::ViewboxSize:
            set_document_viewbox_size(_document, x, y);
            break;
        case PageProperties::Dimension::ViewboxPosition:
            set_document_viewbox_pos(_document, x, y);
            break;
        case PageProperties::Dimension::ScaleContent:
            set_content_scale(_document, x);
            // todo: doc-properties dialog calls both, is this right?
            set_document_scale(_document, x);
            updateViewboxUi(_document);
            break;
        case PageProperties::Dimension::Scale:
            set_document_scale(_document, x);
            updateViewboxUi(_document);
            break;
    }
    updateScaleUi(_document);
}

void DocumentWidget::onResizeToFit() {
    if (_update.pending() || !_document || !_desktop) return;

    auto scoped = _update.block();
    auto& pm = _document->getPageManager();
    pm.selectPage(0);
    pm.fitToSelection(_desktop->getSelection(), true);
    DocumentUndo::done(_document, RC_("Undo", "Resize page to fit"), "");
}

void DocumentWidget::updateViewboxUi(SPDocument* document) {
    if (!document) return;
    auto vb = document->getViewBox();
    _page->setDimension(PageProperties::Dimension::ViewboxPosition, vb.min()[Geom::X], vb.min()[Geom::Y]);
    _page->setDimension(PageProperties::Dimension::ViewboxSize, vb.width(), vb.height());
}

void DocumentWidget::updateScaleUi(SPDocument* document) {
    if (!document) return;

    if (auto scale = get_document_scale_helper(*document)) {
        auto sx = (*scale)[Geom::X];
        auto sy = (*scale)[Geom::Y];
        bool uniform = std::fabs(sx - sy) < 0.0001;
        _page->setDimension(PageProperties::Dimension::Scale, sx, sx);
        _page->setCheck(PageProperties::Check::NonuniformScale, !uniform);
        _page->setCheck(PageProperties::Check::DisabledScale, false);
    } else {
        _page->setDimension(PageProperties::Dimension::Scale, 1, 1);
        _page->setCheck(PageProperties::Check::NonuniformScale, false);
        _page->setCheck(PageProperties::Check::DisabledScale, true);
    }
}

void DocumentWidget::update(SPNamedView* nv, SPRoot* root) {
    if (_update.pending() || !root) return;

    auto scoped = _update.block();

    double doc_w = root->width.value;
    QString doc_w_unit = QString::fromStdString(UnitTable::get().getUnit(root->width.unit)->abbr);
    bool percent = (doc_w_unit == "%");
    if (doc_w_unit.isEmpty()) {
        doc_w_unit = "px";
    } else if (doc_w_unit == "%" && root->viewBox_set) {
        doc_w_unit = "px";
        doc_w = root->viewBox.width();
    }

    double doc_h = root->height.value;
    QString doc_h_unit = QString::fromStdString(UnitTable::get().getUnit(root->height.unit)->abbr);
    percent = percent || (doc_h_unit == "%");
    if (doc_h_unit.isEmpty()) {
        doc_h_unit = "px";
    } else if (doc_h_unit == "%" && root->viewBox_set) {
        doc_h_unit = "px";
        doc_h = root->viewBox.height();
    }

    _page->setCheck(PageProperties::Check::UnsupportedSize, percent);
    _page->setDimension(PageProperties::Dimension::PageSize, doc_w, doc_h);
    _page->setUnit(PageProperties::Units::Document, doc_w_unit);

    if (auto doc = root->document) {
        updateViewboxUi(doc);
        updateScaleUi(doc);
    }

    if (nv) {
        _page->setCheck(PageProperties::Check::YAxisPointsDown, nv->is_y_axis_down());
        _page->setCheck(PageProperties::Check::OriginCurrentPage, nv->get_origin_follows_page());
    }
}

} // namespace Linea::UI
