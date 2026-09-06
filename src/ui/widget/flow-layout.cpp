// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Generic flow layout implementation.
 */

#include "flow-layout.h"

#include <QApplication>
#include <QLayoutItem>
#include <QSpacerItem>
#include <QStyle>
#include <QWidget>
#include <QWidgetItem>
#include <algorithm>

namespace {

bool isEdgeSpacer(QLayoutItem* item) {
    if (item->spacerItem() == nullptr) return false;
    return (item->expandingDirections() & Qt::Horizontal) == 0;
}

}
FlowLayout::FlowLayout(QWidget* parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent)
    , _hSpace(hSpacing)
    , _vSpace(vSpacing) {
    if (margin >= 0) {
        setContentsMargins(margin, margin, margin, margin);
    }
}

FlowLayout::FlowLayout(int hSpacing, int vSpacing)
    : QLayout()
    , _hSpace(hSpacing)
    , _vSpace(vSpacing) {}

FlowLayout::~FlowLayout() {
    while (QLayoutItem* item = takeAt(0)) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem* item) {
    insertItem(static_cast<int>(_items.size()), item);
}

void FlowLayout::insertItem(int index, QLayoutItem* item) {
    int size = static_cast<int>(_items.size());
    _items.insert(std::max(0, std::min(index, size)), item);
}

void FlowLayout::insertWidget(int index, QWidget* widget) {
    addChildWidget(widget);
    insertItem(index, new QWidgetItem(widget));
}

void FlowLayout::addSpacing(int size) {
    insertSpacing(_items.size(), size);
}

void FlowLayout::insertSpacing(int index, int size) {
    insertItem(index, new QSpacerItem(size, 1, QSizePolicy::Fixed, QSizePolicy::Fixed));
}

void FlowLayout::addStretch(int stretch) {
    insertStretch(_items.size(), stretch);
}

void FlowLayout::insertStretch(int index, int /*stretch*/) {
    insertItem(index, new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
}

Qt::Orientations FlowLayout::expandingDirections() const {
    return Qt::Horizontal;
}

bool FlowLayout::hasHeightForWidth() const {
    return true;
}

int FlowLayout::heightForWidth(int width) const {
    return doLayout(QRect(0, 0, width, 0), true).height();
}

QSize FlowLayout::sizeForWidth(int width) const {
    return doLayout(QRect(0, 0, width, 0), true);
}

int FlowLayout::count() const {
    return _items.size();
}

QLayoutItem* FlowLayout::itemAt(int index) const {
    return (index >= 0 && index < _items.size()) ? _items[index] : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index) {
    if (index < 0 || index >= _items.size()) {
        return nullptr;
    }
    return _items.takeAt(index);
}

QSize FlowLayout::minimumSize() const {
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);

    int minW = 1;
    for (const QLayoutItem* item : _items) {
        minW = std::max(minW, item->sizeHint().width());
    }

    int widthWithMargins = minW + left + right;
    return {widthWithMargins, doLayout(QRect(0, 0, widthWithMargins, 0), true).height()};
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const {
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);

    int start = 0;
    int end = static_cast<int>(_items.size()) - 1;
    while (start < _items.size() && isEdgeSpacer(_items[start])) {
        ++start;
    }
    while (end >= start && isEdgeSpacer(_items[end])) {
        --end;
    }
    if (start > end) {
        return {left + right, top + bottom};
    }

    int width = 0;
    int height = 0;
    int hSpace = horizontalSpacing();

    for (int i = start; i <= end; ++i) {
        QSize sz = _items[i]->sizeHint();
        width += sz.width();
        height = std::max(height, sz.height());
        if (i != end) {
            width += hSpace;
        }
    }

    return {width + left + right, height + top + bottom};
}

