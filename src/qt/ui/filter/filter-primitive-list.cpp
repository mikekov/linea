// SPDX-License-Identifier: GPL-2.0-or-later
#include "filter-primitive-list.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainterPath>
#include <QObject>
#include <QStyle>
#include <algorithm>

#include "document-undo.h"
#include "filter-enums.h"
#include "inkgc/gc-alloc.h"
#include "object/filters/blend.h"
#include "object/filters/composite.h"
#include "object/filters/displacementmap.h"
#include "object/filters/merge.h"
#include "object/filters/mergenode.h"
#include "object/filters/sp-filter-primitive.h"
#include "object/sp-filter.h"
#include "ui/icon-names.h"
#include "xml/repr.h"

using namespace Inkscape;
using namespace Inkscape::Filters;

namespace Linea::UI {
namespace {

int input_count(SPFilterPrimitive* primitive) {
    if (!primitive) return 0;

    if (is<SPFeMerge>(primitive)) {
        int count = 0;
        for (auto& child : primitive->children) {
            if (is<SPFeMergeNode>(&child)) ++count;
        }
        return count + 1;
    }
    if (is<SPFeBlend>(primitive) || is<SPFeComposite>(primitive) || is<SPFeDisplacementMap>(primitive)) return 2;
    return 1;
}

bool isInput2(SPFilterPrimitive* primitive) {
    return is<SPFeBlend>(primitive) || is<SPFeComposite>(primitive) || is<SPFeDisplacementMap>(primitive);
}

QColor mix(QColor a, QColor b, int amount) {
    return QColor((a.red() * (100 - amount) + b.red() * amount) / 100,
                  (a.green() * (100 - amount) + b.green() * amount) / 100,
                  (a.blue() * (100 - amount) + b.blue() * amount) / 100);
}

void drawRoundedConnector(QPainter& painter, const QPoint& node, const QPoint& out) {
    constexpr qreal radius = FilterGraphGeometry::node_height / 3.0;
    const qreal direction = out.y() >= node.y() ? 1.0 : -1.0;
    QPainterPath path;
    path.moveTo(node);
    path.lineTo(out.x() - radius, node.y());
    path.quadTo(out.x(), node.y(), out.x(), node.y() + direction * radius);
    path.lineTo(out);
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.restore();
}

void drawSourceConnector(QPainter& painter, const QPoint& node, const QPoint& inputPoint,
                        const QColor& color) {
    painter.save();
    painter.setPen(color);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(node, inputPoint);
    painter.setBrush(color);
    painter.drawEllipse(inputPoint, FilterGraphGeometry::input_circle_radius,
                        FilterGraphGeometry::input_circle_radius);
    painter.restore();
}

} // namespace

FilterPrimitiveList::FilterPrimitiveList(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void FilterPrimitiveList::setShowAllSources(bool show) {
    if (_showAllSources == show) return;
    _showAllSources = show;
    updateGeometry();
    update();
}

void FilterPrimitiveList::setFilter(SPFilter* filter) {
    if (_filter == filter) {
        setMinimumHeight(sizeHint().height());
        updateGeometry();
        update();
        return;
    }
    _filter = filter;
    auto old = _selected;
    _selected = nullptr;
    SPFilterPrimitive* first = nullptr;
    if (_filter) {
        for (auto& child : _filter->children) {
            auto primitive = cast<SPFilterPrimitive>(&child);
            if (!primitive) continue;
            if (!first) first = primitive;
            if (primitive == old) {
                _selected = primitive;
                break;
            }
        }
    }
    if (!_selected) _selected = first;
    setMinimumHeight(sizeHint().height());
    updateGeometry();
    update();
    if (old != _selected) Q_EMIT primitiveSelected(_selected);
}

int FilterPrimitiveList::visibleInputCount() const {
    return _showAllSources ? FPINPUT_END : 2;
}

QVector<FilterPrimitiveList::Row> FilterPrimitiveList::rows() const {
    QVector<Row> result;
    if (!_filter) return result;
    int y = 0;
    for (auto& child : _filter->children) {
        auto primitive = cast<SPFilterPrimitive>(&child);
        if (!primitive) continue;
        const int inputs = input_count(primitive);
        const int height = FilterGraphGeometry::single_input_height * inputs;
        result.push_back({primitive, y, inputs, height});
        y += height;
    }
    return result;
}

int FilterPrimitiveList::rowAt(const QPoint& point) const {
    for (int i = 0; i < rows().size(); ++i) {
        auto row = rows().at(i);
        if (point.y() >= row.y && point.y() < row.y + row.height) return i;
    }
    return -1;
}

int FilterPrimitiveList::inputAt(const Row& row, const QPoint& point) const {
    const int x = width() - SourceWidth * visibleInputCount();
    if (point.x() < x || point.x() >= width()) return -1;
    const int offset = point.x() - x;
    const int input = offset / SourceWidth;
    if (offset % SourceWidth == SourceWidth - 1) return -1;
    return input >= 0 && input < row.inputs ? input : -1;
}

QString FilterPrimitiveList::primitiveLabel(SPFilterPrimitive* primitive) const {
    if (!primitive || !primitive->getRepr()) return {};
    const auto id = FPConverter.get_id_from_key(primitive->getRepr()->name());
    auto label = QObject::tr(FPConverter.get_label(id).c_str());
    if (auto primitiveId = primitive->getRepr()->attribute("id")) {
        label += QStringLiteral(" (") + QString::fromUtf8(primitiveId) + QLatin1Char(')');
    }
    return label;
}

QString FilterPrimitiveList::inputName(SPFilterPrimitive* primitive, int input) const {
    if (is<SPFeMerge>(primitive)) {
        int index = 0;
        for (auto& child : primitive->children) {
            if (is<SPFeMergeNode>(&child) && index++ == input)
                return QString::fromUtf8(cast<SPFeMergeNode>(&child)->getRepr()->attribute("in") ?: "");
        }
        return {};
    }
    if (input == 0)
        return primitive && primitive->getRepr() ? QString::fromUtf8(primitive->getRepr()->attribute("in") ?: "")
                                                 : QString();
    return primitive && primitive->getRepr() ? QString::fromUtf8(primitive->getRepr()->attribute("in2") ?: "")
                                             : QString();
}

int FilterPrimitiveList::inputSlot(SPFilterPrimitive* primitive, int input) const {
    if (!primitive) return -1;
    if (input == 0) return primitive->get_in();
    if (auto blend = cast<SPFeBlend>(primitive)) return blend->get_in2();
    if (auto composite = cast<SPFeComposite>(primitive)) return composite->get_in2();
    if (auto displacement = cast<SPFeDisplacementMap>(primitive)) return displacement->get_in2();
    return -1;
}

void FilterPrimitiveList::sanitizeConnections() {
    const auto list = rows();
    auto outputIndex = [&list](int result) {
        for (int index = 0; index < list.size(); ++index) {
            if (list.at(index).primitive->get_out() == result) return index;
        }
        return -1;
    };

    for (int index = 0; index < list.size(); ++index) {
        auto primitive = list.at(index).primitive;
        if (primitive->get_in() >= 0 && outputIndex(primitive->get_in()) >= index) {
            primitive->removeAttribute("in");
        }
        if (auto blend = cast<SPFeBlend>(primitive)) {
            if (blend->get_in2() >= 0 && outputIndex(blend->get_in2()) >= index) primitive->removeAttribute("in2");
        } else if (auto composite = cast<SPFeComposite>(primitive)) {
            if (composite->get_in2() >= 0 && outputIndex(composite->get_in2()) >= index) primitive->removeAttribute("in2");
        } else if (auto displacement = cast<SPFeDisplacementMap>(primitive)) {
            if (displacement->get_in2() >= 0 && outputIndex(displacement->get_in2()) >= index) primitive->removeAttribute("in2");
        }
    }
}

void FilterPrimitiveList::drawNode(QPainter& painter, const QPointF& point, bool active) const {
    QPolygonF triangle({point + QPointF(0, -FilterGraphGeometry::node_half_height),
                           point + QPointF(0, FilterGraphGeometry::node_half_height),
                           point + QPointF(-FilterGraphGeometry::node_width, 0)});
    painter.setBrush(active ? palette().highlight() : palette().base());
    painter.setPen(palette().text().color());
    painter.drawPolygon(triangle);
}

void FilterPrimitiveList::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());
    const auto list = rows();
    const int sourceX = width() - SourceWidth * visibleInputCount();
    auto fg = palette().text().color();
    auto faint = mix(palette().base().color(), fg, 18);
    painter.setFont(font());

