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

#ifndef LINEA_UI_COLOR_PALETTE_WIDGET_H
#define LINEA_UI_COLOR_PALETTE_WIDGET_H

#include <optional>
#include <QWidget>
#include "colors/color.h"
#include "qt/ui/simple-grid.h"
#include "ui/dialog/global-palettes.h"
#include "util/style-utils.h"

class QPainter;
class QVBoxLayout;

namespace Linea::UI {

/**
 * A widget that displays a color palette loaded from a PaletteFileData.
 *
 * It contains a SimpleGrid as its only child. Colors are rendered as solid
 * cells. The widget has no selection and emits cellClicked(int, modifiers) when a cell
 * is clicked. Alignment of the grid inside the widget can be adjusted with
 * setAlignment().
 */
class ColorPaletteWidget : public QWidget {
    Q_OBJECT

public:
    explicit ColorPaletteWidget(QWidget* parent = nullptr);

    /// Load colors from a palette file.
    void setPaletteData(Inkscape::UI::Dialog::PaletteFileData data);

    /// Set the basic colors shown at the top of the palette.
    void setBasicColors(std::vector<Inkscape::UI::Dialog::PaletteFileData::ColorItem> basics);

    /// Set vertical/horizontal alignment of the grid inside this widget.
    enum class Alignment { Horizontal, Vertical };
    void setAlignment(Alignment alignment);
    Alignment alignment() const { return _alignment; }

    /// Set the size (width and height) of a single palette tile.
    void setTileSize(int tile);
    /// Current tile size in pixels.
    int tileSize() const { return _tileSize; }
    /// Set the margins around the grid.
    void setMargins(int left, int top, int right, int bottom);
    /// Turn on swatch indicator for each color tile; used to draw document swatches
    void setSwatchIndicator(bool swatch);

    /// Total widget width for a given number of columns.
    int widthForColumns(int columns) const;

    /// Number of columns that fit in the given widget width (rounded to nearest).
    int columnsForWidth(int width) const;

    /// Snap a width to the nearest whole-column increment.
    int snapResizeWidth(int newWidth) const;

    /// Update stroke and fill color indicators
    void updateCurrentColorIndicator(const mixed_property<PaintProp>& fill, const mixed_property<PaintProp>& stroke);

    /// Get the width of the vertical scroll bar
    int verticalScrollBarWidth() const;
    
Q_SIGNALS:
    /// Emitted when a palette cell is clicked.
    void cellClicked(std::optional<Inkscape::Colors::Color> color, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons);

private:
    /// Width of one column of cells (tile + gap).
    int columnPitch() const;
    /// Fixed width not available for cells (margins + vertical scroll bar).
    int fixedOverhead() const;

    void paintColor(QPainter* painter, const Geom::IntRect& rect, const Inkscape::UI::Dialog::PaletteFileData::ColorItem& item) const;
    QString colorTooltip(const Inkscape::UI::Dialog::PaletteFileData::ColorItem& item) const;
    void emitColorClicked(const Inkscape::UI::Dialog::PaletteFileData::ColorItem& item, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons);

    SimpleGrid* _grid = nullptr;
    SimpleGrid* _basicGrid = nullptr;
    QVBoxLayout* _layout = nullptr;
    Alignment _alignment = Alignment::Horizontal;
    Inkscape::UI::Dialog::PaletteFileData _palette;
    std::vector<Inkscape::UI::Dialog::PaletteFileData::ColorItem> _basics;
    int _tileSize = 15;
    int _gap = 1;
    bool _swatchIndicator = false;
    struct CurrentColor {
        bool none = false;
        std::optional<QColor> solid;
        bool update(const mixed_property<PaintProp>& paint);
    };
    CurrentColor _currentFill;
    CurrentColor _currentStroke;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PALETTE_WIDGET_H
