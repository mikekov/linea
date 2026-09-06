// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINEA_UI_GALLERY_FILTER_PROXY_H
#define LINEA_UI_GALLERY_FILTER_PROXY_H

#include <QSortFilterProxyModel>

namespace Linea::UI {

class GalleryFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit GalleryFilterProxyModel(QObject* parent = nullptr);

    void setSearchText(const QString& text);
    void setCategory(const QString& category);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString _searchText;
    QString _category;
};

} // namespace Linea::UI

#endif // LINEA_UI_GALLERY_FILTER_PROXY_H
