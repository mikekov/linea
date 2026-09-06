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

#include "xml-tree-model.h"

#include <QMimeData>
#include <QTreeView>
#include <cassert>
#include <cstring>
#include <functional>

#include "document.h"
#include "object/sp-defs.h"
#include "object/sp-glyph.h"
#include "object/sp-item-group.h"
#include "object/sp-mask.h"
#include "object/sp-pattern.h"
#include "object/sp-root.h"
#include "object/sp-text.h"
#include "object/sp-tspan.h"
#include "xml/node-observer.h"
#include "xml/simple-node.h"

namespace Linea::UI {

/************ XmlTreeItem ************/

XmlTreeItem::XmlTreeItem(Inkscape::XML::Node* node, XmlTreeItem* parent)
    : _node(node)
    , _parent(parent) {}

XmlTreeItem::~XmlTreeItem() = default;

int XmlTreeItem::row() const {
    if (!_parent) {
        return 0;
    }
    const auto& siblings = _parent->_children;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == this) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void XmlTreeItem::addChild(std::unique_ptr<XmlTreeItem> child) {
    _children.push_back(std::move(child));
}

void XmlTreeItem::insertChild(int row, std::unique_ptr<XmlTreeItem> child) {
    if (row < 0 || static_cast<size_t>(row) > _children.size()) {
        row = static_cast<int>(_children.size());
    }
    _children.insert(_children.begin() + row, std::move(child));
}

std::unique_ptr<XmlTreeItem> XmlTreeItem::removeChild(int row) {
    if (row < 0 || static_cast<size_t>(row) >= _children.size()) {
        return nullptr;
    }
    auto child = std::move(_children[row]);
    _children.erase(_children.begin() + row);
    return child;
}

void XmlTreeItem::clearChildren() {
    _children.clear();
}

/************ NodeWatcher ************/

class NodeWatcher : public Inkscape::XML::NodeObserver {
public:
    NodeWatcher() = delete;
    NodeWatcher(XmlTreeModel* model, XmlTreeItem* item);
    ~NodeWatcher() override;

private:
    void updateRow();
    void addChild(Inkscape::XML::Node* child, Inkscape::XML::Node* prev);
    void removeChild(Inkscape::XML::Node* child);
    void moveChild(Inkscape::XML::Node* child, Inkscape::XML::Node* prev);
    XmlTreeItem* findChildItem(Inkscape::XML::Node* node) const;

    // NodeObserver interface
    void notifyChildAdded(Inkscape::XML::Node& node, Inkscape::XML::Node& child, Inkscape::XML::Node* prev) override {
        assert(_item->node() == &node);
        addChild(&child, prev);
    }

    void notifyChildRemoved(Inkscape::XML::Node& node, Inkscape::XML::Node& child, Inkscape::XML::Node*) override {
        assert(_item->node() == &node);
        removeChild(&child);
    }

    void notifyChildOrderChanged(Inkscape::XML::Node& parent, Inkscape::XML::Node& child,
                                 Inkscape::XML::Node* /* old parent */, Inkscape::XML::Node* new_prev) override {
        assert(_item->node() == &parent);
        moveChild(&child, new_prev);
    }

    void notifyContentChanged(Inkscape::XML::Node& /* node */, Inkscape::Util::ptr_shared /* old_content */,
                              Inkscape::Util::ptr_shared /* new_content */) override {
        updateRow();
    }

    void notifyAttributeChanged(Inkscape::XML::Node& node, GQuark key, Inkscape::Util::ptr_shared,
                                Inkscape::Util::ptr_shared) override {
        // Only worry about 'id' or 'inkscape::label' changes
        const auto attribute = g_quark_to_string(key);
        if (std::strcmp(attribute, "id") == 0 || std::strcmp(attribute, "inkscape:label") == 0) {
            updateRow();
        }
    }

    void notifyElementNameChanged(Inkscape::XML::Node& /* node */, GQuark /* old_code */,
                                  GQuark /* new_code */) override {
        updateRow();
    }

