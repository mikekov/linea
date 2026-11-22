// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPaletteWidget — Qt widget displaying a color palette using SimpleGrid.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 */

#include "color-palette-widget.h"

#include <QColor>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdint>

#include "colors/utils.h"
#include "qt/util/drawing-utils.h"

namespace Linea::UI {

using Inkscape::UI::Dialog::PaletteFileData;

namespace {

// Note: _grid is a single container for both document swatches and palette colors.
// Below functions and methods (calcTotalCells, cellToGridPos, entryAtGridPos) are
// used to simulate two-grid layout using a single SimpleGrid. This is done to
// facilitate scrolling; a single scroolbar is used for both sets of tiles, but
// the tiles still reflow independently in multi-column layout, maintaining the
// visual separation between the two collections and keeping palette colors next
// to each other.

// Number of rows needed to fit itemCount tiles across the given number of columns (0 when empty).
std::size_t bandRowCount(std::size_t itemCount, int columns) {
    return (itemCount + columns - 1) / columns;
}

// Map local (col, row) coordinates inside a sub-grid band back to a container index.
std::size_t subGridIndex(int col, int row, int columns, std::size_t bandRows,
                         ColorPaletteWidget::Alignment alignment) {
    if (alignment == ColorPaletteWidget::Alignment::Horizontal) {
        return static_cast<std::size_t>(row) * columns + col;
    }
    return static_cast<std::size_t>(col) * bandRows + row;
}

} // namespace

ColorPaletteWidget::ColorPaletteWidget(QWidget* parent)
    : QWidget(parent) {
    _basicGrid = new SimpleGrid(this);
    _basicGrid->setSelectable(false);
    _basicGrid->setCellSize(_tileSize, _tileSize);
    _basicGrid->setCellStretch(false);
    _basicGrid->setGap(_gap, _gap);
    _basicGrid->setShowGap(false);
    _basicGrid->setHasFrame(false);
    _basicGrid->setActiveBackground(false);
    _basicGrid->setProperty("class", "normal-background");
    _basicGrid->setSizeToContents(true);
    _basicGrid->setFlow(SimpleGrid::Flow::Horizontal);
    _basicGrid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Empty strip grids must be hidden: a size-to-contents SimpleGrid with
    // no cells falls back to the scroll area's default sizeHint, leaving a
    // large gap in the layout.
    _basicGrid->setVisible(false);

    _grid = new SimpleGrid(this);
    _grid->setSelectable(false);
    _grid->setCellSize(_tileSize, _tileSize);
    _grid->setCellStretch(false);
    _grid->setGap(_gap, _gap);
    _grid->setShowGap(false);
    _grid->setHasFrame(false);
    _grid->setActiveBackground(false);
    _grid->setProperty("class", "normal-background");
    _grid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);
    _layout->addWidget(_basicGrid);
    _layout->addWidget(_grid, 1);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(_basicGrid, &SimpleGrid::cellPress, this, [this](int index, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons) {
        if (index >= 0 && index < static_cast<int>(_basics.size())) {
            emitColorClicked(_basics[index], modifiers, buttons);
        }
    });

    connect(_grid, &SimpleGrid::cellPress, this, [this](int index, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons) {
        auto entry = entryForCell(index);
        if (auto swatch = std::get_if<const Linea::Swatch*>(&entry)) {
            Q_EMIT cellClicked(**swatch, modifiers, buttons);
        } else if (auto color = std::get_if<const PaletteFileData::ColorItem*>(&entry)) {
            emitColorClicked(**color, modifiers, buttons);
        }
    });

    // Changing column count alters how many empty gap cells sit between swatches and palette.
    connect(_grid, &SimpleGrid::gridSizeChanged, this, [this](Geom::IntPoint colsRows) {
        _grid->setCellCount(calcTotalCells(std::max(1, colsRows.x())));
    });

    _basicGrid->setDrawFunc(
        [this](QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool /*selected*/) {
            if (index >= _basics.size()) return;
            paintColor(painter, rect, _basics[index]);
        });

    _grid->setDrawFunc([this](QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool /*selected*/) {
        auto entry = entryForCell(static_cast<int>(index));
        if (auto swatch = std::get_if<const Linea::Swatch*>(&entry)) {
            paintColor(painter, rect, (*swatch)->color(), true);
        } else if (auto color = std::get_if<const PaletteFileData::ColorItem*>(&entry)) {
            paintColor(painter, rect, **color);
        }
    });

    _basicGrid->setTooltipFunc([this](int index) -> QString {
        if (index < 0 || index >= static_cast<int>(_basics.size())) {
            return {};
        }
        return colorTooltip(_basics[index]);
    });

    _grid->setTooltipFunc([this](int index) -> QString {
        auto entry = entryForCell(index);
        if (auto swatch = std::get_if<const Linea::Swatch*>(&entry)) {
            return QString::fromStdString((*swatch)->displayName());
        }
        if (auto color = std::get_if<const PaletteFileData::ColorItem*>(&entry)) {
            return colorTooltip(**color);
        }
        return {};
    });
}

