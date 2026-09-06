// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Qt model for the abstracted object tree.
 */

#include "object-tree-model.h"

#include <QDebug>
#include <QIcon>
#include <QItemSelectionModel>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QTreeView>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>

#include "document.h"
#include "object/sp-defs.h"
#include "object/sp-ellipse.h"
#include "object/sp-flowtext.h"
#include "object/sp-grid.h"
#include "object/sp-guide.h"
#include "object/sp-image.h"
#include "object/sp-item-group.h"
#include "object/sp-line.h"
#include "object/sp-metadata.h"
#include "object/sp-namedview.h"
#include "object/sp-path.h"
#include "object/sp-polygon.h"
#include "object/sp-polyline.h"
#include "object/sp-rect.h"
#include "object/sp-root.h"
#include "object/sp-spiral.h"
#include "object/sp-star.h"
#include "object/sp-text.h"
#include "object/sp-title.h"
#include "object/sp-use.h"
#include "preferences.h"
#include "ui/util.h"
#include "util/cast.h"
#include "xml/node-observer.h"
#include "xml/simple-node.h"

namespace {

constexpr double iconOpacity = 0.50;

inline QIcon composeOverlay(const QIcon& base, unsigned clipmask, double opacity);

inline QIcon visibilityIcon(bool hidden) {
    return composeOverlay(QIcon(hidden ? ":/icons/object-hidden" : ":/icons/object-visible"), 0,
                          hidden ? 1.0 : iconOpacity);
}

inline QIcon lockIcon(bool locked) {
    return composeOverlay(QIcon(locked ? ":/icons/object-locked" : ":/icons/object-unlocked"), 0,
                          locked ? 1.0 : iconOpacity);
}

inline QIcon baseIconForType(const char* type) {
    if (!type) return {};

    static std::unordered_map<std::string, QIcon> cache;
    auto key = std::string(type);
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    const QString base = QString::fromUtf8(type);
    const std::vector<QString> candidates = {QStringLiteral("shape-") + base, base, QStringLiteral("shape-unknown")};
    QIcon icon;
    for (const auto& name : candidates) {
        QIcon candidate(QStringLiteral(":/icons/") + name);
        if (!candidate.isNull()) {
            icon = candidate;
            break;
        }
    }
    cache.emplace(std::move(key), icon);
    return icon;
}

inline QIcon composeOverlay(const QIcon& base, unsigned clipmask, double opacity) {
    if (base.isNull()) return base;
    if (clipmask == 0 && opacity == 1.0) return base;

    const int iconSize = 16;
    QPixmap basePixmap = base.pixmap(iconSize, iconSize);
    if (basePixmap.isNull()) return base;

    QPixmap combined(basePixmap.size());
    combined.setDevicePixelRatio(basePixmap.devicePixelRatio());
    combined.fill(Qt::transparent);
    QPainter painter(&combined);
    painter.setOpacity(opacity);
    painter.drawPixmap(0, 0, basePixmap);
    if (clipmask != 0) {
        QString overlayName;
        if (clipmask == 3) {
            overlayName = QStringLiteral("overlay-clipmask");
        } else if (clipmask == 1) {
            overlayName = QStringLiteral("overlay-clip");
        } else {
            overlayName = QStringLiteral("overlay-mask");
        }
        QIcon overlayIcon(QStringLiteral(":/icons/") + overlayName);
        if (!overlayIcon.isNull()) {
            QPixmap overlayPixmap = overlayIcon.pixmap(iconSize / 2, iconSize / 2);
            painter.drawPixmap(basePixmap.width() - overlayPixmap.width(), basePixmap.height() - overlayPixmap.height(),
                               overlayPixmap);
        }
    }
    painter.end();
    return QIcon(combined);
}

inline QIcon objectIcon(SPItem* item) {
    if (!item) return {};

    QIcon base = baseIconForType(item->typeName());
    const unsigned clipmask = (item->getClipObject() ? 1U : 0U) | (item->getMaskObject() ? 2U : 0U);
    return composeOverlay(base, clipmask, iconOpacity);
}

} // namespace

