// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * AdaptiveStack — QStackedWidget that sizes to the current page,
 * not the tallest page.
 */

#ifndef LINEA_UI_ADAPTIVE_STACK_H
#define LINEA_UI_ADAPTIVE_STACK_H

#include <QStackedWidget>

namespace Linea::UI {

/**
 * QStackedWidget subclass whose sizeHint() and minimumSizeHint()
 * reflect the currently visible page rather than the maximum of all pages.
 */
class AdaptiveStack : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;

    QSize sizeHint() const override {
        if (!currentWidget()) return QStackedWidget::sizeHint();
        auto hint = currentWidget()->sizeHint();
        // hint.setWidth(QStackedWidget::sizeHint().width());
        return hint;
    }

    QSize minimumSizeHint() const override {
        if (!currentWidget()) return QStackedWidget::minimumSizeHint();
        auto hint = currentWidget()->minimumSizeHint();
        // hint.setWidth(QStackedWidget::minimumSizeHint().width());
        return hint;
    }
};

} // namespace Linea::UI

#endif // LINEA_UI_ADAPTIVE_STACK_H