void ColorPaletteWidget::setMargins(int left, int top, int right, int bottom) {
    _layout->setContentsMargins(left, top, right, bottom);
}

void ColorPaletteWidget::setBasicColors(std::vector<PaletteFileData::ColorItem> basics) {
    _basics = std::move(basics);
    _basicGrid->setCellCount(_basics.size());
    _basicGrid->setVisible(!_basics.empty());
    _basicGrid->invalidate();
}

void ColorPaletteWidget::setSwatchColors(std::vector<Linea::Swatch> swatches) {
    _swatches = std::move(swatches);
    updateGrid();
}

void ColorPaletteWidget::setPaletteData(PaletteFileData data) {
    _palette = std::move(data);
    updateGrid();
}

// Calculate the cell count _grid needs to simulate two stacked grids (swatches above palette).
std::size_t ColorPaletteWidget::calcTotalCells(int columns) const {
    if (columns <= 0) return 0;

    const auto swatchCount = _swatches.size();
    const auto paletteCount = _palette.colors.size();
    if (swatchCount == 0) return paletteCount;
    if (paletteCount == 0) return swatchCount;

    const auto cols = static_cast<std::size_t>(columns);
    const auto swatchRows = bandRowCount(swatchCount, columns);
    if (_alignment == Alignment::Horizontal) {
        // Pad the swatch band to full rows so the palette starts at column 0 of the next row.
        return swatchRows * cols + paletteCount;
    }

    // In vertical (column-major) flow, SimpleGrid indexes cells as col * totalRows + row and
    // skips indices >= cellCount. The count must both cover the highest occupied swatch/palette
    // index and be large enough for SimpleGrid to allocate all swatchRows + paletteRows rows.
    const auto paletteRows = bandRowCount(paletteCount, columns);
    const auto totalRows = swatchRows + paletteRows;
    const auto lastSwatch = ((swatchCount - 1) / swatchRows) * totalRows + ((swatchCount - 1) % swatchRows);
    const auto lastPalette = ((paletteCount - 1) / paletteRows) * totalRows + swatchRows + ((paletteCount - 1) % paletteRows);
    const auto minForRows = (totalRows - 1) * cols + 1;
    return std::max({minForRows, lastSwatch + 1, lastPalette + 1});
}

// Inverse of SimpleGrid's (col, row) -> index mapping.
Geom::IntPoint ColorPaletteWidget::cellToGridPos(int index, int columns, int totalRows) const {
    if (_alignment == Alignment::Horizontal) {
        return {index % columns, index / columns};
    }
    return {index / totalRows, index % totalRows};
}

// Rows [0, swatchRows) belong to the swatch band; rows [swatchRows, swatchRows + paletteRows)
// belong to the palette band. Unfilled slots in either band return monostate (empty cell).
ColorPaletteWidget::CellEntry ColorPaletteWidget::entryAtGridPos(
    Geom::IntPoint pos, int columns, std::size_t swatchRows, std::size_t paletteRows) const {
    if (pos.x() < 0 || pos.x() >= columns || pos.y() < 0) return {};

    const auto row = static_cast<std::size_t>(pos.y());
    if (row < swatchRows) {
        auto idx = subGridIndex(pos.x(), pos.y(), columns, swatchRows, _alignment);
        if (idx < _swatches.size()) {
            return &_swatches[idx];
        }
        return {};
    }

    const auto localRow = row - swatchRows;
    if (localRow < paletteRows) {
        auto idx = subGridIndex(pos.x(), static_cast<int>(localRow), columns, paletteRows, _alignment);
        if (idx < _palette.colors.size()) {
            return &_palette.colors[idx];
        }
    }
    return {};
}

// Translate a raw SimpleGrid cell index into its backing swatch or palette color, if any.
ColorPaletteWidget::CellEntry ColorPaletteWidget::entryForCell(int index) const {
    if (index < 0) return {};

    const int columns = std::max(1, _grid->gridSize().x());
    const auto swatchRows = bandRowCount(_swatches.size(), columns);
    const auto paletteRows = bandRowCount(_palette.colors.size(), columns);
    const auto totalRows = swatchRows + paletteRows;
    if (totalRows == 0) return {};

    auto pos = cellToGridPos(index, columns, static_cast<int>(totalRows));
    return entryAtGridPos(pos, columns, swatchRows, paletteRows);
}

void ColorPaletteWidget::updateGrid() {
    int columns = std::max(1, _grid->gridSize().x());
    _grid->setCellCount(calcTotalCells(columns));
    _grid->invalidate(false);
}