namespace Linea::UI {

/************ ObjectTreeItem ************/

ObjectTreeItem::ObjectTreeItem(SPDocument* doc, SPObject* object, ObjectTreeItem* parent)
    : _document(doc)
    , _parent(parent)
    , _node(object ? object->getRepr() : nullptr)
    , _virtualType(VirtualNodeType::None)
    , _label() {
    if (_node) Inkscape::GC::anchor(_node);
}

ObjectTreeItem::ObjectTreeItem(VirtualNodeType virtualType, const QString& label, ObjectTreeItem* parent)
    : _document(nullptr)
    , _parent(parent)
    , _node(nullptr)
    , _virtualType(virtualType)
    , _label(label) {}

ObjectTreeItem::~ObjectTreeItem() {
    if (_node) Inkscape::GC::release(_node);
}

SPObject* ObjectTreeItem::object() const {
    return _document && _node ? _document->getObjectByRepr(_node) : nullptr;
}

int ObjectTreeItem::row() const {
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

void ObjectTreeItem::addChild(std::unique_ptr<ObjectTreeItem> child) {
    _children.push_back(std::move(child));
}

void ObjectTreeItem::insertChild(int row, std::unique_ptr<ObjectTreeItem> child) {
    if (row < 0 || static_cast<size_t>(row) > _children.size()) {
        row = static_cast<int>(_children.size());
    }
    _children.insert(_children.begin() + row, std::move(child));
}

std::unique_ptr<ObjectTreeItem> ObjectTreeItem::removeChild(int row) {
    if (row < 0 || static_cast<size_t>(row) >= _children.size()) {
        return nullptr;
    }
    auto child = std::move(_children[row]);
    _children.erase(_children.begin() + row);
    return child;
}

void ObjectTreeItem::clearChildren() {
    _children.clear();
}

ObjectTreeItem* ObjectTreeItem::findChild(Inkscape::XML::Node* node) const {
    for (const auto& child : _children) {
        if (child->node() == node) {
            return child.get();
        }
    }
    return nullptr;
}

/************ ObjectNodeWatcher ************/

// Watches an XML node's repr and keeps the Qt model in sync.
// Child watchers are keyed by XML::Node* (not SPObject*) so they remain
// valid even when an SPObject is temporarily detached during a move.
//
// Structural notifications (child added/removed/reordered) are handled
// synchronously and immediately, the same way GTK's ObjectWatcher
// (ui/dialog/objects.cpp) handles them: each notification applies a small,
// targeted edit using only the sibling reference the notification itself
// provides (prev/new_prev), the same way ObjectWatcher::addChild/moveChild/
// notifyChildRemoved do — not a rescan of the whole parent's children.
class ObjectNodeWatcher : public Inkscape::XML::NodeObserver {
public:
    ObjectNodeWatcher() = delete;
    // Regular item watcher — observes item->node().
    ObjectNodeWatcher(ObjectTreeModel* model, ObjectTreeItem* item);
    // Root watcher — _item is the invisible _rootItem, but we observe node directly.
    ObjectNodeWatcher(ObjectTreeModel* model, ObjectTreeItem* item, Inkscape::XML::Node* node);
    ~ObjectNodeWatcher() override;

    // Find a direct child watcher by XML node.
    ObjectNodeWatcher* findChild(Inkscape::XML::Node* node) const;

    // Create child watchers for any child items that don't have one yet.
    void rebuildChildren();

    // Add/remove/reposition a single child in response to a notification.
    // Exposed (not just private notify* overrides) so notifyAttributeChanged
    // can re-run them for a single node on a groupmode (layer <-> group)
    // change, which can change whether that node passes the layers-only
    // filter.
    void insertChildNode(Inkscape::XML::Node& childNode, Inkscape::XML::Node* prev);
    void removeChildNode(Inkscape::XML::Node& childNode);

    std::unordered_map<Inkscape::XML::Node*, std::unique_ptr<ObjectNodeWatcher>> _childWatchers;

private:
    SPObject* getObject(Inkscape::XML::Node* node) const;
    void updateRow();

    // Insert a brand new child (with its own subtree/dummy/watcher) at `row`.
    void insertChildAt(int row, Inkscape::XML::Node& childNode);
    void moveChildNode(Inkscape::XML::Node& childNode, Inkscape::XML::Node* newPrev);

    // Walk `sibling`, then its XML prev-siblings, until one is found that is
    // currently a tracked (real, materialized) child of this watcher, and
    // return its current row. Returns -1 if none is tracked (i.e. `sibling`
    // and everything before it in XML order is untracked/nonexistent, so
    // the node in question belongs at the bottom of this item's children).
    // This mirrors ObjectWatcher::moveChild's "while (sibling && ...)
    // sibling = sibling->prev();" walk — bounded by how many untracked
    // siblings sit in between, not a scan of the whole children list.
    int rowOfNearestTrackedSibling(Inkscape::XML::Node* sibling) const;

    void notifyChildAdded(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node* prev) override;
    void notifyChildRemoved(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node*) override;
    void notifyChildOrderChanged(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node*,
                                 Inkscape::XML::Node* new_prev) override;
    void notifyContentChanged(Inkscape::XML::Node&, Inkscape::Util::ptr_shared, Inkscape::Util::ptr_shared) override;
    void notifyAttributeChanged(Inkscape::XML::Node& node, GQuark key, Inkscape::Util::ptr_shared,
                                Inkscape::Util::ptr_shared) override;
    void notifyElementNameChanged(Inkscape::XML::Node&, GQuark, GQuark) override;

    ObjectTreeModel* _model;
    ObjectTreeItem* _item;
    Inkscape::XML::Node* _node = nullptr;
};

SPObject* ObjectNodeWatcher::getObject(Inkscape::XML::Node* node) const {
    if (!node || !_model->_document) return nullptr;
    return _model->_document->getObjectByRepr(node);
}

ObjectNodeWatcher::ObjectNodeWatcher(ObjectTreeModel* model, ObjectTreeItem* item)
    : _model(model)
    , _item(item) {
    _item->setWatcher(this);
    if (item->isVirtual()) return;
    _node = item->node();
    if (!_node) return;
    rebuildChildren();
    _node->addObserver(*this);
    updateRow();
}

ObjectNodeWatcher::ObjectNodeWatcher(ObjectTreeModel* model, ObjectTreeItem* item, Inkscape::XML::Node* node)
    : _model(model)
    , _item(item)
    , _node(node) {
    _item->setWatcher(this);
    if (!_node) return;
    rebuildChildren();
    _node->addObserver(*this);
}

ObjectNodeWatcher::~ObjectNodeWatcher() {
    if (_node) _node->removeObserver(*this);
    // Erase this item's entry from _nodeToItem. Descendant entries are
    // erased recursively as _childWatchers is cleared (each child watcher's
    // destructor runs this same cleanup).
    if (_item && _item->node()) {
        _model->_nodeToItem.erase(_item->node());
    }
    if (_item) _item->setWatcher(nullptr);
    _childWatchers.clear();
}

ObjectNodeWatcher* ObjectNodeWatcher::findChild(Inkscape::XML::Node* node) const {
    auto it = _childWatchers.find(node);
    return it != _childWatchers.end() ? it->second.get() : nullptr;
}

void ObjectNodeWatcher::rebuildChildren() {
    // qDebug() << "[rebuildChildren] node:" << _node->name()
    //          << "id:" << (_node->attribute("id") ? _node->attribute("id") : "(none)")
    //          << "item:" << _item;
    for (auto child = _node->firstChild(); child; child = child->next()) {
        if (_childWatchers.count(child)) continue;
        auto childItem = _item->findChild(child);
        // qDebug() << "  child repr:" << child->name()
        //          << "id:" << (child->attribute("id") ? child->attribute("id") : "(none)")
        //          << "childItem:" << childItem;
        if (!childItem) continue;
        _childWatchers[child] = std::make_unique<ObjectNodeWatcher>(_model, childItem);
    }
}

void ObjectNodeWatcher::updateRow() {
    auto index = _model->indexForItem(_item);
    if (index.isValid()) {
        auto lastCol = _model->index(index.row(), ObjectTreeModel::ColumnCount - 1, index.parent());
        Q_EMIT _model->dataChanged(
            index, lastCol,
            {Qt::DisplayRole, Qt::DecorationRole, static_cast<int>(ObjectTreeModel::PlainTextRole),
             static_cast<int>(ObjectTreeModel::IsHiddenRole), static_cast<int>(ObjectTreeModel::IsLockedRole)});
    }
}

void ObjectNodeWatcher::insertChildAt(int row, Inkscape::XML::Node& childNode) {
    auto child = getObject(&childNode);

    auto parentIndex = _model->indexForItem(_item);
    _model->beginInsertRows(parentIndex, row, row);

    auto childItem = std::make_unique<ObjectTreeItem>(_model->_document, child, _item);
    auto childPtr = childItem.get();
    _item->insertChild(row, std::move(childItem));
    _model->_nodeToItem[&childNode] = childPtr;

    bool useDummy = false;
    if (auto group = cast<SPGroup>(child)) {
        for (auto& gc : child->children) {
            if (ObjectTreeModel::shouldShowObject(&gc) && _model->passesFiltersRecursive(&gc)) {
                // Disable dummy children when text filtering
                useDummy = !group->isExpanded() && _model->_filterText.isEmpty();
                break;
            }
        }
    }

    if (useDummy) {
        childPtr->setHasDummyChild(true);
    } else {
        _model->addVisibleChildren(childPtr, child, false);
    }

    _childWatchers[&childNode] = std::make_unique<ObjectNodeWatcher>(_model, childPtr);
    _model->endInsertRows();
}

int ObjectNodeWatcher::rowOfNearestTrackedSibling(Inkscape::XML::Node* sibling) const {
    for (; sibling; sibling = sibling->prev()) {
        if (auto item = _item->findChild(sibling)) return item->row();
    }
    return -1;
}

// Add a single new child in response to notifyChildAdded, exactly like
// ObjectWatcher::addChild + moveChild: no rescan of the other children,
// just a row computed from the `prev` sibling the notification gave us
// (walking back through untracked/filtered-out siblings if needed).
void ObjectNodeWatcher::insertChildNode(Inkscape::XML::Node& childNode, Inkscape::XML::Node* prev) {
    auto child = getObject(&childNode);
    if (!child || !ObjectTreeModel::shouldShowObject(child)) return;
    if (!_model->passesFiltersRecursive(child)) return;
    if (_item->findChild(&childNode)) return; // already present (defensive)

    // Already lazily collapsed behind a dummy placeholder: stay lazy, the
    // real children get materialized in full when the group is expanded
    // (ObjectTreeModel::itemExpanded).
    if (_item->hasDummyChild()) return;

    // First child appearing under a currently childless, collapsed group:
    // show a dummy placeholder instead of materializing a real row, same as
    // ObjectWatcher::addChild's default dummy=true behavior.
    if (_item->children().empty()) {
        if (auto group = cast<SPGroup>(getObject(_node));
            group && !group->isExpanded() && _model->_filterText.isEmpty()) {
            auto parentIndex = _model->indexForItem(_item);
            _model->beginInsertRows(parentIndex, 0, 0);
            _item->setHasDummyChild(true);
            _model->endInsertRows();
            return;
        }
    }

    int row = rowOfNearestTrackedSibling(prev);
    if (row < 0) row = static_cast<int>(_item->children().size());
    insertChildAt(row, childNode);
}

// Remove a single child in response to notifyChildRemoved, mirroring
// ObjectWatcher::notifyChildRemoved: if it was tracked, just erase its row;
// otherwise, if the group is now completely empty, drop the dummy
// placeholder so it no longer looks expandable.
void ObjectNodeWatcher::removeChildNode(Inkscape::XML::Node& childNode) {
    auto item = _item->findChild(&childNode);
    if (!item) {
        if (_item->hasDummyChild() && !_node->firstChild()) {
            auto parentIndex = _model->indexForItem(_item);
            _model->beginRemoveRows(parentIndex, 0, 0);
            _item->setHasDummyChild(false);
            _model->endRemoveRows();
        }
        return;
    }

    int row = item->row();

    // Destroying the child watcher (if any) recursively cleans up
    // _nodeToItem for it and all of its descendants.
    _childWatchers.erase(&childNode);

    auto parentIndex = _model->indexForItem(_item);
    _model->beginRemoveRows(parentIndex, row, row);
    _item->removeChild(row);
    _model->endRemoveRows();
}

// Reposition a single already-tracked child in response to
// notifyChildOrderChanged, mirroring ObjectWatcher::moveChild.
void ObjectNodeWatcher::moveChildNode(Inkscape::XML::Node& childNode, Inkscape::XML::Node* newPrev) {
    auto childItem = _item->findChild(&childNode);
    if (!childItem) return; // not shown/tracked, nothing to reposition

    int oldRow = childItem->row();
    int size = static_cast<int>(_item->children().size());

    // rowOfNearestTrackedSibling() reports positions in the *current* list,
    // which still includes childItem itself; if the sibling it found sits
    // after childItem's current position, its row will shift down by one
    // once childItem is removed below, so account for that here.
    int rawRow = rowOfNearestTrackedSibling(newPrev);
    int newRow = rawRow < 0 ? size - 1 : (rawRow > oldRow ? rawRow - 1 : rawRow);

    if (newRow == oldRow) return;

    auto parentIndex = _model->indexForItem(_item);
    int destination = newRow > oldRow ? newRow + 1 : newRow;
    if (!_model->beginMoveRows(parentIndex, oldRow, oldRow, parentIndex, destination)) return;

    // _childWatchers is keyed by XML::Node*, not position, so moving the
    // item within _item's children doesn't require touching it.
    auto item = _item->removeChild(oldRow);
    _item->insertChild(newRow, std::move(item));

    _model->endMoveRows();
}

void ObjectNodeWatcher::notifyChildAdded(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node* prev) {
    insertChildNode(child, prev);
}

void ObjectNodeWatcher::notifyChildRemoved(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node*) {
    removeChildNode(child);
}

void ObjectNodeWatcher::notifyChildOrderChanged(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node*,
                                                Inkscape::XML::Node* new_prev) {
    moveChildNode(child, new_prev);
}

void ObjectNodeWatcher::notifyContentChanged(Inkscape::XML::Node&, Inkscape::Util::ptr_shared,
                                             Inkscape::Util::ptr_shared) {
    updateRow();
}

void ObjectNodeWatcher::notifyAttributeChanged(Inkscape::XML::Node& node, GQuark key, Inkscape::Util::ptr_shared,
                                               Inkscape::Util::ptr_shared) {
    static const GQuark q_id = g_quark_from_static_string("id");
    static const GQuark q_label = g_quark_from_static_string("inkscape:label");
    static const GQuark q_style = g_quark_from_static_string("style");
    static const GQuark q_display = g_quark_from_static_string("display");
    static const GQuark q_insens = g_quark_from_static_string("sodipodi:insensitive");
    static const GQuark q_grpmode = g_quark_from_static_string("inkscape:groupmode");

    if (key == q_id || key == q_label || key == q_style || key == q_display || key == q_insens) {
        updateRow();
    }
    if (key == q_grpmode) {
        // Group mode change (layer ↔ group) can change whether this node
        // passes the layers-only filter. Ask the parent watcher to drop and
        // re-add just this one child against the new state (same pattern
        // used elsewhere for a single node's visibility changing).
        if (auto parentItem = _item->parent()) {
            if (auto parentWatcher = parentItem->watcher()) {
                parentWatcher->removeChildNode(node);
                parentWatcher->insertChildNode(node, node.prev());
            }
        }
    }
}

void ObjectNodeWatcher::notifyElementNameChanged(Inkscape::XML::Node&, GQuark, GQuark) {
    updateRow();
}

/************ NamedViewWatcher ************/

// Watches the sp-namedview repr and keeps virtual node rows up to date.
// All virtual nodes that reflect namedview state are refreshed through here.
class NamedViewWatcher : public Inkscape::XML::NodeObserver {
public:
    NamedViewWatcher() = delete;
    NamedViewWatcher(ObjectTreeModel* model, Inkscape::XML::Node* node);
    ~NamedViewWatcher() override;

private:
    void emitVirtualDataChanged(VirtualNodeType type);

    void notifyAttributeChanged(Inkscape::XML::Node&, GQuark key, Inkscape::Util::ptr_shared,
                                Inkscape::Util::ptr_shared) override;

    ObjectTreeModel* _model;
    Inkscape::XML::Node* _node;
};

NamedViewWatcher::NamedViewWatcher(ObjectTreeModel* model, Inkscape::XML::Node* node)
    : _model(model)
    , _node(node) {
    if (_node) _node->addObserver(*this);
}

NamedViewWatcher::~NamedViewWatcher() {
    if (_node) _node->removeObserver(*this);
}

void NamedViewWatcher::emitVirtualDataChanged(VirtualNodeType type) {
    auto index = _model->indexForVirtualType(type);
    if (!index.isValid()) return;
    auto lastCol = _model->index(index.row(), ObjectTreeModel::ColumnCount - 1, index.parent());
    Q_EMIT _model->dataChanged(index, lastCol,
                               {Qt::DecorationRole, static_cast<int>(ObjectTreeModel::IsHiddenRole),
                                static_cast<int>(ObjectTreeModel::IsLockedRole)});
}

void NamedViewWatcher::notifyAttributeChanged(Inkscape::XML::Node&, GQuark key, Inkscape::Util::ptr_shared,
                                              Inkscape::Util::ptr_shared) {
    static const GQuark q_showguides = g_quark_from_static_string("showguides");
    static const GQuark q_lockguides = g_quark_from_static_string("inkscape:lockguides");
    static const GQuark q_showgrid = g_quark_from_static_string("showgrid");

    if (key == q_showguides || key == q_lockguides) {
        emitVirtualDataChanged(VirtualNodeType::Guides);
    } else if (key == q_showgrid) {
        emitVirtualDataChanged(VirtualNodeType::Grids);
    }
}

/************ ObjectTreeModel ************/

ObjectTreeModel::ObjectTreeModel(QObject* parent)
    : QAbstractItemModel(parent) {}

ObjectTreeModel::~ObjectTreeModel() = default;

void ObjectTreeModel::buildTree(SPDocument* document) {
    beginResetModel();

    _namedViewWatcher.reset();
    _rootWatcher.reset();
    _rootItem.reset();
    _nodeToItem.clear();
    _document = document;

    if (!document) {
        endResetModel();
        return;
    }

    auto root = document->getRoot();
    if (!root) {
        endResetModel();
        return;
    }

    _rootItem = std::make_unique<ObjectTreeItem>(nullptr, nullptr, nullptr);
    buildVirtualDocumentProps();
    addVisibleChildren(_rootItem.get(), root, false);

    // Watch root's repr with _rootItem as the backing item so notifications
    // for root's children (layers etc.) are received and dispatched correctly.
    _rootWatcher = std::make_unique<ObjectNodeWatcher>(this, _rootItem.get(), root->getRepr());

    // Watch namedview repr so virtual nodes (Guides, Grids) stay in sync.
    if (auto nv = document->getNamedView()) {
        _namedViewWatcher = std::make_unique<NamedViewWatcher>(this, nv->getRepr());
    }

    endResetModel();
}

void ObjectTreeModel::buildVirtualDocumentProps() {
    auto docProps =
        std::make_unique<ObjectTreeItem>(VirtualNodeType::DocumentProps, QObject::tr("Document"), _rootItem.get());
    auto docPropsPtr = docProps.get();
    _rootItem->addChild(std::move(docProps));

    auto display = std::make_unique<ObjectTreeItem>(VirtualNodeType::Display, QObject::tr("Display"), docPropsPtr);
    docPropsPtr->addChild(std::move(display));

    auto pages = std::make_unique<ObjectTreeItem>(VirtualNodeType::Pages, QObject::tr("Pages"), docPropsPtr);
    docPropsPtr->addChild(std::move(pages));

    auto about = std::make_unique<ObjectTreeItem>(VirtualNodeType::About, QObject::tr("About"), docPropsPtr);
    docPropsPtr->addChild(std::move(about));

    // auto canvas = std::make_unique<ObjectTreeItem>(VirtualNodeType::Canvas, QObject::tr("Canvas"), docPropsPtr);
    // docPropsPtr->addChild(std::move(canvas));

    auto grids = std::make_unique<ObjectTreeItem>(VirtualNodeType::Grids, QObject::tr("Grids"), docPropsPtr);
    docPropsPtr->addChild(std::move(grids));

    auto guides = std::make_unique<ObjectTreeItem>(VirtualNodeType::Guides, QObject::tr("Guides"), docPropsPtr);
    docPropsPtr->addChild(std::move(guides));
}

int ObjectTreeModel::visibleChildCount(SPObject* parentObject) const {
    if (!parentObject || !is<SPGroup>(parentObject)) return 0;

    return static_cast<int>(std::count_if(parentObject->children.begin(), parentObject->children.end(), [this](auto& child) {
        return shouldShowObject(&child) && passesFiltersRecursive(&child);
    }));
}

void ObjectTreeModel::addVisibleChildren(ObjectTreeItem* parentItem, SPObject* parentObject, bool useDummy) {
    if (!parentObject) return;
    // Only groups (and their subclasses like SPRoot) are containers in the tree.
    // SPUse, for example, stores the referenced object as a child but should not
    // list it underneath.
    if (!is<SPGroup>(parentObject)) return;

    if (useDummy) {
        bool hasVisibleChildren = false;
        for (auto& child : parentObject->children) {
            if (shouldShowObject(&child) && passesFiltersRecursive(&child)) {
                hasVisibleChildren = true;
                break;
            }
        }
        if (hasVisibleChildren) parentItem->setHasDummyChild(true);
        return;
    }

    // When text filtering is active, don't use dummy children — process
    // entire trees so all matching items are visible (matches GTK behavior).
    bool textFiltering = !_filterText.isEmpty();

    // SVG paints later siblings on top, so the tree lists children in reverse
    // document order to keep the topmost object at the top.
    for (auto child = parentObject->children.rbegin(); child != parentObject->children.rend(); ++child) {
        if (!shouldShowObject(&*child)) continue;
        if (!passesFiltersRecursive(&*child)) continue;

        auto childItem = std::make_unique<ObjectTreeItem>(_document, &*child, parentItem);
        auto childPtr = childItem.get();
        parentItem->addChild(std::move(childItem));
        _nodeToItem[childPtr->node()] = childPtr;

        bool childUseDummy = false;
        if (auto group = dynamic_cast<SPGroup*>(&*child)) {
            bool hasVisibleGrandchildren = false;
            for (auto& gc : child->children) {
                if (shouldShowObject(&gc) && passesFiltersRecursive(&gc)) {
                    hasVisibleGrandchildren = true;
                    break;
                }
            }
            // Disable dummy children when text filtering — show everything
            childUseDummy = hasVisibleGrandchildren && !group->isExpanded() && !textFiltering;
        }

        if (childUseDummy) {
            childPtr->setHasDummyChild(true);
        } else {
            addVisibleChildren(childPtr, &*child, false);
        }
    }
}

bool ObjectTreeModel::shouldShowObject(SPObject* object) {
    if (!object) return false;
    auto node = object->getRepr();
    if (!node) return false;

    if (is<SPDefs>(object)) return false;
    if (is<SPNamedView>(object)) return false;
    if (is<SPMetadata>(object)) return false;
    if (is<SPTitle>(object)) return false;

    if (is<SPText>(object->parent)) {
        if (node->type() == Inkscape::XML::NodeType::ELEMENT_NODE) {
            auto name = std::string(node->name());
            if (name == "svg:tspan" || name == "tspan" || name == "svg:textPath" || name == "textPath") {
                return false;
            }
        }
    }

    if (node->type() != Inkscape::XML::NodeType::ELEMENT_NODE) return false;

    return isVisibleItemType(object);
}

bool ObjectTreeModel::isVisibleItemType(SPObject* object) {
    // The object tree represents every drawable SPItem. Keep this test broad
    // so new item types are not silently omitted from the tree.
    return is<SPItem>(object);
}

bool ObjectTreeModel::passesFilters(SPObject* object) const {
    if (!object) return false;

    if (_layersOnly) {
        auto group = cast<SPGroup>(object);
        if (!group || group->layerMode() != SPGroup::LAYER) return false;
    }
    return true;
}

bool ObjectTreeModel::passesFiltersRecursive(SPObject* object) const {
    if (!object) return false;

    // Layers-only filter is not recursive — it's a type check on the item itself.
    if (_layersOnly) {
        auto group = cast<SPGroup>(object);
        if (!group || group->layerMode() != SPGroup::LAYER) return false;
    }

    // Text filter: show if this item matches, or any descendant matches.
    // Matches the displayed label: get_synthetic_object_name (displayName + id)
    // or user-defined label, plus tag name.
    if (!_filterText.isEmpty()) {
        auto term = _filterText.toLower();

        // Build source string from what the tree actually displays
        QString source;
        if (auto label = object->label()) {
            source = QString::fromUtf8(label);
        } else {
            source = get_synthetic_object_name(object);
        }

        if (source.toLower().contains(term)) return true;

        // Search descendants
        for (auto& child : object->children) {
            if (passesFiltersRecursive(&child)) return true;
        }
        return false;
    }

    return true;
}

bool ObjectTreeModel::hasVisibleDescendants(SPObject* object) const {
    if (!object || !is<SPGroup>(object)) return false;

    for (auto& child : object->children) {
        if (shouldShowObject(&child) && passesFilters(&child)) return true;
        if (hasVisibleDescendants(&child)) return true;
    }
    return false;
}

SPNamedView* ObjectTreeModel::findNamedView() const {
    if (!_document) return nullptr;

    return _document->getNamedView();

    // auto root = _document->getRoot();
    // if (!root) return nullptr;
    // for (auto& child : root->children) {
    //     if (auto nv = dynamic_cast<SPNamedView*>(&child)) return nv;
    // }
    // return nullptr;
}

QModelIndex ObjectTreeModel::indexForVirtualType(VirtualNodeType type) const {
    if (!_rootItem || _rootItem->children().empty()) return QModelIndex();
    // DocumentProps is always the first root child.
    auto docProps = _rootItem->children()[0].get();
    if (docProps->virtualType() == type) return indexForItem(docProps);

    for (const auto& child : docProps->children()) {
        if (child->virtualType() == type) return indexForItem(child.get());
    }
    return QModelIndex();
}

SPObject* ObjectTreeModel::objectForIndex(const QModelIndex& index) const {
    if (!index.isValid()) return nullptr;

    auto item = itemForIndex(index);
    return item ? item->object() : nullptr;
}

ObjectTreeItem* ObjectTreeModel::itemForIndex(const QModelIndex& index) const {
    if (!index.isValid()) return _rootItem.get();

    return static_cast<ObjectTreeItem*>(index.internalPointer());
}

QModelIndex ObjectTreeModel::indexForObject(SPObject* object) const {
    if (!_rootItem || !object) return QModelIndex();

    auto node = object->getRepr();
    if (!node) return QModelIndex();

    auto it = _nodeToItem.find(node);
    if (it != _nodeToItem.end()) return indexForItem(it->second);
    return QModelIndex();
}

void ObjectTreeModel::expandToReveal(QTreeView* view, SPObject* object) {
    if (!view || !object) return;

    // Check the preference before doing any work.
    if (!Inkscape::Preferences::get()->getBool("/dialogs/objects/expand_to_layer", true)) return;

    // Collect ancestors from root down to the target's parent.
    std::vector<SPObject*> ancestors;
    for (auto p = object->parent; p; p = p->parent) {
        ancestors.push_back(p);
    }
    std::reverse(ancestors.begin(), ancestors.end());

    // Expand each ancestor from root down. Expanding a parent triggers
    // itemExpanded → materializes its children + creates watchers, so the
    // next ancestor's index becomes valid by the time we reach it.
    for (auto ancestor : ancestors) {
        auto index = indexForObject(ancestor);
        if (!index.isValid()) continue;
        view->expand(index);
    }
}

void ObjectTreeModel::selectObject(QTreeView* view, SPObject* object, bool edit) {
    if (!object || !view) return;

    expandToReveal(view, object);

    auto index = indexForObject(object);
    if (!index.isValid()) return;

    view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    view->scrollTo(index, QAbstractItemView::PositionAtCenter);
    if (edit) view->edit(index);
}

void ObjectTreeModel::selectObjects(QTreeView* view, const Inkscape::RandomAccessIndex& objects) {
    if (!view) return;

    view->selectionModel()->clearSelection();
    bool first = true;
    for (auto obj : objects) {
        expandToReveal(view, obj);

        auto index = indexForObject(obj);
        if (!index.isValid()) continue;
        auto flags = QItemSelectionModel::Select | QItemSelectionModel::Rows;
        view->selectionModel()->select(index, flags);
        if (first) {
            view->scrollTo(index, QAbstractItemView::PositionAtCenter);
            first = false;
        }
    }
}

QModelIndex ObjectTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) return QModelIndex();

