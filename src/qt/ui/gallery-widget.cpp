// SPDX-License-Identifier: GPL-2.0-or-later

#include "gallery-widget.h"

#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QListView>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>

#include <algorithm>

#include "ui_gallery-widget.h"

namespace Linea::UI {

QString cleanGalleryName(const char* value) {
    auto name = QString::fromUtf8(value ? value : "");
    name.remove(QStringLiteral("..."));
    name.remove(QChar(0x2026));
    name.remove(QChar('_'));
    return name;
}

GalleryWidget::GalleryWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::GalleryWidget>()) {
    _ui->setupUi(this);

    _delegate = new GalleryItemDelegate(_ui->items);
    _ui->items->setItemDelegate(_delegate);
    _ui->items->setViewMode(QListView::IconMode);
    _ui->items->setFlow(QListView::LeftToRight);
    _ui->items->setWrapping(true);
    _ui->items->setResizeMode(QListView::Adjust);
    _ui->items->setLayoutMode(QListView::Batched);
    _ui->items->setBatchSize(24);
    _ui->items->setUniformItemSizes(false);
    _ui->items->setMovement(QListView::Static);
    _ui->items->setSelectionMode(QAbstractItemView::SingleSelection);
    _ui->items->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _ui->items->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _ui->items->setWordWrap(true);
    _ui->categories->setSelectionMode(QAbstractItemView::SingleSelection);
    _ui->categories->setUniformItemSizes(true);
    _ui->categories->setMovement(QListView::Static);
    _ui->splitter->setChildrenCollapsible(false);
    _ui->splitter->setStretchFactor(0, 0);
    _ui->splitter->setStretchFactor(1, 1);
    QTimer::singleShot(0, this, [this]() {
        auto totalWidth = _ui->splitter->width();
        if (totalWidth <= 0) return;

        auto categoryWidth = std::min(150, totalWidth - 1);
        _ui->splitter->setSizes({categoryWidth, totalWidth - categoryWidth});
    });

    _footerButtonText = _ui->footerButton->text();
    _footerButtonSuffix = QStringLiteral("...");

    connect(_ui->items, &QListView::doubleClicked, this, &GalleryWidget::itemActivated);
    connect(_ui->items, &QListView::activated, this, &GalleryWidget::itemActivated);
    connect(_ui->footerButton, &QPushButton::clicked, this, [this]() {
        auto index = _ui->items->currentIndex();
        if (index.isValid()) {
            Q_EMIT actionRequested(index);
        }
    });
}

GalleryWidget::~GalleryWidget() = default;

QString GalleryWidget::headerText() const {
    return _ui->header->text();
}

void GalleryWidget::setHeaderText(const QString& text) {
    _ui->header->setText(text);
}

bool GalleryWidget::headerVisible() const {
    return _ui->header->isVisible();
}

void GalleryWidget::setHeaderVisible(bool visible) {
    _ui->header->setVisible(visible);
}

QString GalleryWidget::footerButtonText() const {
    return _footerButtonText;
}

void GalleryWidget::setFooterButtonText(const QString& text) {
    _footerButtonText = text;
    updateFooter(_ui->items->currentIndex());
}

QString GalleryWidget::footerButtonSuffix() const {
    return _footerButtonSuffix;
}

void GalleryWidget::setFooterButtonSuffix(const QString& suffix) {
    _footerButtonSuffix = suffix;
    updateFooter(_ui->items->currentIndex());
}

bool GalleryWidget::footerButtonVisible() const {
    return _ui->footerButton->isVisible();
}

void GalleryWidget::setFooterButtonVisible(bool visible) {
    _ui->footerButton->setVisible(visible);
}

bool GalleryWidget::showDescriptions() const {
    return _delegate ? _delegate->showDescriptions() : true;
}

void GalleryWidget::setShowDescriptions(bool show) {
    if (!_delegate) return;

    _delegate->setShowDescriptions(show);
    _ui->items->doItemsLayout();
    _ui->items->viewport()->update();
}