    const int sourceColumnWidth = SourceWidth - 1;
    for (int source = 0; source < visibleInputCount(); ++source) {
        QRect band(sourceX + source * SourceWidth, 0, sourceColumnWidth, height());
        painter.fillRect(band, mix(palette().base().color(), fg, 10 + (source % 2) * 4));
        painter.save();
        painter.setPen(mix(palette().base().color(), fg, 70));
        painter.translate(band.center());
        painter.rotate(-90);
        painter.drawText(
            QRect(-height() / 2, -SourceWidth / 2, height(), SourceWidth), Qt::AlignCenter,
            QObject::tr(FPInputConverter.get_label(static_cast<FilterPrimitiveInput>(source)).c_str()));
        painter.restore();
    }

    for (int index = 0; index < list.size(); ++index) {
        const auto& row = list.at(index);
        QRect rowRect(0, row.y, sourceX, row.height);
        if (row.primitive == _selected) painter.fillRect(rowRect, palette().highlight());
        painter.setPen(fg);
        painter.drawText(QRect(10, row.y, LabelWidth, row.height), Qt::AlignVCenter | Qt::AlignLeft,
                         primitiveLabel(row.primitive));
        painter.setPen(faint);
        const int separatorEnd = sourceX - (index + 1) * FilterGraphGeometry::route_offset;
        painter.drawLine(0, row.y + row.height - 1, separatorEnd, row.y + row.height - 1);

        for (int input = 0; input < row.inputs; ++input) {
            const QPoint node(sourceX - 3 - (index + 1) * FilterGraphGeometry::route_offset,
                              row.y + row.height * (input * 2 + 1) / (row.inputs * 2));
            drawNode(painter, node, _dragPrimitive == row.primitive && _dragInput == input);
            const int slot = inputSlot(row.primitive, input);
            int target = -1;
            for (int prior = 0; prior < index; ++prior)
                if (list.at(prior).primitive->get_out() == slot && slot >= 0) target = prior;
            painter.setPen(fg);
            if (target >= 0) {
                const QPoint out(sourceX - (target + 1) * FilterGraphGeometry::route_offset, list.at(target).y + list.at(target).height);
                drawRoundedConnector(painter, node, out);
            } else if (slot < -1) {
                const int source = -slot - 2;
                if (source >= 0 && source < visibleInputCount()) {
                    const QPoint inputPoint(sourceX + source * SourceWidth + sourceColumnWidth / 2, node.y());
                    drawSourceConnector(painter, node, inputPoint, fg);
                }
            } else if (index == 0) {
                const int source = std::min(input, visibleInputCount() - 1);
                const QPoint inputPoint(sourceX + source * SourceWidth + sourceColumnWidth / 2, node.y());
                drawSourceConnector(painter, node, inputPoint, fg);
            } else {
                const int previous = index - 1;
                const QPoint out(sourceX - (previous + 1) * FilterGraphGeometry::route_offset, list.at(previous).y + list.at(previous).height);
                drawRoundedConnector(painter, node, out);
            }
            if (_dragging && row.primitive == _dragPrimitive && input == _dragInput) {
                painter.drawLine(node, QPoint(_dragPoint.x(), node.y()));
                painter.drawLine(QPoint(_dragPoint.x(), node.y()), _dragPoint);
            }
        }
    }
}