    auto parentItem = itemForIndex(parent);
    if (!parentItem || row < 0 || static_cast<size_t>(row) >= parentItem->children().size()) return QModelIndex();
    auto childItem = parentItem->children()[row].get();
    return createIndex(row, column, childItem);
}

QModelIndex ObjectTreeModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) return QModelIndex();

    auto childItem = itemForIndex(child);
    auto parentItem = childItem->parent();
    if (!parentItem || parentItem == _rootItem.get()) return QModelIndex();
    return indexForItem(parentItem);
}

QModelIndex ObjectTreeModel::indexForItem(ObjectTreeItem* item) const {
    if (!item || item == _rootItem.get()) return QModelIndex();

    return createIndex(item->row(), 0, item);
}

int ObjectTreeModel::rowCount(const QModelIndex& parent) const {
    auto parentItem = itemForIndex(parent);
    if (!parentItem) return 0;
    if (parentItem->hasDummyChild()) return 1;
    return static_cast<int>(parentItem->children().size());
}

int ObjectTreeModel::columnCount(const QModelIndex&) const {
    return ColumnCount;
}

QVariant ObjectTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    auto item = itemForIndex(index);
    if (!item) return {};

    if (item->isVirtual()) {
        const int col = index.column();
        const auto vtype = item->virtualType();

        if (role == Qt::DecorationRole) {
            if (col == ColumnVisible) {
                if (vtype != VirtualNodeType::Guides && vtype != VirtualNodeType::Grids) return {};

                auto nv = findNamedView();
                bool hidden = (vtype == VirtualNodeType::Guides && nv && !nv->getShowGuides()) ||
                              (vtype == VirtualNodeType::Grids && nv && !nv->getShowGrids());
                return visibilityIcon(hidden);
            }
            if (col == ColumnLocked) {
                if (vtype != VirtualNodeType::Guides) return {};

                auto nv = findNamedView();
                bool locked = vtype == VirtualNodeType::Guides && nv && nv->getLockGuides();
                return lockIcon(locked);
            }
            return QVariant();
        }

        if (role == IsHiddenRole) {
            if (col != ColumnVisible) return {};

            auto nv = findNamedView();
            if (vtype == VirtualNodeType::Guides) return nv && !nv->getShowGuides();
            if (vtype == VirtualNodeType::Grids) return nv && !nv->getShowGrids();
            return {};
        }

        if (role == IsLockedRole) {
            if (col != ColumnLocked) return {};

            auto nv = findNamedView();
            if (vtype == VirtualNodeType::Guides) return nv && nv->getLockGuides();
            return {};
        }

        switch (role) {
            case Qt::DisplayRole:
            case Qt::EditRole:
            case PlainTextRole:
                return item->label();
            case VirtualTypeRole:
                return static_cast<int>(vtype);
            case IsLayerRole:
                return false;
            default:
                return QVariant();
        }
    }

    auto obj = item->object();
    if (!obj) return QVariant();
    if (role == VirtualTypeRole) return static_cast<int>(VirtualNodeType::None);
    auto itemObj = dynamic_cast<SPItem*>(obj);

    const int col = index.column();

    if (col == ColumnVisible) {
        if (role == Qt::DecorationRole) {
            bool hidden = itemObj ? itemObj->isHidden() : false;
            return visibilityIcon(hidden);
        }
        if (role == IsHiddenRole) return itemObj ? itemObj->isHidden() : false;
        return QVariant();
    }

    if (col == ColumnLocked) {
        if (role == Qt::DecorationRole) {
            bool locked = itemObj ? !itemObj->isSensitive() : false;
            return lockIcon(locked);
        }
        if (role == IsLockedRole) return itemObj ? !itemObj->isSensitive() : false;
        return QVariant();
    }

    switch (role) {
        case Qt::DecorationRole:
            return itemObj ? objectIcon(itemObj) : QVariant();
        case Qt::DisplayRole:
        case Qt::EditRole:
        case PlainTextRole: {
            if (itemObj) {
                // user-defined label takes priority
                if (auto label = itemObj->label()) return QString::fromUtf8(label);
            }
            return get_synthetic_object_name(obj);
        }
        case ObjectRole:
            return QVariant::fromValue(obj);
        case IsLayerRole: {
            auto group = cast<SPGroup>(obj);
            return group && group->layerMode() == SPGroup::LAYER;
        }
        case IsHiddenRole:
            return itemObj ? itemObj->isHidden() : false;
        case IsLockedRole:
            return itemObj ? !itemObj->isSensitive() : false;
        default:
            return QVariant();
    }
}

