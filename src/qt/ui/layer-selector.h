// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * LayerSelector — A widget for selecting the current layer.
 *
 * A push button showing the current layer name. Clicking opens a popup
 * with a filter box and a tree view of the document's layer hierarchy.
 * The widget is driven entirely through setters/signals and does not
 * talk to LayerManager directly.
 */

#ifndef LINEA_UI_LAYER_SELECTOR_H
#define LINEA_UI_LAYER_SELECTOR_H

#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QPushButton;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
QT_END_NAMESPACE

class SPObject;

namespace Linea::UI {

class PopupMenu;
class ElidingLabel;

/// Populate a QTreeWidget with the layer hierarchy under currentRoot.
/// Walks SPObject children recursively, filtering for layers (SP_IS_LAYER).
/// Selects currentLayer and expands its ancestors. Each item stores its
/// SPObject* in Qt::UserRole.
void populateLayerTree(QTreeWidget* tree, SPObject* currentRoot, SPObject* currentLayer);


class LayerSelector : public QWidget {
    Q_OBJECT

public:
    explicit LayerSelector(QWidget* parent = nullptr);
    ~LayerSelector() override;

    /// Set the currently selected layer by id. Updates the button label.
    void setCurrentLayer(const QString& id);

Q_SIGNALS:
    /// Emitted when the user picks a layer in the popup.
    void layerSelected(const QString& id);

    /// Emitted when the popup is about to be shown. The receiver should
    /// populate the given QTreeWidget with the current layer hierarchy
    /// (e.g. via populateLayerTree).
    void populateLayers(QTreeWidget* tree);

private Q_SLOTS:
    void onButtonClicked();
    void onSearchChanged();
    void onItemClicked(QTreeWidgetItem* item);

private:
    void buildPopup();
    void filterTree(const QString& text);
    bool filterItem(QTreeWidgetItem* item, const QString& text);
    QTreeWidgetItem* findItemById(const QString& id) const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    QPushButton* _button = nullptr;
    ElidingLabel* _label = nullptr;
    PopupMenu* _popup = nullptr;
    QLineEdit* _searchEdit = nullptr;
    QTreeWidget* _tree = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_LAYER_SELECTOR_H
