// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RectangleWidget - Widget for editing rectangle corner radius.
 *
 */

#ifndef LINEA_UI_RECTANGLE_WIDGET_H
#define LINEA_UI_RECTANGLE_WIDGET_H

#include <memory>
#include <QWidget>

namespace Ui {
class RectangleWidget;
}

namespace Linea {

namespace Props {
class Binder;
}

namespace UI {

/**
 * Widget for editing rectangle corner radius.
 *
 * Uses a vertical layout:
 * - Row 1: "Corners" label
 * - Row 2: NumberEdit widgets for rx and ry
 * - Row 3: Sharp and rounded corners toggle buttons
 */
class RectangleWidget : public QWidget {
    Q_OBJECT

public:
    explicit RectangleWidget(QWidget* parent = nullptr);
    ~RectangleWidget() override;

    // Bind rx/ry edits and corner buttons; also installs
    // the visibility rule (shown when the selection is all rectangles).
    void bind(Props::Binder& binder);

private:
    std::unique_ptr<Ui::RectangleWidget> _ui;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_RECTANGLE_WIDGET_H