bool ObjectTreeModel::setData(const QModelIndex& /*index*/, const QVariant& /*value*/, int /*role*/) {
    // The model is read-only. Mutations (visibility, lock, label) are requested
    // via signals from ObjectTreeView and performed by the client with proper
    // undo handling.
    return false;
}

Qt::ItemFlags ObjectTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    auto item = itemForIndex(index);
    if (!item) return Qt::NoItemFlags;

    if (item->isVirtual()) return Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    auto obj = item->object();
    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (obj) f |= Qt::ItemIsEditable;
    if (index.parent().isValid() && obj) f |= Qt::ItemIsDragEnabled;

    // Only groups and layers can receive dropped children (OnItem drop).
    if (obj && is<SPGroup>(obj)) f |= Qt::ItemIsDropEnabled;

    return f;
}

Qt::DropActions ObjectTreeModel::supportedDropActions() const {
    return Qt::MoveAction;
}

Qt::DropActions ObjectTreeModel::supportedDragActions() const {
    return Qt::MoveAction;
}

QStringList ObjectTreeModel::mimeTypes() const {
    return QStringList() << "application/x-inkscape-object";
}

QMimeData* ObjectTreeModel::mimeData(const QModelIndexList& indexes) const {
    if (indexes.isEmpty()) return nullptr;

    auto mimeData = new QMimeData();
    auto obj = objectForIndex(indexes.first());
    if (obj) {
        QByteArray encoded(sizeof(obj), Qt::Uninitialized);
        std::memcpy(encoded.data(), &obj, sizeof(obj));
        mimeData->setData("application/x-inkscape-object", encoded);
    }
    return mimeData;
}