QSize FilterPrimitiveList::sizeHint() const {
    const auto list = rows();
    const int height = list.isEmpty() ? FilterGraphGeometry::single_input_height : list.constLast().y + list.constLast().height;
    return {LabelWidth + SourceWidth * visibleInputCount(), height};
}

QSize FilterPrimitiveList::minimumSizeHint() const {
    return sizeHint();
}

void FilterPrimitiveList::selectPrimitive(SPFilterPrimitive* primitive) {
    if (primitive) {
        bool found = false;
        for (const auto& row : rows())
            if (row.primitive == primitive) {
                found = true;
                break;
            }
        if (!found) return;
    }
    if (_selected == primitive) return;
    _selected = primitive;
    update();
    Q_EMIT primitiveSelected(_selected);
}

void FilterPrimitiveList::mousePressEvent(QMouseEvent* event) {
    const auto list = rows();
    const int index = rowAt(event->position().toPoint());
    if (index < 0) return;
    const auto& row = list.at(index);

    if (event->button() == Qt::RightButton && is<SPFeMerge>(row.primitive)) {
        const int sourceX = width() - SourceWidth * visibleInputCount();
        const int nodeX = sourceX - 3 - (index + 1) * FilterGraphGeometry::route_offset;
        const auto point = event->position().toPoint();
        for (int input = 0; input < row.inputs - 1; ++input) {
            const int nodeY = row.y + row.height * (input * 2 + 1) / (row.inputs * 2);
            if (point.x() < nodeX - FilterGraphGeometry::node_width - 4 ||
                point.x() > nodeX + 4 || point.y() < nodeY - FilterGraphGeometry::node_half_height - 4 ||
                point.y() > nodeY + FilterGraphGeometry::node_half_height + 4) {
                continue;
            }
            int childIndex = 0;
            for (auto& child : row.primitive->children) {
                if (!is<SPFeMergeNode>(&child)) continue;
                if (childIndex++ != input) continue;
                sp_repr_unparent(child.getRepr());
                row.primitive->requestModified(SP_OBJECT_MODIFIED_FLAG);
                DocumentUndo::done(row.primitive->document, RC_("Undo", "Remove merge node"),
                                   INKSCAPE_ICON("dialog-filters"));
                Q_EMIT primitiveChanged(row.primitive);
                update();
                return;
            }
        }
        Q_EMIT primitiveContextMenu(row.primitive, mapToGlobal(event->position().toPoint()));
        return;
    }
    if (event->button() == Qt::RightButton) {
        Q_EMIT primitiveContextMenu(row.primitive, mapToGlobal(event->position().toPoint()));
        return;
    }

    if (event->button() != Qt::LeftButton) return;
    setFocus();
    if (_selected != row.primitive) {
        _selected = row.primitive;
        Q_EMIT primitiveSelected(_selected);
    }
    const int input = inputAt(row, event->position().toPoint());
    if (input >= 0) {
        _dragPrimitive = row.primitive;
        _dragInput = input;
        _dragging = true;
        _dragPoint = event->position().toPoint();
    } else {
        _reorderPrimitive = row.primitive;
        _reordering = true;
    }
    update();
}

