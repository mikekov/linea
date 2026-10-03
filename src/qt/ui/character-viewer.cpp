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

#include "character-viewer.h"
#include "drawing-area.h"
#include "ui_character-viewer.h"
#include "icon-combobox.h"
#include "popup-menu.h"

#include <QColor>
#include <QPainter>
#include <QPaintEvent>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QVBoxLayout>

#include <optional>

#include "preferences.h"
#include "libnrtype/font-instance.h"
#include "util/unicode.h"
#include "qt/util/drawing-utils.h"

using namespace Inkscape;

namespace Linea::UI {

CharacterViewer::CharacterViewer(QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::CharacterViewer>())
{
    ui->setupUi(this);
    setupUi();
}

CharacterViewer::~CharacterViewer() = default;

void CharacterViewer::setupUi() {
    // Setup glyph preview drawing callback
    ui->glyphPreview->setDrawCallback([this](QPainter* painter, const QRect& rect) {
        drawGlyphPreview(painter, rect);
    });

    // Setup options button icon and popup
    ui->optionsButton->setIcon(QIcon(QString(":/icons/gear")));
    connect(ui->optionsButton, &QPushButton::clicked, this, [this]() {
        showCharSizePopup();
    });

    // Setup range selector as IconComboBox
    static_cast<IconComboBox*>(ui->rangeSelector)->setHeaderType(IconComboBox::LabelOnly);
    ui->rangeSelector->setFixedWidth(100);

    // Connect signals
    connect(ui->searchEntry, &QLineEdit::textChanged, this, &CharacterViewer::refresh);
    connect(static_cast<IconComboBox*>(ui->rangeSelector), &IconComboBox::currentChanged, this, &CharacterViewer::refresh);

    // Setup SimpleGrid
    ui->charGrid->setCellSize(_cellSize, _cellSize);
    ui->charGrid->setGap(1, 1);
    ui->charGrid->setFocusPolicy(Qt::StrongFocus);

    connect(ui->charGrid, &SimpleGrid::cellSelected, this, [this](int index) {
        updateSelectedChar(index);
    });

    connect(ui->charGrid, &SimpleGrid::cellOpened, this, [this](int index) {
        if (!_font || index < 0 || index >= _characters.size()) return;

        auto [unicode, glyph_index] = _characters.at(index);
        if (unicode) {
            char u[10];
            auto const len = g_unichar_to_utf8(unicode, u);
            u[len] = '\0';
            Q_EMIT insertText(QString::fromUtf8(u));
        }
    });

    ui->charGrid->setDrawFunc([this](QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool selected) {
        drawGridCell(painter, index, rect, selected);
    });

    ui->charGrid->setTooltipFunc([this](int index) -> QString {
        if (_font && index >= 0 && index < _characters.size()) {
            auto [unicode, glyph_index] = _characters.at(index);
            return QString::fromStdString(Util::get_unicode_name(unicode));
        }
        return QString();
    });

    // Populate range selector
    auto ranges = Util::get_unicode_ranges();
    for (size_t i = 0; i < ranges.size(); ++i) {
        static_cast<IconComboBox*>(ui->rangeSelector)->addRow("", QString::fromStdString(ranges[i].name), i);
    }
    static_cast<IconComboBox*>(ui->rangeSelector)->setActiveById(0);

    // Load saved character size preference
    auto charSize = Preferences::get()->getInt("/options/charmap/char-size", _cellSize);
    auto it = std::find(charSizes.begin(), charSizes.end(), charSize);
    if (it != charSizes.end()) {
        _cellSize = charSize;
    } else {
        _cellSize = charSizes[2]; // Default to 30
    }
    ui->charGrid->setCellSize(_cellSize, _cellSize);
}

void CharacterViewer::setFont(FontInstance* font, const QString& name) {
    _font = font;
    auto currentIndex = static_cast<IconComboBox*>(ui->rangeSelector)->getActiveRowId();
    auto ranges = Util::get_unicode_ranges();
    if (currentIndex < 0 || currentIndex >= ranges.size()) {
        currentIndex = 0;
    }
    showCharacters(ranges[currentIndex].from, ranges[currentIndex].to, ui->searchEntry->text());

    ui->fontName->setText(name);
    ui->fontName->setToolTip(name);

    if (!font) {
        ui->charGrid->clear();
    }
}

void CharacterViewer::refresh() {
    auto currentIndex = static_cast<IconComboBox*>(ui->rangeSelector)->getActiveRowId();
    auto ranges = Util::get_unicode_ranges();
    if (currentIndex < 0 || currentIndex >= ranges.size()) {
        currentIndex = 0;
    }
    showCharacters(ranges[currentIndex].from, ranges[currentIndex].to, ui->searchEntry->text());
}

void CharacterViewer::setCharSize(int size) {
    _cellSize = size;
    ui->charGrid->setCellSize(_cellSize, _cellSize);
    // Save preference
    Preferences::get()->setInt("/options/charmap/char-size", _cellSize);
}

void CharacterViewer::showCharSizePopup() {
    if (!_charSizePopup) {
        _charSizePopup = new PopupMenu(this);

        auto popupContent = new QWidget();
        popupContent->setMinimumWidth(180);
        auto layout = new QVBoxLayout(popupContent);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        auto label = new QLabel("Character size");
        label->setProperty("class", "panel-label");
        layout->addWidget(label);

        // Value label to show current size
        auto valueLabel = new QLabel();
        valueLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(valueLabel);

        _charSizeSlider = new QSlider(Qt::Horizontal);
        _charSizeSlider->setRange(0, 6);
        _charSizeSlider->setTickPosition(QSlider::TicksBelow);
        _charSizeSlider->setTickInterval(1);
        layout->addWidget(_charSizeSlider);

        auto sizeLabels = new QHBoxLayout();
        auto smallLabel = new QLabel("Small");
        auto bigLabel = new QLabel("Big");
        smallLabel->setProperty("class", "info-text");
        bigLabel->setProperty("class", "info-text");
        bigLabel->setAlignment(Qt::AlignRight);
        sizeLabels->addWidget(smallLabel);
        sizeLabels->addStretch();
        sizeLabels->addWidget(bigLabel);
        layout->addLayout(sizeLabels);

        // Find current size index
        auto it = std::find(charSizes.begin(), charSizes.end(), _cellSize);
        if (it != charSizes.end()) {
            _charSizeSlider->setValue(std::distance(charSizes.begin(), it));
        }

        // Update value label
        auto updateValueLabel = [this, valueLabel]() {
            int index = _charSizeSlider->value();
            if (index >= 0 && index < charSizes.size()) {
                valueLabel->setText(QString::number(charSizes[index]));
            }
        };
        updateValueLabel();

        connect(_charSizeSlider, &QSlider::valueChanged, this, [this, updateValueLabel](int value) {
            updateValueLabel();
            if (value >= 0 && value < charSizes.size()) {
                setCharSize(charSizes[value]);
            }
        });

        _charSizePopup->setContent(popupContent);
    }

    _charSizePopup->showBelowWidget(ui->optionsButton);
}

void CharacterViewer::updateSelectedChar(int index) {
    _currentCell = index;
    if (index < 0 || index >= static_cast<int>(_characters.size())) return;

    auto [unicode, glyph_index] = _characters.at(index);
    QColor dimmed = ui->charName->palette().color(QPalette::WindowText);
    dimmed.setAlphaF(dimmed.alphaF() * 0.5);
    QString result = QString("<br>U+%1<br><br><span style=\"color: %2\">%3</span>")
        .arg(QString::number(unicode, 16).toUpper().rightJustified(4, '0'))
        .arg(dimmed.name(QColor::HexArgb))
        .arg(QString::fromStdString(Util::get_unicode_name(unicode)).toHtmlEscaped());
    ui->charName->setText(result);
    ui->glyphPreview->update();
}

void CharacterViewer::showCharacters(std::uint32_t from, std::uint32_t to, const QString& filter) {
    // remember the selected character so it can be reselected after the grid is repopulated
    std::optional<std::uint32_t> selected_char;
    if (_currentCell >= 0 && _currentCell < static_cast<int>(_characters.size())) {
        selected_char = _characters[_currentCell].unicode;
    }

    _characters.clear();
    _currentCell = -1;
    ui->charGrid->setCellCount(0);
    ui->charName->setText("");
    ui->glyphPreview->update();

    if (!_font) return;

    auto characters = _font->find_all_characters(from, to);

    if (!filter.isEmpty()) {
        QString filterUpper = filter.toUpper();
        std::copy_if(characters.begin(), characters.end(), std::back_inserter(_characters),
            [&filterUpper](const FontInstance::CharInfo& info) {
                QString name = QString::fromStdString(Util::get_unicode_name(info.unicode));
                return name.contains(filterUpper, Qt::CaseInsensitive);
            });
    } else {
        _characters = std::move(characters);
    }
    ui->charGrid->setCellCount(_characters.size());

    // reselect the same unicode glyph if it is still present
    if (!selected_char) return;
    auto it = std::find_if(_characters.begin(), _characters.end(),
        [selected_char](const FontInstance::CharInfo& info) { return info.unicode == *selected_char; });
    if (it == _characters.end()) return;
    int index = static_cast<int>(std::distance(_characters.begin(), it));
    ui->charGrid->setSelectedCell(index);
    updateSelectedChar(index);
}

void CharacterViewer::drawGlyphPreview(QPainter* painter, const QRect& rect) {
    if (!_font || _currentCell < 0 || _characters.empty()) return;

    auto [unicode, glyph_index] = _characters.at(_currentCell);

    auto fg = palette().color(QPalette::Text);
    QColor line = fg;
    line.setAlphaF(0.15);

    Linea::UI::drawGlyph({
        .font = _font,
        .font_size = 0, // auto
        .glyph_index = glyph_index,
        .painter = painter,
        .rect = Geom::IntRect::from_xywh(rect.x(), rect.y(), rect.width(), rect.height()),
        .glyph_color = fg,
        .line_color = line,
        .background_color = QColor(),
        .draw_metrics = true,
        .draw_background = false
    });
}

void CharacterViewer::drawGridCell(QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool selected) {
    if (!_font || index >= _characters.size()) return;

    auto [unicode, glyph_index] = _characters.at(index);

    QColor bgColor = QColor(0x0D, 0x6E, 0xFF); // Selection blue
    QColor fgColor = palette().color(QPalette::Text);

    if (selected) {
        // Use selection colors from palette
        bgColor = palette().color(QPalette::Highlight);
        fgColor = palette().color(QPalette::HighlightedText);
    }

    // Draw background if selected
    if (selected) {
        painter->fillRect(QRect(rect.left(), rect.top(), rect.width(), rect.height()), bgColor);
    }

    // Use Qt glyph drawing
    Linea::UI::drawGlyph({
        .font = _font,
        .font_size = 0, // auto
        .glyph_index = glyph_index,
        .painter = painter,
        .rect = rect,
        .glyph_color = fgColor,
        .line_color = QColor(),
        .background_color = selected ? bgColor : QColor(),
        .draw_metrics = false,
        .draw_background = selected
    });
}

} // namespace Linea::UI
