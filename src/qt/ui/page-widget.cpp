// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Page widget.
 */

#include "page-widget.h"

#include "number-range.h"
#include "page-popup.h"
#include "popup-menu.h"
#include "props/binder.h"
#include "props/property-def.h"
#include "ui_page-widget.h"

namespace Linea::UI {

PageWidget::PageWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PageWidget>()) {
    _ui->setupUi(this);

    _ui->pageWidth->setRange(0.001, NumberRange::maximum);
    _ui->pageWidth->setDecimals(NumberRange::decimals);
    _ui->pageHeight->setRange(0.001, NumberRange::maximum);
    _ui->pageHeight->setDecimals(NumberRange::decimals);

    _popup = new PopupMenu(this);
    _pagePopup = new PagePopup;
    _popup->setContent(_pagePopup);

    connect(_ui->marginsButton, &QPushButton::clicked, this, &PageWidget::onMarginsClicked);
}

PageWidget::~PageWidget() = default;

void PageWidget::bind(Props::Binder& binder) {
    binder.visibleWhen(this, Props::Cond::single<&Props::Counts::pages, &Props::Counts::svgs>);
    binder.bind(Props::page_width, _ui->pageWidth);
    binder.bind(Props::page_height, _ui->pageHeight);
    _pagePopup->bind(binder);
}

void PageWidget::onMarginsClicked() {
    _popup->showBelowWidget(_ui->marginsButton);
}

} // namespace Linea::UI
