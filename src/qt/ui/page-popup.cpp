// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Page popup — margins and bleed editor for the selected page.
 *
 * Mirrors the margin popover in upstream's page toolbar: four margin edits in
 * a box-model cross plus a uniform bleed edit. Values are shown and entered
 * in the document's display unit; reads/writes go through the props model.
 */

#include "page-popup.h"

#include "number-range.h"
#include "props/binder.h"
#include "props/property-def.h"
#include "ui_page-popup.h"

namespace Linea::UI {

PagePopup::PagePopup(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PagePopup>()) {
    _ui->setupUi(this);

    for (auto edit : {_ui->marginTop, _ui->marginRight, _ui->marginBottom, _ui->marginLeft,
                      _ui->bleedEdit}) {
        edit->setRange(0, NumberRange::maximum);
        edit->setDecimals(2);
    }
}

PagePopup::~PagePopup() = default;

void PagePopup::bind(Props::Binder& binder) {
    binder.bind(Props::page_margin_top, _ui->marginTop);
    binder.bind(Props::page_margin_right, _ui->marginRight);
    binder.bind(Props::page_margin_bottom, _ui->marginBottom);
    binder.bind(Props::page_margin_left, _ui->marginLeft);
    binder.bind(Props::page_bleed, _ui->bleedEdit);
}

} // namespace Linea::UI
