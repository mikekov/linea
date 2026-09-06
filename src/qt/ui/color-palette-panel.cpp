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

#include "color-palette-panel.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "color-palette-options.h"
#include "popup-menu.h"

namespace Linea::UI {

ColorPalettePanel::ColorPalettePanel(QWidget* parent)
    : ResizableEdgeWidget(parent) {
    _palette = new ColorPaletteWidget(this);

    _button = new QPushButton(this);
    _button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    _button->setIcon(QIcon(":/icons/color-palette"));

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_palette, 1);

    _buttonRow = new QHBoxLayout();
    _buttonRow->setContentsMargins(0, 0, _palette->verticalScrollBarWidth(), 4);
    _buttonRow->addWidget(_button, 0, Qt::AlignRight);
    layout->addLayout(_buttonRow);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Create the popup with palette options as its content
    _options = new ColorPaletteOptions();
    _popup = new PopupMenu(this);
    _popup->setContent(_options);

    connect(_button, &QPushButton::clicked, this, &ColorPalettePanel::showOptions);
}

void ColorPalettePanel::showOptions() {
    _popup->showAboveWidget(_button);
}

void ColorPalettePanel::updateButtonMargin() {
    int buttonWidth = _button->sizeHint().width();
    int adjustment = (buttonWidth - _palette->tileSize()) / 2;
    int right = _palette->verticalScrollBarWidth() - adjustment;
    _buttonRow->setContentsMargins(0, 0, right, 4);
}

int ColorPalettePanel::snapResizeWidth(int newWidth) const {
    return _palette->snapResizeWidth(newWidth);
}

} // namespace Linea::UI
