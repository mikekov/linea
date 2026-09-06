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
#include <QPainterPath>
#include <QRect>
#include <QString>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdint>

#include "colors/utils.h"

namespace Linea::UI {

using Inkscape::UI::Dialog::PaletteFileData;

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
        if (index >= 0 && index < static_cast<int>(_palette.colors.size())) {
            emitColorClicked(_palette.colors[index], modifiers, buttons);
        }
    });

    _basicGrid->setDrawFunc(
        [this](QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool /*selected*/) {
            if (index >= _basics.size()) return;
            paintColor(painter, rect, _basics[index]);
        });

    _grid->setDrawFunc([this](QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool /*selected*/) {
        if (index >= _palette.colors.size()) return;
        paintColor(painter, rect, _palette.colors[index]);
    });

    _basicGrid->setTooltipFunc([this](int index) -> QString {
        if (index < 0 || index >= static_cast<int>(_basics.size())) {
            return {};
        }
        return colorTooltip(_basics[index]);
    });

    _grid->setTooltipFunc([this](int index) -> QString {
        if (index < 0 || index >= static_cast<int>(_palette.colors.size())) {
            return {};
        }
        return colorTooltip(_palette.colors[index]);
    });
}

void ColorPaletteWidget::setMargins(int left, int top, int right, int bottom) {
    _layout->setContentsMargins(left, top, right, bottom);
}

void ColorPaletteWidget::setBasicColors(std::vector<PaletteFileData::ColorItem> basics) {
    _basics = std::move(basics);
    _basicGrid->setCellCount(_basics.size());
    _basicGrid->invalidate();
}

void ColorPaletteWidget::setPaletteData(PaletteFileData data) {
    _palette = std::move(data);
    _grid->setCellCount(_palette.colors.size());
    _grid->invalidate();
}

void ColorPaletteWidget::paintColor(QPainter* painter, const Geom::IntRect& rect,
                                    const PaletteFileData::ColorItem& item) const {
    auto rgb = QColor::fromRgba(0xffffffff);
    bool none = false;
    const auto* c = std::get_if<Inkscape::Colors::Color>(&item);
    if (c) {
        rgb = QColor::fromRgba(static_cast<QRgb>(c->toARGB()));
    } else if (std::holds_alternative<PaletteFileData::None>(item)) {
        none = true;
    } else {
        // nothing to draw
        return;
    }

    auto tile = QRectF(rect.left(), rect.top(), rect.width(), rect.height());
    double radius = 2.0;
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(rgb);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(tile, radius, radius);

    if (_swatchIndicator) {
        painter->save();

        QPainterPath clip;
        clip.addRoundedRect(tile, radius, radius);
        painter->setClipPath(clip);

        auto side = std::min(tile.width(), tile.height()) / 2.5;
        auto line = 1.5;
        auto left = tile.left();
        auto top = tile.top();

        painter->setPen(Qt::NoPen);

        QPainterPath separator;
        separator.moveTo(left, top + side);
        separator.lineTo(left, top + side - line);
        separator.lineTo(left + side - line, top);
        separator.lineTo(left + side, top);
        separator.closeSubpath();
        painter->setBrush(QColor::fromHslF(0, 0, 1, 0.7));
        painter->drawPath(separator);

        QPainterPath triangle;
        triangle.moveTo(left, top + side - line);
        triangle.lineTo(left, top);
        triangle.lineTo(left + side - line, top);
        triangle.closeSubpath();
        painter->setBrush(QColor::fromHslF(0, 0, 0.25));
        painter->drawPath(triangle);

        painter->restore();
    }

    if (none) {
        // draw diagonal line
        painter->setPen(QPen(Qt::red, 1.5));
        auto r = tile.adjusted(2, 2, -2, -2);
        painter->drawLine(r.bottomLeft(), r.topRight());
    }

    const bool is_fill = none ? _currentFill.none : _currentFill.solid && _currentFill.solid->rgb() == rgb.rgb();
    const bool is_stroke =
        none ? _currentStroke.none : _currentStroke.solid && _currentStroke.solid->rgb() == rgb.rgb();

    if (is_fill || is_stroke) {
        auto color = c ? *c : Inkscape::Colors::Color(0xffffffffu);
        const auto lightness = Inkscape::Colors::get_perceptual_lightness(color);
        auto [gray, alpha] = Inkscape::Colors::get_contrasting_color(lightness);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor::fromRgbF(gray, gray, gray, alpha));

        auto w = tile.width();
        auto h = tile.height();
        auto minwh = std::min(w, h);
        auto center = tile.center();
        auto scale = minwh / 2.0;

        if (is_fill) {
            painter->drawEllipse(center, 0.35 * scale, 0.35 * scale);
        }

        if (is_stroke) {
            QPainterPath ring;
            ring.addEllipse(center, 0.65 * scale, 0.65 * scale);
            ring.addEllipse(center, 0.5 * scale, 0.5 * scale);
            ring.setFillRule(Qt::OddEvenFill);
            painter->drawPath(ring);
        }
    }

    // relief effect
    QLinearGradient grad(rect.left(), rect.top(), rect.right(), rect.bottom());
    auto w = palette().color(QPalette::Window);
    auto b = palette().color(QPalette::WindowText);
    QColor white = w;
    white.setAlphaF(w.alphaF() * 0.30);
    QColor black = b;
    black.setAlphaF(b.alphaF() * 0.15);
    grad.setColorAt(0, white);
    grad.setColorAt(0.48, white);
    white.setAlphaF(0);
    grad.setColorAt(0.49, white);
    auto a = black.alphaF();
    black.setAlphaF(0);
    grad.setColorAt(0.50, black);
    black.setAlphaF(a);
    grad.setColorAt(0.51, black);
    grad.setColorAt(1, black);
    QPen pen(QBrush(grad), 1.0);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    tile.adjust(0.5, 0.5, -0.5, -0.5);
    radius -= 0.5;
    painter->drawRoundedRect(tile, radius, radius);
}

QString ColorPaletteWidget::colorTooltip(const PaletteFileData::ColorItem& item) const {
    if (const auto& color = std::get_if<Inkscape::Colors::Color>(&item)) {
        auto name = color->getName();
        if (!name.empty()) {
            return QString::fromStdString(name);
        }
        return QString::fromStdString(color->toString(false));
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
        Q_EMIT cellClicked(std::nullopt, modifiers, buttons);
    }
}

void ColorPaletteWidget::setAlignment(Alignment alignment) {
    if (_alignment == alignment) return;

    _alignment = alignment;
    _grid->setFlow(_alignment == Alignment::Horizontal ? SimpleGrid::Flow::Horizontal : SimpleGrid::Flow::Vertical);
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

    if (paint.is_single()) {
        if (paint.value().mode == Inkscape::UI::Widget::PaintMode::Solid && paint.value().color) {
            color = QColor::fromRgba(static_cast<QRgb>(paint.value().color->toARGB()));
        } else if (paint.value().mode == Inkscape::UI::Widget::PaintMode::None) {
            isNone = true;
        }
    }

    if (none != isNone || color != solid) {
        none = isNone;
        solid = color;
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

void ColorPaletteWidget::setSwatchIndicator(bool swatch) {
    _swatchIndicator = swatch;
    _grid->invalidate(false);
    _basicGrid->invalidate(false);
}

int ColorPaletteWidget::verticalScrollBarWidth() const {
    return _grid->verticalScrollBarWidth();
}

} // namespace Linea::UI
