// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPalettePanel — resizable panel containing a ColorPaletteWidget and a button.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 */

#ifndef LINEA_UI_COLOR_PALETTE_PANEL_H
#define LINEA_UI_COLOR_PALETTE_PANEL_H

#include "qt/ui/color-palette-widget.h"
#include "ui/widget/resizable-edge-widget.h"

class QPushButton;
class QHBoxLayout;

namespace Linea::UI {

class PopupMenu;
class ColorPaletteOptions;

/**
 * A resizable panel that wraps a ColorPaletteWidget with a push button below it.
 *
 * The panel itself is a ResizableEdgeWidget so the resize handle and column
 * snapping work on the combined unit. The contained ColorPaletteWidget is a
 * passive child whose width follows the panel.
 */
class ColorPalettePanel : public ResizableEdgeWidget {
    Q_OBJECT

public:
    explicit ColorPalettePanel(QWidget* parent = nullptr);

    /// The contained palette widget.
    ColorPaletteWidget* palette() const { return _palette; }

    /// The push button below the palette.
    QPushButton* button() const { return _button; }

    /// The palette options widget shown in the popup.
    ColorPaletteOptions* options() const { return _options; }

    /// Update the button row right margin for the current tile size.
    void updateButtonMargin();

protected:
    int snapResizeWidth(int newWidth) const override;

private:
    void showOptions();

    ColorPaletteWidget* _palette = nullptr;
    QPushButton* _button = nullptr;
    QHBoxLayout* _buttonRow = nullptr;
    PopupMenu* _popup = nullptr;
    ColorPaletteOptions* _options = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PALETTE_PANEL_H
