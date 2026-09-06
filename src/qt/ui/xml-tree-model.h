// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A Qt model for the XML tree.
 *
 * Authors:
 *   Mike Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_XMLTREEMODEL_H
#define LINEA_UI_XMLTREEMODEL_H

#include <QAbstractItemModel>
#include <memory>
#include <unordered_map>
#include <vector>

QT_BEGIN_NAMESPACE
class QMimeData;
class QTreeView;
QT_END_NAMESPACE

class SPDocument;

namespace Inkscape {
namespace XML {
class Node;
class NodeObserver;
} // namespace XML
} // namespace Inkscape

namespace Linea::UI {

class NodeWatcher;

/**
 * Tree item representing a node in the XML tree.
 */
class XmlTreeItem {
public:
    explicit XmlTreeItem(Inkscape::XML::Node* node, XmlTreeItem* parent = nullptr);
    ~XmlTreeItem();

    // Prevent copying
    XmlTreeItem(const XmlTreeItem&) = delete;
    XmlTreeItem& operator=(const XmlTreeItem&) = delete;

    // Allow moving
    XmlTreeItem(XmlTreeItem&&) = default;
    XmlTreeItem& operator=(XmlTreeItem&&) = default;

    Inkscape::XML::Node* node() const { return _node; }
    XmlTreeItem* parent() const { return _parent; }
    const std::vector<std::unique_ptr<XmlTreeItem>>& children() const { return _children; }

    int row() const;
    void addChild(std::unique_ptr<XmlTreeItem> child);
    void insertChild(int row, std::unique_ptr<XmlTreeItem> child);
    std::unique_ptr<XmlTreeItem> removeChild(int row);
    void clearChildren();

private:
    Inkscape::XML::Node* _node;
    XmlTreeItem* _parent;
    std::vector<std::unique_ptr<XmlTreeItem>> _children;
};

/**
 * Qt model for the XML tree.
 *
 * Provides a hierarchical model of the XML document structure
 * for display in a QTreeView. Observes XML changes and updates
 * the model accordingly.
 */
class XmlTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column { ColumnNode = 0, ColumnCount };

    enum Role {
        NodeRole = Qt::UserRole + 1, // Returns XML::Node* for the item
        PlainTextRole,               // Returns plain text without markup
        MarkupRole                   // Returns markup text with syntax highlighting
    };

    explicit XmlTreeModel(QObject* parent = nullptr);
    ~XmlTreeModel() override;

    // Prevent copying
    XmlTreeModel(const XmlTreeModel&) = delete;
    XmlTreeModel& operator=(const XmlTreeModel&) = delete;

    // Build the tree from a document
    void buildTree(SPDocument* document);

    // Get the XML node for a model index
    Inkscape::XML::Node* nodeForIndex(const QModelIndex& index) const;

    // Find the model index for an XML node
    QModelIndex indexForNode(Inkscape::XML::Node* node) const;

    // Select a node in the tree (expand path, scroll to, select)
    void selectNode(QTreeView* view, Inkscape::XML::Node* node, bool edit = false);

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
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

private:
    friend class NodeWatcher;

    XmlTreeItem* itemForIndex(const QModelIndex& index) const;
    QModelIndex indexForItem(XmlTreeItem* item) const;

    SPDocument* _document = nullptr;
    std::unique_ptr<XmlTreeItem> _rootItem;
    std::unique_ptr<NodeWatcher> _rootWatcher;
    std::unordered_map<Inkscape::XML::Node*, XmlTreeItem*> _nodeToItem;
};

} // namespace Linea::UI

#endif // LINEA_UI_XMLTREEMODEL_H
