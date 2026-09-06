// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINEA_UI_GALLERY_WIDGET_H
#define LINEA_UI_GALLERY_WIDGET_H

#include <QSize>
#include <QString>
#include <QWidget>

#include <memory>

#include "gallery-item-delegate.h"

class QAbstractItemModel;
class QModelIndex;

namespace Ui {
class GalleryWidget;
}

namespace Linea::UI {

QString cleanGalleryName(const char* value);

class GalleryWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString headerText READ headerText WRITE setHeaderText)
    Q_PROPERTY(bool headerVisible READ headerVisible WRITE setHeaderVisible)
    Q_PROPERTY(QString footerButtonText READ footerButtonText WRITE setFooterButtonText)
    Q_PROPERTY(QString footerButtonSuffix READ footerButtonSuffix WRITE setFooterButtonSuffix)
    Q_PROPERTY(bool footerButtonVisible READ footerButtonVisible WRITE setFooterButtonVisible)
    Q_PROPERTY(bool showDescriptions READ showDescriptions WRITE setShowDescriptions)
    Q_PROPERTY(int cardWidth READ cardWidth WRITE setCardWidth)
    Q_PROPERTY(int previewCacheLimit READ previewCacheLimit WRITE setPreviewCacheLimit)

public:
    explicit GalleryWidget(QWidget* parent = nullptr);
    ~GalleryWidget() override;

    QString headerText() const;
    void setHeaderText(const QString& text);
    bool headerVisible() const;
    void setHeaderVisible(bool visible);

    QString footerButtonText() const;
    void setFooterButtonText(const QString& text);

    QString footerButtonSuffix() const;
    void setFooterButtonSuffix(const QString& suffix);

    bool footerButtonVisible() const;
    void setFooterButtonVisible(bool visible);

    bool showDescriptions() const;
    void setShowDescriptions(bool show);

    int cardWidth() const;
    void setCardWidth(int width);

    int previewCacheLimit() const;
    void setPreviewCacheLimit(int limit);

    void setModel(QAbstractItemModel* model);
    QAbstractItemModel* model() const;
    void setCategoryModel(QAbstractItemModel* model);
    QAbstractItemModel* categoryModel() const;

    void setPreviewProvider(GalleryItemDelegate::PreviewProvider provider);
    void setThumbnailSize(const QSize& size);
    QSize thumbnailSize() const;
    void clearPreviewCache();

Q_SIGNALS:
    void itemActivated(const QModelIndex& index);
    void currentItemChanged(const QModelIndex& index);
    void categoryChanged(const QModelIndex& index);
    void actionRequested(const QModelIndex& index);

private:
    void updateFooter(const QModelIndex& index);

    std::unique_ptr<Ui::GalleryWidget> _ui;
    GalleryItemDelegate* _delegate = nullptr;
    QString _footerButtonText;
    QString _footerButtonSuffix;
    QMetaObject::Connection _selectionConnection;
    QMetaObject::Connection _categoryConnection;
};

} // namespace Linea::UI

#endif // LINEA_UI_GALLERY_WIDGET_H