    XmlTreeModel* _model;
    XmlTreeItem* _item;
    std::unordered_map<Inkscape::XML::Node*, std::unique_ptr<NodeWatcher>> _childWatchers;
};

NodeWatcher::NodeWatcher(XmlTreeModel* model, XmlTreeItem* item)
    : _model(model)
    , _item(item) {
    auto* node = item->node();
    assert(node);

    // Create watchers for existing children
    for (auto* child = node->firstChild(); child != nullptr; child = child->next()) {
        auto childItem = std::make_unique<XmlTreeItem>(child, _item);
        auto* childPtr = childItem.get();
        _item->addChild(std::move(childItem));
        _childWatchers[child] = std::make_unique<NodeWatcher>(_model, childPtr);
    }

    node->addObserver(*this);
    updateRow();
}

NodeWatcher::~NodeWatcher() {
    _item->node()->removeObserver(*this);
    _childWatchers.clear();
}

void NodeWatcher::updateRow() {
    auto index = _model->indexForItem(_item);
    if (index.isValid()) {
        Q_EMIT _model->dataChanged(index, index,
                                   {Qt::DisplayRole, static_cast<int>(XmlTreeModel::PlainTextRole),
                                    static_cast<int>(XmlTreeModel::MarkupRole)});
    }
}

void NodeWatcher::addChild(Inkscape::XML::Node* child, Inkscape::XML::Node* prev) {
    // Find insertion position
    int row = 0;
    if (prev) {
        for (const auto& c : _item->children()) {
            if (c->node() == prev) {
                row++;
                break;
            }
            row++;
        }
    }

    auto parentIndex = _model->indexForItem(_item);
    _model->beginInsertRows(parentIndex, row, row);

    auto childItem = std::make_unique<XmlTreeItem>(child, _item);
    auto* childPtr = childItem.get();
    _item->insertChild(row, std::move(childItem));
    _childWatchers[child] = std::make_unique<NodeWatcher>(_model, childPtr);

    _model->endInsertRows();
}

void NodeWatcher::removeChild(Inkscape::XML::Node* child) {
    auto it = _childWatchers.find(child);
    if (it == _childWatchers.end()) {
        return;
    }

    // Find the row of this child
    int row = 0;
    const auto& children = _item->children();
    for (; static_cast<size_t>(row) < children.size(); ++row) {
        if (children[row]->node() == child) {
            break;
        }
    }

    auto parentIndex = _model->indexForItem(_item);
    _model->beginRemoveRows(parentIndex, row, row);

    _childWatchers.erase(it);
    _item->removeChild(row);

    _model->endRemoveRows();
}

void NodeWatcher::moveChild(Inkscape::XML::Node* child, Inkscape::XML::Node* prev) {
    // Find current row
    int oldRow = -1;
    const auto& children = _item->children();
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i]->node() == child) {
            oldRow = static_cast<int>(i);
            break;
        }
    }
    if (oldRow < 0) {
        return;
    }

    // Calculate new row
    int newRow = 0;
    if (prev) {
        for (const auto& c : _item->children()) {
            if (c->node() == prev) {
                newRow++;
                break;
            }
            if (c->node() != child) {
                newRow++;
            }
        }
    }

    if (oldRow == newRow) {
        return;
    }

    auto parentIndex = _model->indexForItem(_item);
    if (!_model->beginMoveRows(parentIndex, oldRow, oldRow, parentIndex, newRow)) {
        return;
    }

    // Move the child item
    auto childWatcherIt = _childWatchers.find(child);
    if (childWatcherIt == _childWatchers.end()) {
        return;
    }
    auto childWatcher = std::move(childWatcherIt->second);
    _childWatchers.erase(childWatcherIt);

    auto childItem = _item->removeChild(oldRow);
    _item->insertChild(newRow > oldRow ? newRow - 1 : newRow, std::move(childItem));
    _childWatchers[child] = std::move(childWatcher);

    _model->endMoveRows();
}

XmlTreeItem* NodeWatcher::findChildItem(Inkscape::XML::Node* node) const {
    for (const auto& child : _item->children()) {
        if (child->node() == node) {
            return child.get();
        }
    }
    return nullptr;
}

/************ XmlTreeModel ************/

XmlTreeModel::XmlTreeModel(QObject* parent)
    : QAbstractItemModel(parent) {}

XmlTreeModel::~XmlTreeModel() = default;

void XmlTreeModel::buildTree(SPDocument* document) {
    beginResetModel();

    _rootWatcher.reset();
    _rootItem.reset();
    _nodeToItem.clear();
    _document = document;

    if (!document) {
        endResetModel();
        return;
    }

    auto* root = document->getReprRoot();
    if (!root) {
        endResetModel();
        return;
    }

    _rootItem = std::make_unique<XmlTreeItem>(nullptr); // Invisible root
    auto rootChild = std::make_unique<XmlTreeItem>(root, _rootItem.get());
    auto* rootPtr = rootChild.get();
    _rootItem->addChild(std::move(rootChild));
    _rootWatcher = std::make_unique<NodeWatcher>(this, rootPtr);

    endResetModel();
}

