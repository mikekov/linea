// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A Qt widget showing the abstracted object tree.
 */

#ifndef LINEA_UI_OBJECTTREEVIEW_H
#define LINEA_UI_OBJECTTREEVIEW_H

#include <QTreeView>
#include <memory>
#include <vector>
#include "object/object-set.h"
#include "ui/operation-blocker.h"

QT_BEGIN_NAMESPACE
class QStyledItemDelegate;
QT_END_NAMESPACE

class SPDocument;
class SPDesktop;
class SPObject;
class SPItem;

namespace Linea::UI {

enum class VirtualNodeType;
class ObjectTreeModel;

/**
 * A QTreeView widget for displaying an abstracted view of the SVG document.
 *
 * This widget provides a hierarchical view showing:
 * - Virtual "Document properties" node with abstracted children (About, Canvas, Display)
 * - Layers and groups
 * - Visible elements (excluding defs, metadata, namedview, etc.)
 *
 * Features:
 * - Lazy loading for performance with large documents
 * - Selection synchronization with the document
 * - Drag and drop for reordering
 */
class ObjectTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit ObjectTreeView(QWidget* parent = nullptr);
    ~ObjectTreeView() override;

    // Prevent copying
    ObjectTreeView(const ObjectTreeView&) = delete;
    ObjectTreeView& operator = (const ObjectTreeView&) = delete;

    // for context menu
    void setDesktop(SPDesktop* desktop);

    // Build the tree from a document
    void buildTree(SPDocument* document);

    // Get the selected object
    SPObject* selectedObject() const;
    SPItem* selectedItem() const;
    VirtualNodeType selectedVirtualNode() const;

    // Select object(s) in the tree
    void selectObject(SPObject* object, bool edit = false);
    void selectObjects(const Inkscape::RandomAccessIndex& objects);

    // Select object(s) and emit the selection signal so listeners (properties
    // panel, desktop selection) are driven by the tree. Used after buildTree
    // to restore the tree's selection from the desktop and let the tree drive
    // the panel, rather than bypassing it.
    void restoreSelection(const Inkscape::RandomAccessIndex& objects);

    // Select a virtual node (DocumentProps, Pages, Guides, etc.) and emit the
    // signal so the properties panel refreshes through the normal tree → panel
    // path. Used after buildTree to restore a saved virtual node selection.
    void restoreVirtualNode(VirtualNodeType type);

    // Access the model
    ObjectTreeModel* objectModel() const;

    // Filtering options
    void setShowLayersOnly(bool layersOnly);
    bool showLayersOnly() const;

    void setFilterText(const QString& text);
    QString filterText() const;

    // Update the current layer for bold rendering
    void setCurrentLayer(SPObject* layer);

    /// Collapse all nodes, then expand only the ancestors of \a layer so it
    /// is visible. Marks collapsed groups via the model's itemCollapsed.
    void collapseAllExcept(SPObject* layer);

Q_SIGNALS:
    void objectsSelected(std::vector<SPObject*> objects);
    void virtualNodeSelected(Linea::UI::VirtualNodeType type);

    // Visibility/lock toggles are requested by the user; the client handles the
    // actual change (actions, undo, etc.) rather than the model mutating state.
    void objectVisibilityRequested(SPObject* object, bool hidden);
    void objectLockRequested(SPObject* object, bool lock);
    void objectLabelRequested(SPObject* object, const QString& label);
    void virtualNodeVisibilityRequested(Linea::UI::VirtualNodeType type, bool hidden);
    void virtualNodeLockRequested(Linea::UI::VirtualNodeType type, bool lock);

protected:
    // Mouse interaction
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    // Drag and drop
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    // Selection handling
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;

    // Expand/collapse handling for lazy loading
    void drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const override;

private:
    void onExpanded(const QModelIndex& index);
    void onCollapsed(const QModelIndex& index);
    void emitSelectionSignals();

    SPDesktop* _desktop = nullptr;
    ObjectTreeModel* _model = nullptr;
    std::unique_ptr<QStyledItemDelegate> _delegate;
    OperationBlocker _selectingProgrammatically;

    // Qt's QItemSelectionModel auto-adjusts the current index/selection
    // whenever rows are inserted, removed, or moved (e.g. clearing the
    // selection of a row that just got removed). That adjustment fires
    // selectionChanged() synchronously, which selectionChanged() would
    // otherwise forward straight into the desktop's Selection — reentering
    // document-mutating code while a document edit (e.g. a boolean op or
    // the eraser tool, which can add/remove several sibling nodes
    // back-to-back before it's done) is still in progress. Block that
    // forwarding for the duration of any model structural change.
    //
    // A plain counter is used here (rather than OperationBlocker) because
    // the "begin" and "end" sides of a structural change arrive as two
    // separate signal-handler invocations
    int _modelMutationDepth = 0;

    friend class ObjectTreeDelegate;
};

} // namespace Linea::UI

#endif // LINEA_UI_OBJECTTREEVIEW_H