void ColorPaletteWidget::paintColor(QPainter* painter, const Geom::IntRect& rect,
                                    const PaletteFileData::ColorItem& item, bool swatch) const {
    auto rgb = QColor::fromRgba(0xffffffff);
    bool none = false;
    auto c = std::get_if<Inkscape::Colors::Color>(&item);
    if (c) {
        rgb = QColor::fromRgba(static_cast<QRgb>(c->toARGB()));
    } else if (std::holds_alternative<PaletteFileData::None>(item)) {
        none = true;
    } else {
        // nothing to draw
        return;
    }

    // Swatch tiles match by custom property name only: var(--x) lights up
    // exactly the tile it references, never a tile sharing the resolved
    // color.
    const bool is_fill = swatch ? (c && _currentFill.cssVar && *_currentFill.cssVar == c->getName())
                                : none ? _currentFill.none
                                       : _currentFill.solid && _currentFill.solid->rgb() == rgb.rgb();
    const bool is_stroke = swatch ? (c && _currentStroke.cssVar && *_currentStroke.cssVar == c->getName())
                                  : none ? _currentStroke.none
                                         : _currentStroke.solid && _currentStroke.solid->rgb() == rgb.rgb();

    paintColorTile(painter, rect, palette(), ColorTileParams{
        .color = c ? std::optional<Inkscape::Colors::Color>{*c} : std::nullopt,
        .none = none,
        .swatch = swatch,
        .fill = is_fill,
        .stroke = is_stroke,
    });
}

QString ColorPaletteWidget::colorTooltip(const PaletteFileData::ColorItem& item) const {
    if (const auto& color = std::get_if<Inkscape::Colors::Color>(&item)) {
        return Linea::UI::colorTooltip(*color);
    }
    if (std::holds_alternative<PaletteFileData::None>(item)) {
        return QStringLiteral("No color");
    }
    return {};
}

void ColorPaletteWidget::emitColorClicked(const PaletteFileData::ColorItem& item, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons) {
    if (const auto& color = std::get_if<Inkscape::Colors::Color>(&item)) {
        Q_EMIT cellClicked(*color, modifiers, buttons);
    } else if (std::holds_alternative<PaletteFileData::None>(item)) {
        Q_EMIT cellClicked(std::monostate{}, modifiers, buttons);
    }
}

void ColorPaletteWidget::setAlignment(Alignment alignment) {
    if (_alignment == alignment) return;

    _alignment = alignment;
    auto flow = _alignment == Alignment::Horizontal ? SimpleGrid::Flow::Horizontal : SimpleGrid::Flow::Vertical;
    _grid->setFlow(flow);
    updateGrid();
}

void ColorPaletteWidget::setTileSize(int tile) {
    _tileSize = tile;
    _basicGrid->setCellSize(tile, tile);
    _grid->setCellSize(tile, tile);
}

int ColorPaletteWidget::columnPitch() const {
    return _tileSize + _gap;
}

int ColorPaletteWidget::fixedOverhead() const {
    auto paletteMargins = _layout->contentsMargins();
    auto gridMargins = _grid->layout()->contentsMargins();
    return paletteMargins.left() + paletteMargins.right() + gridMargins.left() + gridMargins.right() +
           _grid->verticalScrollBarWidth();
}

int ColorPaletteWidget::widthForColumns(int columns) const {
    return fixedOverhead() + columns * columnPitch();
}

int ColorPaletteWidget::columnsForWidth(int width) const {
    int step = columnPitch();
    return std::max(0, (width - fixedOverhead() + step / 2) / step);
}

int ColorPaletteWidget::snapResizeWidth(int newWidth) const {
    int columns = std::max(1, columnsForWidth(newWidth));
    return widthForColumns(columns);
}

bool ColorPaletteWidget::CurrentColor::update(const mixed_property<PaintProp>& paint) {
    bool isNone = false;
    std::optional<QColor> color;
    std::optional<std::string> var;

    if (paint.is_single()) {
        auto& v = paint.value();
        if (v.mode == Inkscape::UI::Widget::PaintMode::Solid && v.color) {
            color = QColor::fromRgba(static_cast<QRgb>(v.color->toARGB()));
        } else if (v.mode == Inkscape::UI::Widget::PaintMode::None) {
            isNone = true;
        }
        // CssSwatch populates only cssVar — a var() reference is not a
        // color and must never match regular tiles by resolved RGB.
        if (v.mode == Inkscape::UI::Widget::PaintMode::CssSwatch) {
            var = v.css_var_name;
        }
    }

    if (none != isNone || color != solid || var != cssVar) {
        none = isNone;
        solid = color;
        cssVar = var;
        return true;
    }

    return false;
}

void ColorPaletteWidget::updateCurrentColorIndicator(const mixed_property<PaintProp>& fill,
                                                     const mixed_property<PaintProp>& stroke) {
    auto changedFill = _currentFill.update(fill);
    auto changedStroke = _currentStroke.update(stroke);
    if (changedFill || changedStroke) {
        _grid->invalidate(false);
        _basicGrid->invalidate(false);
    }
}

int ColorPaletteWidget::verticalScrollBarWidth() const {
    return _grid->verticalScrollBarWidth();
}

} // namespace Linea::UI