bool ObjectTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int /*column*/,
                                   const QModelIndex& parent) {
    if (action != Qt::MoveAction) return false;
    if (!data->hasFormat("application/x-inkscape-object")) return false;

    QByteArray encoded = data->data("application/x-inkscape-object");
    if (encoded.size() != sizeof(SPObject*)) return false;

    auto draggedObj = *reinterpret_cast<SPObject**>(encoded.data());
    if (!draggedObj) return false;

    auto draggedParent = draggedObj->parent;
    if (!draggedParent) return false;

    auto draggedRepr = draggedObj->getRepr();
    if (!draggedRepr) return false;

    auto dropItem = itemForIndex(parent);
    SPObject* dropParentObj = dropItem ? dropItem->object() : nullptr;
    if (!dropParentObj) dropParentObj = _document ? _document->getRoot() : nullptr;
    if (!dropParentObj || draggedObj == dropParentObj) return false;

    auto dropParentRepr = dropParentObj->getRepr();
    auto draggedParentRepr = draggedParent->getRepr();
    if (!dropParentRepr || !draggedParentRepr) return false;

    // row is a model row index (visible items only).
    // Find the XML repr to insert after, based on the model child at `row`.
    const auto& modelChildren = dropItem->children();
    Inkscape::XML::Node* insertAfter = nullptr;
    if (row >= 0 && static_cast<size_t>(row) < modelChildren.size()) {
        auto beforeObj = modelChildren[row]->object();
        insertAfter = beforeObj ? beforeObj->getRepr()->prev() : nullptr;
    } else {
        auto lastObj = modelChildren.empty() ? nullptr : modelChildren.back()->object();
        insertAfter = lastObj ? lastObj->getRepr() : dropParentRepr->lastChild();
    }

    if (dropParentRepr == draggedParentRepr) {
        // Same parent: changeOrder keeps SPObject attached so watcher fires cleanly.
        // Skip draggedRepr itself when computing insertAfter.
        Inkscape::XML::Node* sameParentInsertAfter = nullptr;
        int seen = 0;
        for (const auto& c : modelChildren) {
            if (c->object() == draggedObj) continue;
            if (seen == row - 1) {
                auto obj = c->object();
                sameParentInsertAfter = obj ? obj->getRepr() : nullptr;
                break;
            }
            ++seen;
        }
        dropParentRepr->changeOrder(draggedRepr, sameParentInsertAfter);
    } else {
        // Cross-parent: watchers key children by XML::Node* so reconciliation
        // matches correctly even though the SPObject is momentarily detached
        // during removeChild.
        draggedParentRepr->removeChild(draggedRepr);
        if (insertAfter == draggedRepr) insertAfter = dropParentRepr->lastChild();
        dropParentRepr->addChild(draggedRepr, insertAfter);
    }

    return true;
}

