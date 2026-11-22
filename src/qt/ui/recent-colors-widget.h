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

#ifndef LINEA_UI_RECENT_COLORS_WIDGET_H
#define LINEA_UI_RECENT_COLORS_WIDGET_H

#include <QWidget>
#include <vector>

#include "flat-color.h"

namespace Linea::UI {

class SimpleGrid;

/**
 * A widget that displays recently used colors as a single row or column of tiles.
 *
 * It contains a SimpleGrid as its only child, laid out in a single line with no
 * scroll bar. The widget has no selection and emits cellClicked(color, modifiers,
 * buttons) when a tile is pressed. Orientation of the strip can be adjusted with
 * setAlignment().
 */
class RecentColorsWidget : public QWidget {
    Q_OBJECT

public:
    explicit RecentColorsWidget(QWidget* parent = nullptr);

    /// Set horizontal/vertical orientation of the tile strip.
    enum class Alignment { Horizontal, Vertical };
    void setAlignment(Alignment alignment);
    Alignment alignment() const { return _alignment; }

    /// Set the list of colors to display.
    void setEntries(std::vector<FlatColor> entries);
    const std::vector<FlatColor>& entries() const { return _entries; }

    /// Set the size (width and height) of a single tile.
    void setTileSize(int tile);
    /// Current tile size in pixels.
    int tileSize() const { return _tileSize; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// Emitted when a tile is clicked.
    void cellClicked(FlatColor color, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons);

private:
    const FlatColor* entryAt(int index) const;

    SimpleGrid* _grid = nullptr;
    Alignment _alignment = Alignment::Horizontal;
    std::vector<FlatColor> _entries;
    int _tileSize = 15;
    int _gap = 1;
};

} // namespace Linea::UI

#endif // LINEA_UI_RECENT_COLORS_WIDGET_H
