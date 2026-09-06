// SPDX-License-Identifier: GPL-2.0-or-later

#include "gallery-filter-proxy.h"

#include "gallery-item-delegate.h"

namespace Linea::UI {

GalleryFilterProxyModel::GalleryFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent) {}

void GalleryFilterProxyModel::setSearchText(const QString& text) {
    _searchText = text.trimmed();
    invalidateFilter();
}

void GalleryFilterProxyModel::setCategory(const QString& category) {
    _category = category;
    invalidateFilter();
}

bool GalleryFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    auto source = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!_category.isEmpty() && source.data(CategoryRole).toString() != _category) {
        return false;
    }
    if (_searchText.isEmpty()) return true;

    auto matches = [this, &source](int role) {
        return source.data(role).toString().contains(_searchText, Qt::CaseInsensitive);
    };
    return matches(Qt::DisplayRole) || matches(AccessRole) || matches(DescriptionRole);
}

} // namespace Linea::UI