QVariant ObjectTreeModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const {
    return QVariant();
}

void ObjectTreeModel::setShowLayersOnly(bool layersOnly) {
    if (_layersOnly == layersOnly) return;
    _layersOnly = layersOnly;
    // Rebuild tree with new filter
    buildTree(_document);
}

void ObjectTreeModel::setFilterText(const QString& text) {
    if (_filterText == text) return;

    _filterText = text;
    // Rebuild tree with new filter
    buildTree(_document);
}

void ObjectTreeModel::itemExpanded(const QModelIndex& index) {
    auto item = itemForIndex(index);
    if (!item) return;

    if (item->isVirtual()) {
        if (item->virtualType() == VirtualNodeType::DocumentProps) {
            _documentExpanded = true;
        }
        return;
    }

    auto obj = item->object();
    auto group = dynamic_cast<SPGroup*>(obj);
    if (!group) return;

    // Mark as expanded
    group->setExpanded(true);

    // If we have a dummy child, replace it with real children
    if (item->hasDummyChild()) {
        auto parentIndex = indexForItem(item);

        // Remove dummy child with proper signal
        beginRemoveRows(parentIndex, 0, 0);
        item->setHasDummyChild(false);
        endRemoveRows();

        // Count visible+filtered direct children for correct insert range
        const int childCount = visibleChildCount(obj);

        if (childCount > 0) {
            beginInsertRows(parentIndex, 0, childCount - 1);
            addVisibleChildren(item, obj, false);
            endInsertRows();
        }

        // Create watchers for the newly-materialized children.
        // rebuildChildren recurses: each new child watcher's constructor
        // calls rebuildChildren again, covering nested expanded sub-groups.
        if (auto watcher = item->watcher()) {
            watcher->rebuildChildren();
        }
    }
}

