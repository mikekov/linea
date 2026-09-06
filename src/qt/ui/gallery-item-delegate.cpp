// SPDX-License-Identifier: GPL-2.0-or-later

#include "gallery-item-delegate.h"

#include <QAbstractItemModel>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <algorithm>

#include "qt/util/drawing-utils.h"

namespace Linea::UI {

namespace {
constexpr int Margin = 8;
constexpr int PreviewSpacing = 6;
constexpr int DescriptionSpacing = 3;
constexpr int PageMargin = 6;
}

GalleryItemDelegate::GalleryItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void GalleryItemDelegate::setPreviewProvider(PreviewProvider provider) {
    _previewProvider = std::move(provider);
    clearCache();
}

void GalleryItemDelegate::setThumbnailSize(const QSize& size) {
    if (size == _thumbnailSize) return;

    _thumbnailSize = size;
    clearCache();
}

QSize GalleryItemDelegate::thumbnailSize() const {
    return _thumbnailSize;
}

void GalleryItemDelegate::setCardWidth(int width) {
    width = std::max(1, width);
    if (width == _cardWidth) return;

    _cardWidth = width;
}

int GalleryItemDelegate::cardWidth() const {
    return _cardWidth;
}

void GalleryItemDelegate::setShowDescriptions(bool show) {
    if (_showDescriptions == show) return;

    _showDescriptions = show;
}

bool GalleryItemDelegate::showDescriptions() const {
    return _showDescriptions;
}

void GalleryItemDelegate::setCacheLimit(int limit) {
    _cacheLimit = std::max(0, limit);
    while (_cacheOrder.size() > _cacheLimit) {
        _cache.remove(_cacheOrder.takeFirst());
    }
}

int GalleryItemDelegate::cacheLimit() const {
    return _cacheLimit;
}

void GalleryItemDelegate::clearCache() const {
    _cache.clear();
    _cacheOrder.clear();
}

int GalleryItemDelegate::textHeight(const QFont& font, const QString& text, int width) const {
    if (text.isEmpty() || width <= 0) return 0;

    QFontMetrics metrics(font);
    return metrics.boundingRect(QRect(0, 0, width, 10000), Qt::TextWordWrap, text).height();
}

QSize GalleryItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    auto width = std::max(1, _cardWidth);
    auto textWidth = width - 2 * Margin;
    auto title = index.data(Qt::DisplayRole).toString();
    auto description = _showDescriptions ? index.data(DescriptionRole).toString() : QString();
    auto titleHeight = textHeight(option.font, title, textWidth);
    auto descriptionHeight = textHeight(option.font, description, textWidth);

    auto previewHeight = _thumbnailSize.height() + 2 * PageMargin;
    auto height = 2 * Margin + previewHeight;
    if (titleHeight > 0) {
        height += PreviewSpacing + titleHeight;
    }
    if (descriptionHeight > 0) {
        height += DescriptionSpacing + descriptionHeight;
    }

    return {width, height};
}

QString GalleryItemDelegate::cacheKey(const QModelIndex& index, qreal devicePixelRatio) const {
    auto key = index.data(PreviewKeyRole).toString();
    if (key.isEmpty()) {
        key = index.data(Qt::DisplayRole).toString() + QLatin1Char(':') + QString::number(index.row());
    }

    return key + QLatin1Char(':') + QString::number(_thumbnailSize.width()) + QLatin1Char('x') +
           QString::number(_thumbnailSize.height()) + QLatin1Char(':') + QString::number(devicePixelRatio);
}

void GalleryItemDelegate::cacheImage(const QString& key, QImage image) const {
    if (_cacheLimit <= 0 || image.isNull()) return;

    if (_cache.contains(key)) {
        _cache[key] = std::move(image);
        _cacheOrder.removeAll(key);
        _cacheOrder.append(key);
        return;
    }

    _cache.insert(key, std::move(image));
    _cacheOrder.append(key);
    while (_cacheOrder.size() > _cacheLimit) {
        _cache.remove(_cacheOrder.takeFirst());
    }
}

QImage GalleryItemDelegate::preview(const QModelIndex& index, qreal devicePixelRatio) const {
    auto key = cacheKey(index, devicePixelRatio);
    if (auto it = _cache.constFind(key); it != _cache.constEnd()) {
        _cacheOrder.removeAll(key);
        _cacheOrder.append(key);
        return *it;
    }

    if (!_previewProvider) return {};

    auto image = _previewProvider(index, _thumbnailSize, devicePixelRatio);
    cacheImage(key, image);
    return image;
}

void GalleryItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
    if (!painter || !index.isValid()) return;

    painter->save();

    QStyleOptionViewItem itemOption(option);
    itemOption.text.clear();
    if (itemOption.widget) {
        itemOption.widget->style()->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, itemOption.widget);
    }

    auto rect = option.rect.adjusted(Margin, Margin, -Margin, -Margin);
    auto previewSize = _thumbnailSize + QSize(2 * PageMargin, 2 * PageMargin);
    auto previewRect = QRect(QPoint(0, 0), previewSize);
    previewRect.moveCenter(QPoint(rect.center().x(), rect.top() + previewSize.height() / 2));

    auto pageRect = previewRect.adjusted(PageMargin, PageMargin, -PageMargin, -PageMargin);

    drawPageShadow(*painter, pageRect, PageMargin, QColor(0, 0, 0), 0.30);
    painter->fillRect(pageRect, Qt::white);

    auto image = preview(index, option.widget ? option.widget->devicePixelRatioF() : 1.0);
    if (!image.isNull()) {
        auto imageSize = image.deviceIndependentSize().toSize();
        auto imageRect = QRect(QPoint(), imageSize);
        imageRect.moveCenter(pageRect.center());
        painter->drawImage(imageRect, image);
    }

    auto textRect = QRect(rect.left(), previewRect.bottom() + PreviewSpacing,
                          rect.width(), rect.bottom() - previewRect.bottom() - PreviewSpacing);
    auto title = index.data(Qt::DisplayRole).toString();
    auto description = _showDescriptions ? index.data(DescriptionRole).toString() : QString();
    auto textColor = (option.state & QStyle::State_Selected)
        ? option.palette.color(QPalette::HighlightedText)
        : option.palette.color(QPalette::WindowText);

    painter->setPen(textColor);
    auto titleHeight = textHeight(option.font, title, textRect.width());
    if (titleHeight > 0) {
        auto titleRect = QRect(textRect.left(), textRect.top(), textRect.width(), titleHeight);
        painter->drawText(titleRect, Qt::TextWordWrap | Qt::AlignHCenter, title);
        textRect.setTop(titleRect.bottom() + DescriptionSpacing);
    }

    if (!description.isEmpty() && textRect.height() > 0) {
        auto descriptionFont = option.font;
        descriptionFont.setPointSizeF(descriptionFont.pointSizeF() * 0.9);
        painter->setFont(descriptionFont);
        painter->drawText(textRect, Qt::TextWordWrap | Qt::AlignHCenter, description);
    }

    painter->restore();
}

} // namespace Linea::UI
