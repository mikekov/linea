// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * Qt gradient selector widget
 */

#ifndef SEEN_GRADIENT_SELECTOR_QT_H
#define SEEN_GRADIENT_SELECTOR_QT_H

#include <QWidget>
#include <QPixmap>
#include <memory>
#include <vector>
#include <map>

#include "object/sp-paint-server-data.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class GradientSelector;
}
QT_END_NAMESPACE

class SPDocument;
class SPGradient;
class SPStop;
class SPObject;

namespace Linea::UI {

class GradientSelector : public QWidget {
    Q_OBJECT

public:
    enum SelectorMode { MODE_LINEAR, MODE_RADIAL, MODE_SWATCH };

    explicit GradientSelector(QWidget* parent = nullptr);
    ~GradientSelector() override;

    // GradientSelectorInterface implementation
    void setGradient(SPGradient* gradient);
    SPGradient* getVector() const;
    void setVector(SPDocument* doc, SPGradient* vector);
    void setMode(SelectorMode mode);
    void setUnits(SPGradientUnits units);
    SPGradientUnits getUnits() const;
    void setSpread(SPGradientSpread spread);
    SPGradientSpread getSpread() const;

    // UI configuration
    void showEditButton(bool show);
    void setNameColSize(int minWidth);
    void setGradientSize(int width, int height);

Q_SIGNALS:
    void signalGrabbed();
    void signalDragged();
    void signalReleased();
    void signalChanged(SPGradient* gradient);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
    void onAddClicked();
    void onDeleteClicked();
    void onDeleteUnusedClicked();
    void onEditClicked();
    void onTableSelectionChanged();
    void onTableCellChanged(int row, int column);
    void onNameHeaderClicked();
    void onColorHeaderClicked();
    void onCountHeaderClicked();
    void onKeyPressed(int key);

private:
    void setupConnections();
    void updateTable();
    void selectGradientInTable(SPGradient* vector);
    void checkDeleteButton();
    void vectorSet(SPGradient* gr);
    void moveSelection(int amount, bool down = true, bool toEnd = false);
    void getUsageCounts(SPDocument* doc, std::map<SPGradient*, int>& usageCount);

    std::unique_ptr<Ui::GradientSelector> _ui;

    // State
    bool _blocked = false;
    SelectorMode _mode = MODE_LINEAR;
    SPGradientUnits _gradientUnits = SP_GRADIENT_UNITS_USERSPACEONUSE;
    SPGradientSpread _gradientSpread = SP_GRADIENT_SPREAD_PAD;

    // Document and gradient
    SPDocument* _doc = nullptr;
    SPGradient* _gradient = nullptr;

    // Gradient list
    std::vector<SPGradient*> _gradients;
    std::map<SPGradient*, int> _usageCounts;

    // Gradient image size
    int _pixWidth = 64;
    int _pixHeight = 18;

    // Sorting
    int _sortColumn = 1;  // Default sort by name
    Qt::SortOrder _sortOrder = Qt::AscendingOrder;

    // Widget groups for mode switching
    std::vector<QWidget*> _nonsolidWidgets;
    std::vector<QWidget*> _swatchWidgets;
};

} // namespace Linea::UI

#endif // SEEN_GRADIENT_SELECTOR_QT_H
