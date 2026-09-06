// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * EllipseWidget - Widget for editing ellipse/arc properties.
 *
 */

#ifndef LINEA_UI_ELLIPSE_WIDGET_H
#define LINEA_UI_ELLIPSE_WIDGET_H

#include <memory>
#include <QWidget>

namespace Ui {
class EllipseWidget;
}

namespace Linea {

namespace Props {
class Binder;
}

namespace UI {

/**
 * Widget for editing ellipse/arc properties.
 *
 * Uses a grid layout:
 * - Row 0: "Center" label (span 2), Cx, Cy
 * - Row 1: "Radii" label (span 2), Rx, Ry
 * - Row 2: "Cut out" label (span 2), from, to
 * - Row 3: "Mode" label, box with 4 mode buttons
 */
class EllipseWidget : public QWidget {
    Q_OBJECT

public:
    explicit EllipseWidget(QWidget* parent = nullptr);
    ~EllipseWidget() override;

    // Bind center/radii/angle edits and arc-mode buttons;
    // also installs the visibility rule (shown when the selection is all ellipses).
    void bind(Props::Binder& binder);

private:
    void roundCxCy();

    std::unique_ptr<Ui::EllipseWidget> _ui;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_ELLIPSE_WIDGET_H
