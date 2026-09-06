// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PaintOrderWidget — reorderable stack showing paint order (fill, stroke, markers).
 *
 * Rows can be reordered by dragging the handle. The visual order from top to
 * bottom represents the paint order from front (top) to back (bottom).
 */

#ifndef LINEA_UI_PAINT_ORDER_WIDGET_H
#define LINEA_UI_PAINT_ORDER_WIDGET_H

#include <vector>
#include <QWidget>

class QVBoxLayout;
class SPIPaintOrder;

namespace Linea::UI {

class PaintOrderWidget : public QWidget {
    Q_OBJECT

public:
    explicit PaintOrderWidget(QWidget* parent = nullptr);

    // Add an option row (call during construction)
    void addOption(const QString& label, const QString& icon, const QString& tooltip, int value);

    // Set the row order from a vector of values
    void setValues(const std::vector<int>& values);

    // Get the current row order as a vector of values
    std::vector<int> getValues() const;

    // Show or hide a specific row by its value
    void setRowVisible(int value, bool visible);

    // Convenience: set from SPIPaintOrder
    void setValue(const SPIPaintOrder& po, bool hasMarkers);

    // Convenience: get as SPIPaintOrder
    SPIPaintOrder getValue() const;

Q_SIGNALS:
    void orderChanged();

private:
    struct Row {
        QWidget* widget = nullptr;
        int value = 0;
    };

    void moveRow(int fromIndex, int toIndex);
    int indexAtY(int globalY) const;
    int rowIndexOf(QWidget* widget) const;

    class RowWidget;
    std::vector<Row> _rows;
    QVBoxLayout* _layout = nullptr;
    RowWidget* _draggedRow = nullptr;
    int _dragStartIndex = -1;
};

} // namespace Linea::UI

#endif // LINEA_UI_PAINT_ORDER_WIDGET_H
