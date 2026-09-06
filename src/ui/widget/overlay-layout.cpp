// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Overlay layout implementation for positioning panels that overlap a canvas.
 */

#include "overlay-layout.h"
#include "toolbar.h"
#include <algorithm>
#include <ranges>
#include <vector>

namespace Linea::UI {

OverlayLayout::OverlayLayout(QWidget* parent)
    : QLayout(parent)
{ }

OverlayLayout::~OverlayLayout() {
    qDeleteAll(_items);
}

void OverlayLayout::addItem(QLayoutItem* item) {
    _items.append(item);
}

QLayoutItem* OverlayLayout::itemAt(int index) const {
    return _items.value(index);
}

QLayoutItem* OverlayLayout::takeAt(int index) {
    if (index >= 0 && index < _items.size()) {
        return _items.takeAt(index);
    }
    return nullptr;
}

int OverlayLayout::count() const {
    return _items.size();
}

QRect OverlayLayout::calculatePanelGeometry(const PanelInfo& panel, QRect& rect) {
    QRect geom;
    auto margins = panel.mode == Mode::Floating ? panel.margins : PanelMargins{};

    auto panelWidth = [&]() -> int {
        if (panel.widget && panel.widget->width() > 0) {
            return panel.widget->width();
        }
        return panel.size.width();
    };
    auto panelHeight = [&]() -> int {
        if (panel.widget && panel.widget->height() > 0) {
            return panel.widget->height();
        }
        return panel.size.height();
    };

    if (panel.scaleHeight) {
        // Calculate height based on margins, but respect panel's current size
        int availableHeight = rect.height() - margins.top - margins.bottom;
        int y = rect.top() + margins.top;
        int h = availableHeight;
        if (panel.widget) {
            h = qMin(h, panel.widget->maximumHeight());
        }

        if (panel.position == Position::Left) {
            int x = rect.left() + margins.left;
            int w = panelWidth();
            geom = QRect(x, y, w, h);
            rect.adjust(margins.left + w, 0, 0, 0);
        } else if (panel.position == Position::Right) {
            int w = panelWidth();
            int x = rect.right() - margins.right - w + 1;
            geom = QRect(x, y, w, h);
            rect.adjust(0, 0, -(w + margins.right), 0);
        } else if (panel.position == Position::Center) {
            int w = panelWidth();
            int availableWidth = rect.width() - margins.left - margins.right;
            if (panel.widget) {
                auto p = panel.widget;
                w = std::clamp(availableWidth, p->minimumWidth(), p->maximumWidth());
            }
            int x = rect.left() + margins.left + (availableWidth - w) / 2;
            geom = QRect(x, y, w, h);
            // center docked panel doesn't adjust rect; it would collapse rect to nothing
        } else {
            geom = QRect(QPoint(0, 0), panel.size);
        }
    }
    else {
        // fixed (given) height panel
        geom = QRect(QPoint(0, 0), QSize(panelWidth(), panelHeight()));

        if (panel.position == Position::Right) {
            geom.moveRight(rect.right() - margins.right);
            geom.moveTop(rect.top() + margins.top);
            rect.adjust(0, 0, -(geom.width() + margins.right), 0);
        }
        else if (panel.position == Position::Left) {
            geom.moveLeft(rect.left() + margins.left);
            geom.moveTop(rect.top() + margins.top);
            rect.adjust(margins.left + geom.width(), 0, 0, 0);
        }
        else if (panel.position == Position::Center) {
            int y = rect.top() + margins.top;
            int w = panel.widget ? std::max(panel.widget->sizeHint().width(), panel.size.width()) : panel.size.width();
            w = std::clamp(w, 0, rect.width());
            int h = panel.size.height();
            if (panel.widget) {
                // cannot shrink panel below its min size
                if (w < panel.widget->minimumWidth()) {
                    w = panel.widget->minimumWidth();
                }

                // toolbar has some special sizing logic exposed thanks to its internal flow layout
                if (auto toolbar = qobject_cast<Toolbar*>(panel.widget)) {
                    QSize sz = toolbar->sizeForWidth(w);
                    h = std::max(1, sz.height());
                    if (sz.width() > 0) {
                        w = sz.width();
                    }
                } else {
                    int hfw = panel.widget->heightForWidth(w);
                    h = hfw >= 1 ? hfw : panel.widget->sizeHint().height();
                }
            }
            int x = rect.left() + (rect.width() - w) / 2;
            geom = QRect(x, y, w, h);
            // do not shrink the rect for central panel, which always flows, is not docked
            // rect.adjust(0, geom.height(), 0, 0);
        }
    }

    return geom;
}

void OverlayLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    auto docked = rect;
    auto area = rect;
    auto center = rect;
// printf("set geo: %d %d %d %d (panels: %ld)\n", rect.x(), rect.y(), rect.width(), rect.height(), static_cast<long>(_panels.size()));
    for (auto mode : {Mode::Docked, Mode::Floating}) {
        area = rect;
        for (auto& panel : _panels) {
            if (!panel.widget || panel.mode != mode) continue;

            if (!panel.widget->isVisible() && panel.mode == Mode::Docked) continue;

            QRect geom = calculatePanelGeometry(panel, area);
            panel.widget->setGeometry(geom);

            if (panel.widget->isVisible()) {
                panel.widget->raise();
            }

            if (panel.mode == Mode::Docked) {
                switch (panel.position) {
                    case Position::Left:
                        docked.setLeft(area.left());
                        break;
                    case Position::Right:
                        docked.setRight(area.right());
                        break;
                    case Position::Center:
                        break;
                }
            }
        }
        if (mode == Mode::Docked) {
            center = area;
        }
    }