void FilterPrimitiveList::mouseMoveEvent(QMouseEvent* event) {
    if (!_dragging && !_reordering) return;
    _dragPoint = event->position().toPoint();
    if (_dragging) {
        if (auto scroll = qobject_cast<QScrollArea*>(parentWidget()->parentWidget())) {
            auto bar = scroll->verticalScrollBar();
            if (_dragPoint.y() < 20) bar->setValue(bar->value() - 10);
            else if (_dragPoint.y() > height() - 20) bar->setValue(bar->value() + 10);
            auto horizontal = scroll->horizontalScrollBar();
            if (_dragPoint.x() < 20) horizontal->setValue(horizontal->value() - 10);
            else if (_dragPoint.x() > width() - 20) horizontal->setValue(horizontal->value() + 10);
        }
    }
    update();
}

void FilterPrimitiveList::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    if (_dragging) {
        commitConnection(event->position().toPoint());
        _dragging = false;
        _dragPrimitive = nullptr;
        _dragInput = -1;
    }
    if (_reordering) {
        const auto list = rows();
        const int sourceIndex = std::find_if(list.begin(), list.end(), [this](const auto& row) {
            return row.primitive == _reorderPrimitive;
        }) - list.begin();
        const int targetIndex = rowAt(event->position().toPoint());
        if (sourceIndex >= 0 && sourceIndex < list.size() && targetIndex >= 0 && targetIndex < list.size() &&
            sourceIndex != targetIndex && _filter) {
            _reorderPrimitive->getRepr()->setPosition(targetIndex);
            sanitizeConnections();
            _filter->requestModified(SP_OBJECT_MODIFIED_FLAG);
            DocumentUndo::done(_filter->document, RC_("Undo", "Reorder filter primitive"),
                               INKSCAPE_ICON("dialog-filters"));
            Q_EMIT primitiveChanged(_reorderPrimitive);
        }
        _reordering = false;
        _reorderPrimitive = nullptr;
    }
    update();
}

