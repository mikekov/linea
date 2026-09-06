// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINEA_UI_EXTENSION_GALLERY_H
#define LINEA_UI_EXTENSION_GALLERY_H

#include <QWidget>

#include <memory>

class QSortFilterProxyModel;
class QStandardItemModel;

namespace Ui {
class ExtensionGallery;
}

namespace Linea::UI {

class ExtensionGallery : public QWidget {
    Q_OBJECT

public:
    explicit ExtensionGallery(QWidget* parent = nullptr);
    ~ExtensionGallery() override;

Q_SIGNALS:
    void itemActivated(const QString& id);
    void actionRequested(const QString& id);

private:
    void activate(const QModelIndex& index);
    void requestAction(const QModelIndex& index);

    std::unique_ptr<Ui::ExtensionGallery> _ui;
    QStandardItemModel* _model = nullptr;
    QStandardItemModel* _categories = nullptr;
    QSortFilterProxyModel* _filter = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_EXTENSION_GALLERY_H
