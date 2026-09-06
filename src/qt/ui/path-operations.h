// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PathOperations - Widget with path operation buttons.
 */

#ifndef LINEA_UI_PATH_OPERATIONS_H
#define LINEA_UI_PATH_OPERATIONS_H

#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class PathOperations;
}
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * Widget with path operation buttons arranged in a grid.
 *
 * Row 0: Title label "Path Operations"
 * Row 1: Union, Difference, Intersection, Exclusion, Division, Cut Path
 * Row 2: Combine, Break Apart, Split Path, Fracture, Flatten
 *
 * Each button triggers the corresponding app action via ActionRegistry.
 */
class PathOperations : public QWidget {
    Q_OBJECT

public:
    explicit PathOperations(QWidget* parent = nullptr);
    ~PathOperations() override;

private:
    std::unique_ptr<Ui::PathOperations> _ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_PATH_OPERATIONS_H
