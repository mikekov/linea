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

#ifndef LINEA_UI_ALIGNMENT_SELECTOR_H
#define LINEA_UI_ALIGNMENT_SELECTOR_H

#include <array>
#include <QWidget>

class QToolButton;
class QGridLayout;

namespace Linea::UI {

/**
 * A 3×3 grid of buttons for selecting an alignment / anchor point.
 *
 * Button indices map to positions:
 *   0=TL  1=T   2=TR
 *   3=L   4=C   5=R
 *   6=BL  7=B   8=BR
 *
 * Emits alignmentClicked(int index) when a button is pressed.
 */
class AlignmentSelector : public QWidget {
    Q_OBJECT

public:
    explicit AlignmentSelector(QWidget* parent = nullptr);
    ~AlignmentSelector() override = default;

Q_SIGNALS:
    void alignmentClicked(int index);

private:
    void setupButton(QToolButton* button, const QString& iconName);

    QGridLayout* _grid = nullptr;
    std::array<QToolButton*, 9> _buttons{};
};

} // namespace Linea::UI

#endif // LINEA_UI_ALIGNMENT_SELECTOR_H
