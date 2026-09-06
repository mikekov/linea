// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINEA_UI_GALLERY_ITEM_DELEGATE_H
#define LINEA_UI_GALLERY_ITEM_DELEGATE_H

#include <QImage>
#include <QSize>
#include <QStyledItemDelegate>

#include <functional>

class QModelIndex;

namespace Linea::UI {

enum GalleryItemRole {
    CategoryRole = Qt::UserRole + 1,
    DescriptionRole,
    PreviewKeyRole,
    RequiresParametersRole,
    ActionLabelRole,
    AccessRole = Qt::UserRole + 10
};

class GalleryItemDelegate : public QStyledItemDelegate {
public:
    using PreviewProvider = std::function<QImage (const QModelIndex&, const QSize&, qreal)>;

    explicit GalleryItemDelegate(QObject* parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    void setPreviewProvider(PreviewProvider provider);
    void setThumbnailSize(const QSize& size);
    QSize thumbnailSize() const;
    void setCardWidth(int width);
    int cardWidth() const;
    void setShowDescriptions(bool show);
    bool showDescriptions() const;
    void setCacheLimit(int limit);
    int cacheLimit() const;
    void clearCache() const;

private:
    QImage preview(const QModelIndex& index, qreal devicePixelRatio) const;
    QString cacheKey(const QModelIndex& index, qreal devicePixelRatio) const;
    int textHeight(const QFont& font, const QString& text, int width) const;
    void cacheImage(const QString& key, QImage image) const;

    PreviewProvider _previewProvider;
    QSize _thumbnailSize = {128, 96};
    int _cardWidth = 180;
    bool _showDescriptions = true;
    int _cacheLimit = 1000;
    mutable QHash<QString, QImage> _cache;
    mutable QList<QString> _cacheOrder;
};

} // namespace Linea::UI

#endif // LINEA_UI_GALLERY_ITEM_DELEGATE_H