void ObjectTreeModel::itemCollapsed(const QModelIndex& index) {
    auto item = itemForIndex(index);
    if (!item) return;

    if (item->isVirtual()) {
        if (item->virtualType() == VirtualNodeType::DocumentProps) {
            _documentExpanded = false;
        }
        return;
    }

    auto obj = item->object();
    auto group = dynamic_cast<SPGroup*>(obj);
    if (!group) return;

    group->setExpanded(false);
}

void ObjectTreeModel::restoreExpanded(QTreeView* view) {
    if (!view || !_rootItem) return;
    _restoringExpanded = true;
    restoreExpandedRecursive(view, _rootItem.get());
    _restoringExpanded = false;
}

void ObjectTreeModel::restoreExpandedRecursive(QTreeView* view, ObjectTreeItem* item) {
    for (const auto& child : item->children()) {
        auto index = indexForItem(child.get());
        if (!index.isValid()) continue;

        if (child->isVirtual()) {
            if (child->virtualType() == VirtualNodeType::DocumentProps && _documentExpanded) {
                view->expand(index);
                restoreExpandedRecursive(view, child.get());
            }
            continue;
        }

        auto obj = child->object();
        if (!obj) continue;
        auto group = dynamic_cast<SPGroup*>(obj);
        if (!group || !group->isExpanded()) continue;

        // expand triggers itemExpanded → materializes children + creates watchers
        view->expand(index);

        // recurse into now-materialized children
        restoreExpandedRecursive(view, child.get());
    }
}

} // namespace Linea::UI
