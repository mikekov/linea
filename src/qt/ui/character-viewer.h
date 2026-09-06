// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * CharacterViewer — widget for browsing and inserting font characters.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 */

#ifndef LINEA_UI_CHARACTER_VIEWER_H
#define LINEA_UI_CHARACTER_VIEWER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QPushButton>

#include "libnrtype/font-instance.h"
#include "popup-menu.h"

namespace Ui {
class CharacterViewer;
}

namespace Linea::UI {

/**
 * Character viewer widget that displays font characters in a grid
 * and allows browsing Unicode ranges and inserting characters.
 */
class CharacterViewer : public QWidget {
    Q_OBJECT

public:
    explicit CharacterViewer(QWidget* parent = nullptr);
    ~CharacterViewer() override;

    void setFont(FontInstance* font, const QString& name);

Q_SIGNALS:
    void insertText(const QString& text);

private:
    void setupUi();
    void refresh();
    void showCharacters(std::uint32_t from, std::uint32_t to, const QString& filter);
    void drawGlyphPreview(QPainter* painter, const QRect& rect);
    void drawGridCell(QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool selected);
    void setCharSize(int size);
    void showCharSizePopup();

    // UI components
    std::unique_ptr<Ui::CharacterViewer> ui;
    PopupMenu* _charSizePopup = nullptr;
    QSlider* _charSizeSlider;

    // State
    FontInstance* _font = nullptr;
    std::vector<FontInstance::CharInfo> _characters;
    int _cellSize = 30;
    int _currentCell = -1;
    static constexpr std::array<int, 7> charSizes = {20, 25, 30, 35, 40, 50, 60};
};

} // namespace Linea::UI

#endif // LINEA_UI_CHARACTER_VIEWER_H
