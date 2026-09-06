// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Qt model for the abstracted object tree.
 */

#ifndef LINEA_UI_OBJECTTREEMODEL_H
#define LINEA_UI_OBJECTTREEMODEL_H

#include <QAbstractItemModel>
#include <memory>
#include <unordered_map>
#include <vector>

#include "object/object-set.h"
#include "qt/util/virtual-node-type.h"

QT_BEGIN_NAMESPACE
class QMimeData;
class QTreeView;
QT_END_NAMESPACE

class SPDocument;
class SPObject;
class SPItem;
class SPNamedView;

namespace Inkscape {
namespace XML {
class Node;
class NodeObserver;
} // namespace XML
} // namespace Inkscape

namespace Linea::UI {

class ObjectNodeWatcher;
class NamedViewWatcher;

/**
 * Tree item representing a node in the object tree.
 * Can represent either an XML node (with a lazily-resolved SPObject)
 * or a virtual node.
 */
class ObjectTreeItem {
public:
    // Constructor for XML node-backed items
    explicit ObjectTreeItem(SPDocument* doc, SPObject* object, ObjectTreeItem* parent = nullptr);
    // Constructor for virtual nodes
    explicit ObjectTreeItem(VirtualNodeType virtualType, const QString& label, ObjectTreeItem* parent = nullptr);
    ~ObjectTreeItem();

    // Prevent copying/moving; the tree is built from unique_ptrs
    ObjectTreeItem(const ObjectTreeItem&) = delete;
    ObjectTreeItem& operator=(const ObjectTreeItem&) = delete;
    ObjectTreeItem(ObjectTreeItem&&) = delete;
    ObjectTreeItem& operator=(ObjectTreeItem&&) = delete;

    // Item properties
    Inkscape::XML::Node* node() const { return _node; }
    SPObject* object() const;
    ObjectTreeItem* parent() const { return _parent; }
    const std::vector<std::unique_ptr<ObjectTreeItem>>& children() const { return _children; }

    // Virtual node properties
    bool isVirtual() const { return _virtualType != VirtualNodeType::None; }
    VirtualNodeType virtualType() const { return _virtualType; }
    QString label() const { return _label; }

    // Tree operations
    int row() const;
    void addChild(std::unique_ptr<ObjectTreeItem> child);
    void insertChild(int row, std::unique_ptr<ObjectTreeItem> child);
    std::unique_ptr<ObjectTreeItem> removeChild(int row);
    void clearChildren();
    ObjectTreeItem* findChild(Inkscape::XML::Node* node) const;

    // Lazy loading support
    bool hasDummyChild() const { return _hasDummyChild; }
    void setHasDummyChild(bool hasDummy) { _hasDummyChild = hasDummy; }

    // Back-pointer to the watcher observing this item, if any.
    // Set by ObjectNodeWatcher constructor, cleared by destructor.
    ObjectNodeWatcher* watcher() const { return _watcher; }
    void setWatcher(ObjectNodeWatcher* w) { _watcher = w; }

private:
    SPDocument* _document = nullptr;      // Null for virtual nodes
    Inkscape::XML::Node* _node = nullptr; // XML node; anchored for lifetime of item
    ObjectTreeItem* _parent;
    std::vector<std::unique_ptr<ObjectTreeItem>> _children;
    VirtualNodeType _virtualType;
    QString _label;
    bool _hasDummyChild = false;
    ObjectNodeWatcher* _watcher = nullptr;
};

/**
 * Qt model for the abstracted object tree.
 *
 * Provides a hierarchical model showing:
 * - Virtual "Document properties" node with abstracted children
 * - Layers and groups
 * - Visible elements (excluding defs, metadata, etc.)
 *
 * Observes document changes and updates the model accordingly.
 */
class ObjectTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column { ColumnLabel = 0, ColumnVisible, ColumnLocked, ColumnCount };

