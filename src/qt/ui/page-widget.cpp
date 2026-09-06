// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Page widget.
 */

#include "page-widget.h"

#include "props/binder.h"
#include "ui_page-widget.h"

namespace Linea::UI {

PageWidget::PageWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PageWidget>()) {
    _ui->setupUi(this);
}

PageWidget::~PageWidget() = default;

void PageWidget::bind(Props::Binder& binder) {
    binder.visibleWhen(this, Props::Cond::single<&Props::Counts::pages, &Props::Counts::svgs>);
}

} // namespace Linea::UI
