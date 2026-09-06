// SPDX-License-Identifier: GPL-2.0-or-later
/** Self-contained Qt view of an SVG filter's primitives and connections. */
#ifndef LINEA_UI_FILTER_FILTER_PRIMITIVE_LIST_H
#define LINEA_UI_FILTER_FILTER_PRIMITIVE_LIST_H

#include <QPoint>
#include <QVector>
#include <QWidget>

class SPFilter;
class SPFilterPrimitive;

namespace Linea::UI {

namespace FilterGraphGeometry {
inline constexpr int single_input_height = 22;
inline constexpr int node_height = single_input_height * 3 / 4;
inline constexpr int node_half_height = node_height / 2;
inline constexpr int node_width = node_height * 3 / 4;
inline constexpr int route_offset = node_width - 1;
inline constexpr int input_circle_radius = node_height / 5;
}

class FilterPrimitiveList final : public QWidget {
    Q_OBJECT
public:
    explicit FilterPrimitiveList(QWidget* parent = nullptr);

    void setFilter(SPFilter* filter);
    void setShowAllSources(bool show);
    SPFilter* filter() const { return _filter; }
    SPFilterPrimitive* selectedPrimitive() const { return _selected; }
    void selectPrimitive(SPFilterPrimitive* primitive);

Q_SIGNALS:
    void primitiveSelected(SPFilterPrimitive* primitive);
    void primitiveChanged(SPFilterPrimitive* primitive);
    void primitiveContextMenu(SPFilterPrimitive* primitive, const QPoint& globalPosition);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    struct Row {
        SPFilterPrimitive* primitive = nullptr;
        int y = 0;
        int inputs = 1;
        int height = 0;
    };
    QVector<Row> rows() const;
    int visibleInputCount() const;
    int rowAt(const QPoint& point) const;
    int inputAt(const Row& row, const QPoint& point) const;
    void commitConnection(const QPoint& point);
    void sanitizeConnections();
    QString primitiveLabel(SPFilterPrimitive* primitive) const;
    QString inputName(SPFilterPrimitive* primitive, int input) const;
    int inputSlot(SPFilterPrimitive* primitive, int input) const;
    void drawNode(QPainter& painter, const QPointF& point, bool active) const;

    SPFilter* _filter = nullptr;
    SPFilterPrimitive* _selected = nullptr;
    SPFilterPrimitive* _dragPrimitive = nullptr;
    int _dragInput = -1;
    QPoint _dragPoint;
    bool _dragging = false;
    bool _reordering = false;
    SPFilterPrimitive* _reorderPrimitive = nullptr;
    bool _showAllSources = false;

    static constexpr int SourceWidth = 18;
    static constexpr int LabelWidth = 170;
};

} // namespace Linea::UI

#endif // LINEA_UI_FILTER_FILTER_PRIMITIVE_LIST_H