void FilterPrimitiveList::keyPressEvent(QKeyEvent* event) {
    const auto list = rows();
    if (list.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }

    int index = -1;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).primitive == _selected) {
            index = i;
            break;
        }
    }

    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down ||
        event->key() == Qt::Key_Home || event->key() == Qt::Key_End) {
        if (event->key() == Qt::Key_Up) index = std::max(0, index - 1);
        if (event->key() == Qt::Key_Down) index = std::min(static_cast<int>(list.size()) - 1, index + 1);
        if (event->key() == Qt::Key_Home) index = 0;
        if (event->key() == Qt::Key_End) index = list.size() - 1;
        selectPrimitive(list.at(index).primitive);
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void FilterPrimitiveList::commitConnection(const QPoint& point) {
    if (!_dragPrimitive) return;
    const auto list = rows();
    const int index = rowAt(point);
    if (index < 0) return;
    const auto& target = list.at(index);
    int dragIndex = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list.at(i).primitive == _dragPrimitive) {
            dragIndex = i;
            break;
        }
    if (dragIndex < 0 || index >= dragIndex) return;
    QString value;
    const int source = inputAt(target, point);
    if (source >= 0) {
        value = QString::fromUtf8(FPInputConverter.get_key(static_cast<FilterPrimitiveInput>(source)).c_str());
    } else {
        auto targetRepr = target.primitive->getRepr();
        auto result = targetRepr->attribute("result");
        if (!result) {
            if (!_filter) return;
            value = QString::fromUtf8(_filter->get_new_result_name().c_str());
            targetRepr->setAttribute("result", value.toUtf8().constData());
        } else {
            value = QString::fromUtf8(result);
        }
        if (value.isEmpty()) return;
    }
    auto repr = _dragPrimitive->getRepr();
    if (is<SPFeMerge>(_dragPrimitive)) {
        int n = 0;
        for (auto& child : _dragPrimitive->children) {
            if (!is<SPFeMergeNode>(&child)) continue;
            if (n++ == _dragInput) {
                child.getRepr()->setAttributeOrRemoveIfEmpty("in", value.toUtf8().constData());
                Q_EMIT primitiveChanged(_dragPrimitive);
                return;
            }
        }

        if (n == _dragInput) {
            auto xmlDoc = _dragPrimitive->document->getReprDoc();
            auto mergeNode = xmlDoc->createElement("svg:feMergeNode");
            mergeNode->setAttribute("inkscape:collect", "always");
            mergeNode->setAttribute("in", value.toUtf8().constData());
            _dragPrimitive->getRepr()->appendChild(mergeNode);
            Inkscape::GC::release(mergeNode);
            _dragPrimitive->requestModified(SP_OBJECT_MODIFIED_FLAG);
            DocumentUndo::done(_dragPrimitive->document, RC_("Undo", "Add merge node"),
                               INKSCAPE_ICON("dialog-filters"));
            Q_EMIT primitiveChanged(_dragPrimitive);
            return;
        }
    } else if (_dragInput == 0 || isInput2(_dragPrimitive)) {
        repr->setAttributeOrRemoveIfEmpty(_dragInput == 0 ? "in" : "in2", value.toUtf8().constData());
        Q_EMIT primitiveChanged(_dragPrimitive);
    }
}

} // namespace Linea::UI
