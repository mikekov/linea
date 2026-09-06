// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FilterWidget — Qt widget for managing a single filter on an item.
 *
 * Provides an "Add" button that pops up a searchable list of available
 * filter primitives. At most one filter is shown:
 *  - Gaussian blur: a SpinScale to adjust the blur amount
 *  - Other single primitive: a button with the primitive's name
 *  - Compound (multiple primitives): a "Compound filter" button
 * A remove button clears the filter from the item.
 */

#ifndef LINEA_UI_FILTER_WIDGET_H
#define LINEA_UI_FILTER_WIDGET_H

#include <QWidget>
#include <memory>

#include <sigc++/connection.h>

#include "ui/operation-blocker.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
QT_END_NAMESPACE

class SPObject;
class SPDesktop;

namespace Inkscape::Extension { class Effect; }

namespace Linea::Props {
class Binder;
}

namespace Ui {
class FilterWidget;
}

namespace Linea::UI {

class FilterEditor;
class FilterGallery;
class PopupMenu;
class SpinScale;

/**
 * Widget for managing a single filter on a selected item.
 *
 * Mimics the GTK AttributesPanel::add_filters functionality:
 *  - "Add" button opens a PopupMenu with a searchable list of filter primitives
 *  - At most one filter is shown:
 *    - Gaussian blur: a SpinScale to adjust blur amount
 *    - Other single primitive: a button with the primitive's name
 *    - Compound (multiple primitives): a "Compound filter" button
 *  - A remove button clears the filter
 */
class FilterWidget : public QWidget {
    Q_OBJECT

public:
    explicit FilterWidget(QWidget* parent = nullptr);
    ~FilterWidget() override;

    /// Set the object whose filter is managed. Pass nullptr to clear.
    void setObject(SPObject* object);

    /// Set the desktop (needed for document access).
    void setDesktop(SPDesktop* desktop);

    /// Refresh the filter display from the current object.
    void refreshFilters();

    /// Declarative binding: tracks selection changes and drives visibility.
    void bind(Linea::Props::Binder& binder);

private Q_SLOTS:
    void onAddButtonClicked();
    void onGalleryButtonClicked();
    void onEditorButtonClicked();
    void onSearchChanged();

private:
    void populateAddPopup();
    void applyTreeItem(QTreeWidgetItem* item);
    void applyEffect(const QString& id);
    void removeCurrentFilter();
    void onFilterButtonClicked();
    bool filterTreeItem(QTreeWidgetItem* item, const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::unique_ptr<Ui::FilterWidget> _ui;

    // Add-filter popup
    PopupMenu* _addPopup = nullptr;
    PopupMenu* _galleryPopup = nullptr;
    PopupMenu* _editorPopup = nullptr;
    FilterGallery* _filterGallery = nullptr;
    FilterEditor* _filterEditor = nullptr;
    QLineEdit* _searchEdit = nullptr;
    QTreeWidget* _addList = nullptr;

    SPObject* _object = nullptr;
    SPDesktop* _desktop = nullptr;
    sigc::connection _objectDelete;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_FILTER_WIDGET_H