    for (auto& panel : _panels) {
        if (panel.widget && panel.mode == Mode::Docked && panel.position == Position::Center && panel.widget->isVisible()) {
            QRect geom = calculatePanelGeometry(panel, center);
            panel.widget->setGeometry(geom);
        }
    }

    // first item - canvas
    if (!_items.isEmpty()) {
        auto canvas = _items.front()->widget();
        canvas->setGeometry(docked);
    }
}

QSize OverlayLayout::sizeHint() const {
    return QSize(800, 600);
}

QSize OverlayLayout::minimumSize() const {
    return QSize(400, 300);
}

void OverlayLayout::setPanelPosition(QWidget* panel, Position pos, const QSize& size, bool scaleHeight, const PanelMargins& margins) {
    panel->resize(size); // establish initial size as requested
    PanelInfo info{panel, pos, size, margins, scaleHeight};
    _panels.append(info);
}

void OverlayLayout::setPanelMargins(QWidget* panel, const PanelMargins& margins, bool scaleHeight) {
    for (auto& panelInfo : _panels) {
        if (panelInfo.widget == panel) {
            panelInfo.margins = margins;
            panelInfo.scaleHeight = scaleHeight;
            break;
        }
    }
}

QRect OverlayLayout::targetGeometry(QWidget* panel) const {
    for (auto& p : _panels) {
        if (p.widget == panel) {
            QRect rect = geometry();
            if (p.widget && p.widget->parentWidget()) {
                QRect parentRect = p.widget->parentWidget()->rect();
                // Use parent size if layout hasn't been set up or is smaller
                if (parentRect.width() > rect.width() || parentRect.height() > rect.height()) {
                    rect = parentRect;
                }
            }
            return calculatePanelGeometry(p, rect);
        }
    }
    return QRect();
}

void OverlayLayout::updatePanelGeometry() {
    // The layout now reads each panel's current size when it lays out, so no
    // explicit size needs to be stored. Just re-run the layout.
    setGeometry(geometry());
}

OverlayLayout::Mode OverlayLayout::panelMode(QWidget* panel) const {
    for (auto& p : _panels) {
        if (p.widget == panel) {
            return p.mode;
        }
    }
    return Mode::Docked;
}

void OverlayLayout::setPanelMode(QWidget* panel, Mode mode) {
    for (auto& p : _panels) {
        if (p.widget == panel) {
            if (p.mode != mode) {
                p.mode = mode;
                invalidate();
            }
            break;
        }
    }
}

} // namespace Linea::UI
