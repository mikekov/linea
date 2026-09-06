// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Dynamic Size Stacked Widget - QStackedWidget that uses size hint of current widget only.
 */

#ifndef LINEA_UI_DYNAMIC_STACKED_WIDGET_H
#define LINEA_UI_DYNAMIC_STACKED_WIDGET_H

#include <QStackedWidget>

namespace Linea::UI {

/**
 * Custom stacked widget that uses size hint of current widget only.
 *
 * Standard QStackedWidget uses the size hint of the largest widget in the stack,
 * which can cause scrollbars to appear even when the current widget is smaller.
 * This class overrides sizeHint() and minimumSizeHint() to return the size hints
 * of only the currently visible widget.
 */
class DynamicSizeStackedWidget : public QStackedWidget {
    Q_OBJECT

public:
    explicit DynamicSizeStackedWidget(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
};

} // namespace Linea::UI

#endif // LINEA_UI_DYNAMIC_STACKED_WIDGET_H
