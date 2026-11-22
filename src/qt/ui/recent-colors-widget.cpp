// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RecentColorsWidget — Qt widget displaying a strip of recently used colors using SimpleGrid.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 */

#include "recent-colors-widget.h"

#include <QPainter>
#include <QVBoxLayout>
#include <cstdint>

#include "qt/ui/simple-grid.h"
#include "qt/util/drawing-utils.h"

namespace Linea::UI {

RecentColorsWidget::RecentColorsWidget(QWidget* parent)
    : QWidget(parent) {
    _grid = new SimpleGrid(this);
    _grid->setSelectable(false);
    _grid->setCellSize(_tileSize, _tileSize);
    _grid->setGap(_gap, _gap);
    _grid->setCellStretch(false);
    _grid->setShowGap(false);
    _grid->setHasFrame(false);
    _grid->setActiveBackground(false);
    _grid->setProperty("class", "normal-background");
    _grid->setSingleLine(true);
    _grid->setScrollBarSuppressed(true);
    _grid->setFlow(SimpleGrid::Flow::Horizontal);
    _grid->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_grid);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(_grid, &SimpleGrid::cellPress, this,
            [this](int index, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons) {
                if (auto entry = entryAt(index)) {
                    Q_EMIT cellClicked(*entry, modifiers, buttons);
                }
            });

    _grid->setDrawFunc([this](QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool /*selected*/) {
        auto entry = entryAt(static_cast<int>(index));
        if (!entry) return;

        ColorTileParams params;
        if (auto color = std::get_if<Inkscape::Colors::Color>(entry)) {
            params.color = *color;
        } else if (auto swatch = std::get_if<Linea::Swatch>(entry)) {
            params.color = swatch->color();
            params.swatch = true;
        } else {
            params.none = true;
        }
        paintColorTile(painter, rect, palette(), params);
    });

    _grid->setTooltipFunc([this](int index) -> QString {
        auto entry = entryAt(index);
        if (!entry) {
            return {};
        }
        if (auto color = std::get_if<Inkscape::Colors::Color>(entry)) {
            return Linea::UI::colorTooltip(*color);
        }
        if (auto swatch = std::get_if<Linea::Swatch>(entry)) {
            return QString::fromStdString(swatch->displayName());
        }
        return QStringLiteral("No color");
    });
}

const FlatColor* RecentColorsWidget::entryAt(int index) const {
    if (index < 0 || index >= static_cast<int>(_entries.size())) return nullptr;
    return &_entries[index];
}

void RecentColorsWidget::setEntries(std::vector<FlatColor> entries) {
    _entries = std::move(entries);
    _grid->setCellCount(_entries.size());
    _grid->invalidate();
    updateGeometry();
}

void RecentColorsWidget::setTileSize(int tile) {
    if (tile <= 0 || tile == _tileSize) return;

    _tileSize = tile;
    _grid->setCellSize(tile, tile);
    updateGeometry();
}

void RecentColorsWidget::setAlignment(Alignment alignment) {
    if (_alignment == alignment) return;

    _alignment = alignment;
    _grid->setFlow(_alignment == Alignment::Horizontal ? SimpleGrid::Flow::Horizontal : SimpleGrid::Flow::Vertical);
    updateGeometry();
}

QSize RecentColorsWidget::sizeHint() const {
    auto count = static_cast<int>(_entries.size());
    if (count == 0) return {};

    // gaps sit between cells — the trailing tile has none
    const int pitch = _tileSize + _gap;
    return _alignment == Alignment::Horizontal ? QSize(count * pitch - _gap, _tileSize)
                                               : QSize(_tileSize, count * pitch - _gap);
}

QSize RecentColorsWidget::minimumSizeHint() const {
    return sizeHint();
}

} // namespace Linea::UI