Inkscape::XML::Node* XmlTreeModel::nodeForIndex(const QModelIndex& index) const {
    if (!index.isValid()) {
        return nullptr;
    }
    auto* item = itemForIndex(index);
    return item ? item->node() : nullptr;
}

QModelIndex XmlTreeModel::indexForNode(Inkscape::XML::Node* node) const {
    if (!_rootItem || !node) {
        return QModelIndex();
    }

    // Find the item by traversing from root
    std::function<XmlTreeItem*(XmlTreeItem*, Inkscape::XML::Node*)> findNode;
    findNode = [&](XmlTreeItem* parent, Inkscape::XML::Node* target) -> XmlTreeItem* {
        for (const auto& child : parent->children()) {
            if (child->node() == target) {
                return child.get();
            }
            auto* found = findNode(child.get(), target);
            if (found) {
                return found;
            }
        }
        return nullptr;
    };

    // Start from the first child of root (which is the document root)
    for (const auto& child : _rootItem->children()) {
        if (child->node() == node) {
            return indexForItem(child.get());
        }
        auto* found = findNode(child.get(), node);
        if (found) {
            return indexForItem(found);
        }
    }

    return QModelIndex();
}

void XmlTreeModel::selectNode(QTreeView* view, Inkscape::XML::Node* node, bool edit) {
    if (!view) {
        return;
    }

    if (!node) {
        view->selectionModel()->clear();
        return;
    }

    auto index = indexForNode(node);
    if (!index.isValid()) {
        return;
    }

    // Expand all parents
    auto parent = index.parent();
    while (parent.isValid()) {
        view->expand(parent);
        parent = parent.parent();
    }

    // Select and scroll
    view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    view->scrollTo(index, QAbstractItemView::PositionAtCenter);

    if (edit) {
        view->edit(index);
    }
}

XmlTreeItem* XmlTreeModel::itemForIndex(const QModelIndex& index) const {
    if (!index.isValid()) {
        return _rootItem.get();
    }
    return static_cast<XmlTreeItem*>(index.internalPointer());
}

QModelIndex XmlTreeModel::indexForItem(XmlTreeItem* item) const {
    if (!item || item == _rootItem.get()) {
        return QModelIndex();
    }
    return createIndex(item->row(), 0, item);
}

QModelIndex XmlTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    auto* parentItem = itemForIndex(parent);
    if (!parentItem || row < 0 || static_cast<size_t>(row) >= parentItem->children().size()) {
        return QModelIndex();
    }

    auto* childItem = parentItem->children()[row].get();
    return createIndex(row, column, childItem);
}

QModelIndex XmlTreeModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) {
        return QModelIndex();
    }

    auto* childItem = itemForIndex(child);
    auto* parentItem = childItem->parent();

    if (!parentItem || parentItem == _rootItem.get()) {
        return QModelIndex();
    }

    return indexForItem(parentItem);
}

int XmlTreeModel::rowCount(const QModelIndex& parent) const {
    auto* parentItem = itemForIndex(parent);
    if (!parentItem) {
        return 0;
    }
    return static_cast<int>(parentItem->children().size());
}

int XmlTreeModel::columnCount(const QModelIndex&) const {
    return ColumnCount;
}

