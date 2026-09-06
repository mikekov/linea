// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINEA_UI_FILTER_GALLERY_H
#define LINEA_UI_FILTER_GALLERY_H

#include <QWidget>

#include <memory>
#include "document.h"

class QSortFilterProxyModel;
class QStandardItemModel;

namespace Inkscape::Extension {
class Effect;
}

namespace Ui {
class FilterGallery;
}

namespace Linea::UI {

class FilterGallery : public QWidget {
    Q_OBJECT

public:
    explicit FilterGallery(QWidget* parent = nullptr);
    ~FilterGallery() override;

Q_SIGNALS:
    void itemActivated(const QString& id);
    void actionRequested(const QString& id);

private:
    void activate(const QModelIndex& index);
    void requestAction(const QModelIndex& index);

    std::unique_ptr<Ui::FilterGallery> _ui;
    QStandardItemModel* _model = nullptr;
    QStandardItemModel* _categories = nullptr;
    QSortFilterProxyModel* _filter = nullptr;
    std::unique_ptr<SPDocument> _document;
};

} // namespace Linea::UI

#endif // LINEA_UI_FILTER_GALLERY_H