    enum Role {
        ObjectRole = Qt::UserRole + 1, // Returns SPObject* for the item
        VirtualTypeRole,               // Returns VirtualNodeType
        PlainTextRole,                 // Returns plain text label
        IsLayerRole,                   // Returns true if item is a layer
        IsHiddenRole,                  // Returns true if item is hidden
        IsLockedRole                   // Returns true if item is locked
    };

    explicit ObjectTreeModel(QObject* parent = nullptr);
    ~ObjectTreeModel() override;

    // Prevent copying
    ObjectTreeModel(const ObjectTreeModel&) = delete;
    ObjectTreeModel& operator=(const ObjectTreeModel&) = delete;

    // Build the tree from a document
    void buildTree(SPDocument* document);

    // Get the SPObject for a model index
    SPObject* objectForIndex(const QModelIndex& index) const;

    // Get the tree item for a model index
    ObjectTreeItem* itemForIndex(const QModelIndex& index) const;

    // Find the model index for an SPObject
    QModelIndex indexForObject(SPObject* object) const;

    // Find the model index for a virtual node by type
    QModelIndex indexForVirtualType(VirtualNodeType type) const;

    /// Whether the virtual Document node is currently expanded.
    bool isDocumentExpanded() const { return _documentExpanded; }

    // Select object(s) in the tree (expand path, scroll to, select)
    void selectObject(QTreeView* view, SPObject* object, bool edit = false);
    void selectObjects(QTreeView* view, const Inkscape::RandomAccessIndex& objects);

    /// Expand ancestors of \a object from root down, materializing collapsed
    /// groups via itemExpanded so the target's index becomes valid.
    void expandToReveal(QTreeView* view, SPObject* object);

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Drag and drop support
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    // Header data
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Filtering options
    void setShowLayersOnly(bool layersOnly);
    bool showLayersOnly() const { return _layersOnly; }

    void setFilterText(const QString& text);
    QString filterText() const { return _filterText; }

    // Expand handling for lazy loading
    void itemExpanded(const QModelIndex& index);
    void itemCollapsed(const QModelIndex& index);

    // Restore expansion state from SPItem::isExpanded() after a tree rebuild
    void restoreExpanded(QTreeView* view);

    // Check if an object should be shown in the tree
    static bool shouldShowObject(SPObject* object);
    static bool isVisibleItemType(SPObject* object);

private:
    friend class ObjectNodeWatcher;
    friend class NamedViewWatcher;

    QModelIndex indexForItem(ObjectTreeItem* item) const;

    void restoreExpandedRecursive(QTreeView* view, ObjectTreeItem* item);

    // Build virtual document properties node
    void buildVirtualDocumentProps();

    // Add visible children of an object to the tree
    void addVisibleChildren(ObjectTreeItem* parentItem, SPObject* parentObject, bool useDummy = false);
    int visibleChildCount(SPObject* parentObject) const;

    // Find if an object passes the current filters (layers-only + text search).
    // Text search is recursive: an item passes if it or any descendant matches.
    bool passesFilters(SPObject* object) const;
    bool passesFiltersRecursive(SPObject* object) const;
    bool hasVisibleDescendants(SPObject* object) const;

    // Find SPNamedView from document
    SPNamedView* findNamedView() const;

    SPDocument* _document = nullptr;
    std::unique_ptr<ObjectTreeItem> _rootItem;
    std::unique_ptr<ObjectNodeWatcher> _rootWatcher;
    std::unique_ptr<NamedViewWatcher> _namedViewWatcher;
    std::unordered_map<Inkscape::XML::Node*, ObjectTreeItem*> _nodeToItem;

    // Whether the Document virtual node is expanded, persisted across rebuilds.
    bool _documentExpanded = true;

    // When true, itemExpanded/itemCollapsed don't set SPItem::isExpanded,
    // so filter-driven expansions don't persist as user intent.
    bool _restoringExpanded = false;

    bool _layersOnly = false;
    QString _filterText;
};

} // namespace Linea::UI

#endif // LINEA_UI_OBJECTTREEMODEL_H
