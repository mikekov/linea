// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Dynamic Size Stacked Widget implementation.
 */

#include "dynamic-stacked-widget.h"

namespace Linea::UI {

DynamicSizeStackedWidget::DynamicSizeStackedWidget(QWidget* parent)
    : QStackedWidget(parent) {}

QSize DynamicSizeStackedWidget::sizeHint() const {
    if (currentWidget()) {
        return currentWidget()->sizeHint();
    }
    return QStackedWidget::sizeHint();
}

QSize DynamicSizeStackedWidget::minimumSizeHint() const {
    if (currentWidget()) {
        return currentWidget()->minimumSizeHint();
    }
    return QStackedWidget::minimumSizeHint();
}

} // namespace Linea::UI
