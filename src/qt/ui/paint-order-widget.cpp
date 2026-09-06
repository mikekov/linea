// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PaintOrderWidget — reorderable stack for paint order.
 */

#include "paint-order-widget.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

#include "style-enums.h"
#include "style-internal.h"

namespace Linea::UI {

// A single row: [icon] [label] [drag handle]
class PaintOrderWidget::RowWidget : public QWidget {
public:
    RowWidget(const QString& label, const QString& iconName, const QString& tooltip,
              PaintOrderWidget* parent)
        : QWidget(static_cast<QWidget*>(parent)), _parent(parent)
    {
        setToolTip(tooltip);
        setCursor(Qt::OpenHandCursor);

        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(6);

        _iconLabel = new QLabel(this);
        _iconLabel->setPixmap(QIcon(":/icons/" + iconName).pixmap(16, 16));
        _iconLabel->setFixedSize(20, 20);
        _iconLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(_iconLabel);

        _textLabel = new QLabel(label, this);
        _textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(_textLabel);

        _handleLabel = new QLabel(this);
        _handleLabel->setPixmap(QIcon(":/icons/drag-handle").pixmap(16, 16));
        _handleLabel->setFixedSize(20, 20);
        _handleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(_handleLabel);
    }

protected:
    void enterEvent(QEnterEvent* event) override {
        // setCursor() doesn't work inside a Qt::Popup window; use override cursor
        QApplication::setOverrideCursor(cursor());
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        if (!_dragging) {
            QApplication::restoreOverrideCursor();
        }
        QWidget::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            _dragStartPos = event->globalPosition().toPoint();
            _dragging = false;
            setCursor(Qt::ClosedHandCursor);
            QApplication::changeOverrideCursor(cursor());
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!(event->buttons() & Qt::LeftButton)) return;

        if (!_dragging) {
            if ((event->globalPosition().toPoint() - _dragStartPos).manhattanLength()
                < QApplication::startDragDistance()) {
                return;
            }
            _dragging = true;
            _parent->_draggedRow = this;
            _parent->_dragStartIndex = _parent->rowIndexOf(this);
        }

        // Determine target index based on cursor Y position
        int targetIndex = _parent->indexAtY(event->globalPosition().toPoint().y());
        int currentIndex = _parent->rowIndexOf(this);
        if (targetIndex >= 0 && targetIndex != currentIndex) {
            _parent->moveRow(currentIndex, targetIndex);
        }

        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            setCursor(Qt::OpenHandCursor);
            QApplication::changeOverrideCursor(cursor());
            if (_dragging) {
                _dragging = false;
                _parent->_draggedRow = nullptr;
                int newIndex = _parent->rowIndexOf(this);
                if (newIndex != _parent->_dragStartIndex) {
                    Q_EMIT _parent->orderChanged();
                }
                _parent->_dragStartIndex = -1;
            }
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    PaintOrderWidget* _parent;
    QLabel* _iconLabel = nullptr;
    QLabel* _textLabel = nullptr;
    QLabel* _handleLabel = nullptr;
    QPoint _dragStartPos;
    bool _dragging = false;
};

// --- PaintOrderWidget ---

PaintOrderWidget::PaintOrderWidget(QWidget* parent)
    : QWidget(parent)
{
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);
}

void PaintOrderWidget::addOption(const QString& label, const QString& icon,
                                  const QString& tooltip, int value) {
    auto row = new RowWidget(label, icon, tooltip, this);
    _layout->addWidget(row);
    _rows.push_back({row, value});
}

void PaintOrderWidget::setValues(const std::vector<int>& values) {
    // Reorder _rows to match the given value order
    std::vector<Row> newOrder;
    newOrder.reserve(values.size());
    for (int val : values) {
        for (auto& row : _rows) {
            if (row.value == val) {
                newOrder.push_back(row);
                break;
            }
        }
    }
    _rows = std::move(newOrder);

    // Update layout order
    for (auto& row : _rows) {
        _layout->removeWidget(row.widget);
    }
    for (auto& row : _rows) {
        _layout->addWidget(row.widget);
    }
}

std::vector<int> PaintOrderWidget::getValues() const {
    std::vector<int> values;
    values.reserve(_rows.size());
    for (auto& row : _rows) {
        if (row.widget->isVisible()) {
            values.push_back(row.value);
        }
    }
    // Include hidden rows at the end to preserve full ordering
    for (auto& row : _rows) {
        if (!row.widget->isVisible()) {
            values.push_back(row.value);
        }
    }
    return values;
}

void PaintOrderWidget::setRowVisible(int value, bool visible) {
    for (auto& row : _rows) {
        if (row.value == value) {
            row.widget->setVisible(visible);
            break;
        }
    }
}

void PaintOrderWidget::setValue(const SPIPaintOrder& po, bool hasMarkers) {
    auto layers = po.get_layers();
    // What's painted first is at the bottom of the visual stack
    std::vector<int> vec = {layers[2], layers[1], layers[0]};
    setValues(vec);
    setRowVisible(SP_CSS_PAINT_ORDER_MARKER, hasMarkers);
}

SPIPaintOrder PaintOrderWidget::getValue() const {
    SPIPaintOrder po;
    auto values = getValues();
    for (int i = 0; i < 3 && i < static_cast<int>(values.size()); ++i) {
        // Reversed order to match setValue()
        po.layer[i] = static_cast<SPPaintOrderLayer>(values[2 - i]);
        po.layer_set[i] = true;
    }
    po.set = true;
    return po;
}

void PaintOrderWidget::moveRow(int fromIndex, int toIndex) {
    if (fromIndex == toIndex) return;
    if (fromIndex < 0 || fromIndex >= static_cast<int>(_rows.size())) return;
    if (toIndex < 0 || toIndex >= static_cast<int>(_rows.size())) return;

    auto row = _rows[fromIndex];
    _rows.erase(_rows.begin() + fromIndex);
    _rows.insert(_rows.begin() + toIndex, row);

    // Update layout
    _layout->removeWidget(row.widget);
    _layout->insertWidget(toIndex, row.widget);
}

int PaintOrderWidget::indexAtY(int globalY) const {
    for (int i = 0; i < static_cast<int>(_rows.size()); ++i) {
        auto* w = _rows[i].widget;
        if (!w->isVisible()) continue;

        auto topLeft = w->mapToGlobal(QPoint(0, 0));
        int midY = topLeft.y() + w->height() / 2;
        if (globalY < midY) return i;
    }
    return static_cast<int>(_rows.size()) - 1;
}

int PaintOrderWidget::rowIndexOf(QWidget* widget) const {
    for (int i = 0; i < static_cast<int>(_rows.size()); ++i) {
        if (_rows[i].widget == widget) return i;
    }
    return -1;
}

} // namespace Linea::UI