int GalleryWidget::cardWidth() const {
    return _delegate ? _delegate->cardWidth() : 0;
}

void GalleryWidget::setCardWidth(int width) {
    if (!_delegate) return;

    _delegate->setCardWidth(width);
    _ui->items->doItemsLayout();
    _ui->items->viewport()->update();
}

int GalleryWidget::previewCacheLimit() const {
    return _delegate ? _delegate->cacheLimit() : 0;
}

void GalleryWidget::setPreviewCacheLimit(int limit) {
    if (_delegate) {
        _delegate->setCacheLimit(limit);
    }
}

void GalleryWidget::setModel(QAbstractItemModel* model) {
    QObject::disconnect(_selectionConnection);
    _ui->items->setModel(model);
    auto selectionModel = _ui->items->selectionModel();
    if (!selectionModel) {
        updateFooter({});
        return;
    }

    _selectionConnection = connect(selectionModel, &QItemSelectionModel::currentChanged,
                                   this, [this](const QModelIndex& current, const QModelIndex&) {
                                       updateFooter(current);
                                       Q_EMIT currentItemChanged(current);
                                   });
    updateFooter(_ui->items->currentIndex());
}

QAbstractItemModel* GalleryWidget::model() const {
    return _ui->items->model();
}

void GalleryWidget::setCategoryModel(QAbstractItemModel* model) {
    QObject::disconnect(_categoryConnection);
    _ui->categories->setModel(model);
    auto selectionModel = _ui->categories->selectionModel();
    if (!selectionModel) return;

    _categoryConnection = connect(selectionModel, &QItemSelectionModel::currentChanged,
                                  this, [this](const QModelIndex& current, const QModelIndex&) {
                                      Q_EMIT categoryChanged(current);
                                  });
    if (model && model->rowCount() > 0) {
        _ui->categories->setCurrentIndex(model->index(0, 0));
    }
}

QAbstractItemModel* GalleryWidget::categoryModel() const {
    return _ui->categories->model();
}

void GalleryWidget::setPreviewProvider(GalleryItemDelegate::PreviewProvider provider) {
    if (!_delegate) return;

    _delegate->setPreviewProvider(std::move(provider));
    _ui->items->viewport()->update();
}

void GalleryWidget::setThumbnailSize(const QSize& size) {
    if (!_delegate || size.isEmpty()) return;

    _delegate->setThumbnailSize(size);
    _ui->items->doItemsLayout();
    _ui->items->viewport()->update();
}

QSize GalleryWidget::thumbnailSize() const {
    return _delegate ? _delegate->thumbnailSize() : QSize();
}

void GalleryWidget::clearPreviewCache() {
    if (!_delegate) return;

    _delegate->clearCache();
    _ui->items->viewport()->update();
}

void GalleryWidget::updateFooter(const QModelIndex& index) {
    if (!index.isValid()) {
        // _ui->selectedItem->clear();
        // _ui->selectedItem->setToolTip(QString());
        _ui->footerInfo->clear();
        _ui->footerInfo->setToolTip(QString());
        _ui->footerButton->setEnabled(false);
        _ui->footerButton->setText(_footerButtonText);
        return;
    }

    auto label = index.data(Qt::DisplayRole).toString();
    auto description = index.data(DescriptionRole).toString();
    auto actionLabel = index.data(ActionLabelRole).toString();
    if (actionLabel.isEmpty()) {
        actionLabel = _footerButtonText;
        if (index.data(RequiresParametersRole).toBool()) {
            actionLabel += _footerButtonSuffix;
        }
    }

    // _ui->selectedItem->setText(label);
    // _ui->selectedItem->setToolTip(label);
    _ui->footerInfo->setText(description);
    _ui->footerInfo->setToolTip(description);
    _ui->footerButton->setText(actionLabel);
    _ui->footerButton->setEnabled(true);
}

} // namespace Linea::UI