int FlowLayout::horizontalSpacing() const {
    if (_hSpace >= 0) {
        return _hSpace;
    }
    return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const {
    if (_vSpace >= 0) {
        return _vSpace;
    }
    return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const {
    if (QObject* p = parent()) {
        if (p->isWidgetType()) {
            if (QWidget* pw = static_cast<QWidget*>(p)) {
                return std::max(0, pw->style()->pixelMetric(pm, nullptr, pw));
            }
        }
    }
    return 0;
}

QSize FlowLayout::doLayout(const QRect& rect, bool testOnly) const {
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);

    QRect effective = rect.adjusted(+left, +top, -right, -bottom);
    effective.setWidth(std::max(1, effective.width()));
    effective.setHeight(std::max(1, effective.height()));

    int xOrigin = effective.x();
    int y = effective.y();
    int spaceX = horizontalSpacing();
    int spaceY = verticalSpacing();
    int maxRowWidth = 0;

    int i = 0;
    while (i < _items.size()) {
        int rowStart = i;
        int fixedItemWidth = 0;
        int stretchCount = 0;
        int rowHeight = 0;

        // Collect items for the current row.
        while (i < _items.size()) {
            QLayoutItem* item = _items[i];
            bool stretch = (item->expandingDirections() & Qt::Horizontal) != 0;
            int itemW = stretch ? 1 : item->sizeHint().width();
            int projected = fixedItemWidth + itemW + ((i == rowStart) ? 0 : spaceX);

            if (projected > effective.width() && i > rowStart) {
                break;
            }

            fixedItemWidth += itemW;
            if (stretch) {
                ++stretchCount;
            }
            rowHeight = std::max(rowHeight, item->sizeHint().height());
            ++i;
        }

        int rowEnd = i;
        if (rowEnd == rowStart) {
            // Defensive: a single oversized item still needs to be laid out.
            rowHeight = std::max(rowHeight, _items[rowStart]->sizeHint().height());
            rowEnd = ++i;
        }

        // Suppress fixed spacers that would sit at the left/right edge of a wrapped row.
        int first = rowStart;
        int last = rowEnd - 1;
        while (first < rowEnd && isEdgeSpacer(_items[first])) {
            ++first;
        }
        while (last >= rowStart && isEdgeSpacer(_items[last])) {
            --last;
        }
        if (first > last) {
            continue;
        }

        int placeStart = first;
        int placeEnd = last + 1;

        fixedItemWidth = 0;
        stretchCount = 0;
        rowHeight = 0;
        for (int k = placeStart; k < placeEnd; ++k) {
            QLayoutItem* item = _items[k];
            bool stretch = (item->expandingDirections() & Qt::Horizontal) != 0;
            fixedItemWidth += stretch ? 1 : item->sizeHint().width();
            if (stretch) {
                ++stretchCount;
            }
            rowHeight = std::max(rowHeight, item->sizeHint().height());
        }

        int rowCount = placeEnd - placeStart;
        int spacingTotal = (rowCount - 1) * spaceX;
        int fixedTotal = fixedItemWidth + spacingTotal;
        maxRowWidth = std::max(maxRowWidth, fixedTotal);

        int extraPerStretch = 0;
        int extraRemainder = 0;
        if (stretchCount > 0) {
            int leftover = effective.width() - fixedTotal;
            if (leftover > 0) {
                extraPerStretch = leftover / stretchCount;
                extraRemainder = leftover - extraPerStretch * stretchCount;
            }
        }

        int actualTotal = fixedTotal + extraPerStretch * stretchCount + extraRemainder;
        int x = xOrigin;
        if (_alignment & Qt::AlignHCenter) {
            x += (effective.width() - actualTotal) / 2;
        } else if (_alignment & Qt::AlignRight) {
            x += effective.width() - actualTotal;
        }

        for (int k = placeStart; k < placeEnd; ++k) {
            QLayoutItem* item = _items[k];
            bool stretch = (item->expandingDirections() & Qt::Horizontal) != 0;
            QSize hint = item->sizeHint();
            int w = hint.width();
            int h = hint.height();

            if (stretch) {
                w = extraPerStretch + hint.width();
                if (extraRemainder > 0) {
                    w += 1;
                    --extraRemainder;
                }
            }

            int yOff = 0;
            if (_alignment & Qt::AlignVCenter) {
                yOff = (rowHeight - h) / 2;
            } else if (_alignment & Qt::AlignBottom) {
                yOff = rowHeight - h;
            }

            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y + yOff), QSize(w, h)));
            }

            x += w + spaceX;
        }

        y += rowHeight;
        if (i < _items.size()) {
            y += spaceY;
        }
    }
    return {maxRowWidth + left + right, y - rect.y() + bottom};
}
