// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * AlignmentSelector — 3×3 grid of alignment buttons (Qt version).
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "alignment-selector.h"

#include <QGridLayout>
#include <QIcon>
#include <QToolButton>

namespace Linea::UI {

namespace {

struct ButtonInfo {
    const char* icon;
};

// Icon names matching the GTK version's boundingbox icons
const ButtonInfo button_info[9] = {
    {"boundingbox_top_left"},
    {"boundingbox_top"},
    {"boundingbox_top_right"},
    {"boundingbox_left"},
    {"boundingbox_center"},
    {"boundingbox_right"},
    {"boundingbox_bottom_left"},
    {"boundingbox_bottom"},
    {"boundingbox_bottom_right"},
};

} // namespace

AlignmentSelector::AlignmentSelector(QWidget* parent)
    : QWidget(parent)
{
    _grid = new QGridLayout(this);
    _grid->setContentsMargins(2, 2, 2, 2);
    _grid->setSpacing(0);

    for (int i = 0; i < 9; ++i) {
        auto button = new QToolButton(this);
        setupButton(button, button_info[i].icon);

        int row = i / 3;
        int col = i % 3;
        _grid->addWidget(button, row, col);
        _buttons[i] = button;

        connect(button, &QToolButton::clicked, this, [this, i]() {
            Q_EMIT alignmentClicked(i);
        });
    }

    setLayout(_grid);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void AlignmentSelector::setupButton(QToolButton* button, const QString& iconName) {
    button->setIcon(QIcon(":/icons/" + iconName));
    button->setIconSize(QSize(16, 16));
    button->setFixedSize(24, 24);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
}

} // namespace Linea::UI
