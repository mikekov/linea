// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt editor for SVG filter matrix-valued attributes.
 */

#ifndef LINEA_UI_FILTER_MATRIX_EDITOR_WIDGET_H
#define LINEA_UI_FILTER_MATRIX_EDITOR_WIDGET_H

#include <QVector>
#include <QWidget>

class SPFilterPrimitive;
class QGridLayout;

namespace Linea::UI {

/** Editor for the space-separated matrix values of an SVG filter primitive. */
class MatrixEditorWidget final : public QWidget {
    Q_OBJECT

public:
    explicit MatrixEditorWidget(QWidget* parent = nullptr);

    void setPrimitive(SPFilterPrimitive* primitive, const QString& attribute, int rows, int columns);
    SPFilterPrimitive* primitive() const { return _primitive; }
    QString attribute() const { return _attribute; }

Q_SIGNALS:
    void primitiveChanged(SPFilterPrimitive* primitive, const QString& attribute, const QString& value);

private:
    void rebuild();
    void writeValues();

    SPFilterPrimitive* _primitive = nullptr;
    QString _attribute;
    int _rows = 0;
    int _columns = 0;
    QWidget* _gridWidget = nullptr;
    QGridLayout* _grid = nullptr;
    QVector<class SpinScale*> _edits;
    bool _loading = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_FILTER_MATRIX_EDITOR_WIDGET_H
