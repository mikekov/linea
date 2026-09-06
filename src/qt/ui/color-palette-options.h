// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPaletteOptions — widget for selecting a color palette and tile size.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 */

#ifndef LINEA_UI_COLOR_PALETTE_OPTIONS_H
#define LINEA_UI_COLOR_PALETTE_OPTIONS_H

#include <QWidget>
#include <memory>

class QListWidget;

namespace Ui {
class ColorPaletteOptions;
}

namespace Inkscape::UI::Dialog {
struct PaletteFileData;
}

namespace Linea::UI {

class RadioToggle;

/// Item delegate that draws a palette's name and a color preview strip.
class PaletteListDelegate;

/**
 * Widget for choosing a color palette from the GlobalPalettes list
 * and selecting a tile size. Defined in color-palette-options.ui.
 */
class ColorPaletteOptions : public QWidget {
    Q_OBJECT

public:
    explicit ColorPaletteOptions(QWidget* parent = nullptr);
    ~ColorPaletteOptions() override;

    /// Tile size choices.
    enum class TileSize { Small, Medium, Large };

    /// Populate the list with palettes from GlobalPalettes.
    void populatePalettes();

    /// Currently selected palette index, or -1 if none.
    int selectedPaletteIndex() const;
    void setSelectedPaletteIndex(int index);

    /// Currently selected tile size.
    TileSize tileSize() const;
    void setTileSize(TileSize size);

Q_SIGNALS:
    /// Emitted when the selected palette changes.
    void paletteChanged(int index);
    /// Emitted when the tile size changes.
    void tileSizeChanged(TileSize size);

private:
    std::unique_ptr<Ui::ColorPaletteOptions> _ui;
    PaletteListDelegate* _delegate = nullptr;
    RadioToggle* _sizeGroup = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PALETTE_OPTIONS_H