QVariant XmlTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }

    auto* item = itemForIndex(index);
    auto* node = item->node();
    if (!node) {
        return QVariant();
    }

    using Inkscape::XML::NodeType;

    // Build display text
    QString plainText;
    QString markupText;

    switch (node->type()) {
        case NodeType::ELEMENT_NODE: {
            QString name = QString::fromUtf8(node->name());

            // Remove namespace "svg:" prefix
            if (name.startsWith("svg:")) {
                name = name.mid(4);
            }

            plainText = "<" + name;

            // Add id and label attributes
            if (auto* id = node->attribute("id")) {
                plainText += " id=\"" + QString::fromUtf8(id) + "\"";
            }
            if (auto* label = node->attribute("inkscape:label")) {
                plainText += " inkscape:label=\"" + QString::fromUtf8(label) + "\"";
            }
            plainText += ">";

            // For markup, we would use XMLFormatter, but for now use plain text
            markupText = plainText;
            break;
        }
        case NodeType::TEXT_NODE:
        case NodeType::COMMENT_NODE:
        case NodeType::PI_NODE: {
            QString content;
            if (auto* simpleNode = dynamic_cast<Inkscape::XML::SimpleNode*>(node)) {
                if (simpleNode->content()) {
                    content = QString::fromUtf8(simpleNode->content());
                }
            }

            QString start, end;
            switch (node->type()) {
                case NodeType::TEXT_NODE:
                    start = end = "\"";
                    break;
                case NodeType::COMMENT_NODE:
                    start = "<!--";
                    end = "-->";
                    break;
                case NodeType::PI_NODE:
                    start = "<?";
                    end = "?>";
                    break;
                default:
                    break;
            }

            plainText = start + content + end;
            markupText = plainText;
            break;
        }
        case NodeType::DOCUMENT_NODE:
            return QVariant();
        default:
            return QVariant();
    }

    switch (role) {
        case Qt::DisplayRole:
            return plainText;
        case PlainTextRole:
            return plainText;
        case MarkupRole:
            return markupText;
        case NodeRole:
            return QVariant::fromValue(node);
        default:
            return QVariant();
    }
}

Qt::ItemFlags XmlTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto* item = itemForIndex(index);
    auto* node = item->node();
    if (!node) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // Only allow dragging for non-root, non-namedview, non-defs nodes
    if (index.parent().isValid()) {
        bool canDrag = true;
        static const GQuark CODE_sodipodi_namedview = g_quark_from_static_string("sodipodi:namedview");
        static const GQuark CODE_svg_defs = g_quark_from_static_string("svg:defs");

        if (node->code() == CODE_sodipodi_namedview || node->code() == CODE_svg_defs) {
            canDrag = false;
        }

        if (canDrag) {
            f |= Qt::ItemIsDragEnabled;
        }
    }

    f |= Qt::ItemIsDropEnabled;
    return f;
}

Qt::DropActions XmlTreeModel::supportedDropActions() const {
    return Qt::MoveAction;
}

Qt::DropActions XmlTreeModel::supportedDragActions() const {
    return Qt::MoveAction;
}

QStringList XmlTreeModel::mimeTypes() const {
    return QStringList() << "application/x-inkscape-xmlnode";
}

QMimeData* XmlTreeModel::mimeData(const QModelIndexList& indexes) const {
    if (indexes.isEmpty()) {
        return nullptr;
    }

    auto* mimeData = new QMimeData();
    auto* node = nodeForIndex(indexes.first());
    if (node) {
        mimeData->setData("application/x-inkscape-xmlnode",
                          QByteArray::fromRawData(reinterpret_cast<const char*>(&node), sizeof(node)));
    }
    return mimeData;
}

bool XmlTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int /*row*/, int /*column*/,
                                const QModelIndex& parent) {
    if (action != Qt::MoveAction) {
        return false;
    }

    if (!data->hasFormat("application/x-inkscape-xmlnode")) {
        return false;
    }

    QByteArray encoded = data->data("application/x-inkscape-xmlnode");
    if (encoded.size() != sizeof(Inkscape::XML::Node*)) {
        return false;
    }

    auto* draggedNode = *reinterpret_cast<Inkscape::XML::Node**>(encoded.data());
    if (!draggedNode) {
        return false;
    }

    auto* dropItem = itemForIndex(parent);
    auto* dropNode = dropItem->node();
    if (!dropNode) {
        return false;
    }

    // Only drop into element nodes
    if (dropNode->type() != Inkscape::XML::NodeType::ELEMENT_NODE) {
        return false;
    }

    // Check if target is a container
    auto* document = _document;
    if (!document) {
        return false;
    }

    auto* item = document->getObjectByRepr(dropNode);
    if (!item) {
        return false;
    }

    // Check if it's a valid container type
    bool isContainer = is<SPDefs>(item) || is<SPGlyph>(item) || is<SPGroup>(item) || is<SPMask>(item) ||
                       is<SPPattern>(item) || is<SPTSpan>(item) || is<SPText>(item);

    if (!isContainer) {
        return false;
    }

    // Don't drop onto self
    if (draggedNode == dropNode) {
        return false;
    }

    // Perform the move
    auto* draggedParent = draggedNode->parent();
    if (!draggedParent) {
        return false;
    }

    draggedParent->removeChild(draggedNode);
    dropNode->addChild(draggedNode, nullptr);

    return true;
}

QVariant XmlTreeModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const {
    return QVariant();
}

} // namespace Linea::UI
