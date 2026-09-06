// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DisplayWidget — display units, rendering settings, and visual preferences.
 */

#include "display-widget.h"

#include <QVBoxLayout>

#include "document-undo.h"
#include "document.h"
#include "object/sp-namedview.h"
#include "page-manager.h"
#include "util/units.h"

using Inkscape::DocumentUndo;

namespace Linea::UI {

namespace {

void set_namedview_bool(SPNamedView* nv, Inkscape::Util::Internal::ContextString operation, SPAttr key, bool on) {
    if (!nv || !nv->document) return;
    nv->change_bool_setting(key, on);
    nv->document->setModifiedSinceSave();
    DocumentUndo::done(nv->document, operation, "");
}

void set_namedview_color(SPNamedView* nv, const char* key, Inkscape::Util::Internal::ContextString operation,
                         SPAttr color_key, SPAttr opacity_key, const Inkscape::Colors::Color& color) {
    if (!nv || !nv->document) return;
    if (opacity_key != SPAttr::INVALID) {
        nv->change_color(color_key, opacity_key, color);
    } else {
        nv->change_color(color_key, color);
    }
    nv->document->setModifiedSinceSave();
    DocumentUndo::maybeDone(nv->document, key, operation, "");
}

void display_unit_change(SPDocument* doc, SPNamedView* nv, const Inkscape::Util::Unit* unit) {
    if (!nv || !unit) return;
    nv->setAttribute("inkscape:document-units", unit->abbr);
    DocumentUndo::done(doc, RC_("Undo", "Set display unit"), "");
}

} // anonymous namespace

// ---------------------------------------------------------------------------

DisplayWidget::DisplayWidget(QWidget* parent)
    : QWidget(parent)
    , _page(new PageProperties()) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_page->rightGrid());

    connect(_page, &PageProperties::colorChanged, this, &DisplayWidget::onColorChanged);
    connect(_page, &PageProperties::checkToggled, this, &DisplayWidget::onCheckToggled);
    connect(_page, &PageProperties::unitChanged, this, &DisplayWidget::onUnitChanged);
}

void DisplayWidget::setDocument(SPDocument* document) {
    _document = document;
}

void DisplayWidget::onColorChanged(const Inkscape::Colors::Color& color, PageProperties::Color element) {
    if (_update.pending() || !_document) return;
    auto nv = _document->getNamedView();
    if (!nv) return;
    auto scoped = _update.block();
    switch (element) {
        case PageProperties::Color::Desk:
            set_namedview_color(nv, "document-color-desk", RC_("Undo", "Desk color"), SPAttr::INKSCAPE_DESK_COLOR,
                                SPAttr::INKSCAPE_DESK_OPACITY, color);
            break;
        case PageProperties::Color::Background:
            set_namedview_color(nv, "document-color-background", RC_("Undo", "Background color"), SPAttr::PAGECOLOR,
                                SPAttr::INVALID, color);
            break;
        case PageProperties::Color::Border:
            set_namedview_color(nv, "document-color-border", RC_("Undo", "Border color"), SPAttr::BORDERCOLOR,
                                SPAttr::BORDEROPACITY, color);
            break;
    }
}

void DisplayWidget::onCheckToggled(bool checked, PageProperties::Check element) {
    if (_update.pending() || !_document) return;
    auto nv = _document->getNamedView();
    if (!nv) return;
    auto scoped = _update.block();
    switch (element) {
        case PageProperties::Check::Border:
            set_namedview_bool(nv, RC_("Undo", "Toggle page border"), SPAttr::SHOWBORDER, checked);
            break;
        case PageProperties::Check::BorderOnTop:
            set_namedview_bool(nv, RC_("Undo", "Toggle border on top"), SPAttr::BORDERLAYER, checked);
            break;
        case PageProperties::Check::Shadow:
            set_namedview_bool(nv, RC_("Undo", "Toggle page shadow"), SPAttr::SHOWPAGESHADOW, checked);
            break;
        case PageProperties::Check::AntiAlias:
            set_namedview_bool(nv, RC_("Undo", "Toggle anti-aliasing"), SPAttr::INKSCAPE_ANTIALIAS_RENDERING, checked);
            break;
        case PageProperties::Check::ClipToPage:
            set_namedview_bool(nv, RC_("Undo", "Toggle clip to page mode"), SPAttr::INKSCAPE_CLIP_TO_PAGE_RENDERING,
                               checked);
            break;
        case PageProperties::Check::PageLabelStyle:
            set_namedview_bool(nv, RC_("Undo", "Toggle page label style"), SPAttr::PAGELABELSTYLE, checked);
            break;
        default:
            break;
    }
}

void DisplayWidget::onUnitChanged(const Inkscape::Util::Unit* unit, PageProperties::Units element) {
    if (_update.pending() || !_document) return;
    if (element == PageProperties::Units::Display) {
        display_unit_change(_document, _document->getNamedView(), unit);
    }
}

void DisplayWidget::updateDisplayUnitUi(SPNamedView* nv) {
    if (!nv || !nv->display_units) return;
    _page->setUnit(PageProperties::Units::Display, QString::fromStdString(nv->display_units->abbr));
}

void DisplayWidget::update(SPNamedView* nv) {
    if (_update.pending() || !nv || !_document) return;
    auto scoped = _update.block();

    updateDisplayUnitUi(nv);
    // _page->setCheck(PageProperties::Check::Checkerboard, nv->desk_checkerboard);
    _page->setColor(PageProperties::Color::Desk, nv->getDeskColor());
    _page->setCheck(PageProperties::Check::AntiAlias, nv->antialias_rendering);
    _page->setCheck(PageProperties::Check::ClipToPage, nv->clip_to_page);

    auto& pm = _document->getPageManager();
    _page->setColor(PageProperties::Color::Background, pm.getBackgroundColor());
    _page->setCheck(PageProperties::Check::Border, pm.getShowBorder());
    _page->setCheck(PageProperties::Check::BorderOnTop, pm.getBorderOnTop());
    _page->setColor(PageProperties::Color::Border, pm.getBorderColor());
    _page->setCheck(PageProperties::Check::Shadow, pm.getShowShadow());
    _page->setCheck(PageProperties::Check::PageLabelStyle, pm.getLabelStyle() != "default");
}

} // namespace Linea::UI
