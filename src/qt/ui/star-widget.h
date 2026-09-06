// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * StarWidget - Widget for editing star and polygon properties.
 *
 */

#ifndef LINEA_UI_STAR_WIDGET_H
#define LINEA_UI_STAR_WIDGET_H

#include <memory>
#include <QWidget>

namespace Ui {
class StarWidget;
}

namespace Linea {

namespace Props {
class Binder;
}

namespace UI {

/**
 * Widget for editing star / polygon properties.
 *
 * Uses a grid layout:
 * - shape toggle (star / polygon)
 * - number of sides
 * - spoke ratio
 * - rounded and randomized values
 */
class StarWidget : public QWidget {
    Q_OBJECT

public:
    explicit StarWidget(QWidget* parent = nullptr);
    ~StarWidget() override;

    // Bind the star-specific edits and the star/polygon toggle.
    void bind(Props::Binder& binder);

private:
    std::unique_ptr<Ui::StarWidget> _ui;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_STAR_WIDGET_H
