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

#include "color-palette-options.h"

#include <QColor>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyledItemDelegate>
#include <variant>

#include "radio-toggle.h"
#include "ui/dialog/global-palettes.h"
#include "ui_color-palette-options.h"

namespace Linea::UI {

using Inkscape::UI::Dialog::PaletteFileData;
using ColorItem = PaletteFileData::ColorItem;

// ─── delegate ───────────────────────────────────────────────────────────────

/// Extract solid colors from a palette's ColorItem list, skipping non-color entries.
static std::vector<QColor> extractColors(const PaletteFileData& palette) {
    std::vector<QColor> colors;
    colors.reserve(palette.colors.size());
    for (const auto& item : palette.colors) {
        if (const auto* c = std::get_if<Inkscape::Colors::Color>(&item)) {
            colors.emplace_back(QColor::fromRgba(static_cast<QRgb>(c->toARGB())));
        }
    }
    return colors;
}

/**
 * Item delegate that draws a palette's name on top and a color preview strip below.
 *
 * The preview strip is a port of ColorPalettePreview::draw_func (Cairo) to QPainter:
 * for each pixel column across the strip width, a color is picked from the palette
 * by proportional index, producing a continuous gradient-like preview.
 */
class PaletteListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    static constexpr int PreviewHeight = 6;
    static constexpr int Margin = 6;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);

        auto& opt = const_cast<QStyleOptionViewItem&>(option);
        opt.text = QString(); // prevent default text drawing

        // Draw selection / hover background via the style
        if (opt.widget) {
            opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
        }

        const bool selected = opt.state & QStyle::State_Selected;
        const auto& textCol =
            selected ? opt.palette.color(QPalette::HighlightedText) : opt.palette.color(QPalette::WindowText);

        const int textHeight = opt.fontMetrics.height();
        const auto rect = opt.rect.adjusted(Margin, Margin / 2, -Margin, -Margin / 2);
        const int stripY = rect.bottom() - PreviewHeight + 1;
        const int textW = rect.width();

        // Draw palette name
        painter->setPen(textCol);
        painter->drawText(rect.left(), rect.top(), textW, textHeight,
             Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());

        // Draw color preview strip
        // Colors are looked up from GlobalPalettes by index stored in UserRole.
        const int palIndex = index.data(Qt::UserRole).toInt();
        const auto& palettes = Inkscape::UI::Dialog::GlobalPalettes::get().palettes();
        if (palIndex >= 0 && palIndex < static_cast<int>(palettes.size())) {
            auto colors = extractColors(palettes[palIndex]);
            if (!colors.empty() && rect.width() > 0) {
                const int w = rect.width();
                const int h = PreviewHeight;
                const int x0 = rect.left();
                const int y0 = stripY;

                for (int i = 0, px = 0; i < w && px < w; ++i, ++px) {
                    const int ci = i * static_cast<int>(colors.size()) / w;
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(colors[ci]);
                    painter->drawRect(x0 + px, y0, 1, h);
                }
            }
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& /*index*/) const override {
        const int textHeight = option.fontMetrics.height();
        return {200, Margin + textHeight + PreviewHeight + Margin};
    }
};

// ─── main widget ────────────────────────────────────────────────────────────

ColorPaletteOptions::ColorPaletteOptions(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::ColorPaletteOptions>()) {
    _ui->setupUi(this);

    _delegate = new PaletteListDelegate(_ui->palettesList);
    _ui->palettesList->setItemDelegate(_delegate);
    _ui->palettesList->setSelectionMode(QAbstractItemView::SingleSelection);

    // The size group is a single RadioButtonGroup created in the UI file;
    // buttonIcons/buttonValues/buttonToolTips are set via properties there.
    _sizeGroup = _ui->sizeGroup;

    connect(_ui->palettesList, &QListWidget::currentRowChanged, this, [this](int row) { Q_EMIT paletteChanged(row); });

    connect(_sizeGroup, &RadioToggle::valueChanged, this,
            [this](int v) { Q_EMIT tileSizeChanged(static_cast<TileSize>(v)); });

    populatePalettes();
}

ColorPaletteOptions::~ColorPaletteOptions() = default;

void ColorPaletteOptions::populatePalettes() {
    _ui->palettesList->clear();

    const auto& palettes = Inkscape::UI::Dialog::GlobalPalettes::get().palettes();
    for (int i = 0; i < static_cast<int>(palettes.size()); ++i) {
        const auto& pal = palettes[i];
        auto name = QString::fromStdString(pal.name.raw());

        auto* item = new QListWidgetItem(name, _ui->palettesList);
        item->setData(Qt::UserRole, i);
        _ui->palettesList->addItem(item);
    }
}

int ColorPaletteOptions::selectedPaletteIndex() const {
    return _ui->palettesList->currentRow();
}

void ColorPaletteOptions::setSelectedPaletteIndex(int index) {
    _ui->palettesList->setCurrentRow(index);
}

ColorPaletteOptions::TileSize ColorPaletteOptions::tileSize() const {
    return static_cast<TileSize>(_sizeGroup->value());
}

void ColorPaletteOptions::setTileSize(TileSize size) {
    _sizeGroup->setValue(static_cast<int>(size));
}

} // namespace Linea::UI

#include "color-palette-options.moc"
