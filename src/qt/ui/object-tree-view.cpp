// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A Qt widget showing the abstracted object tree.
 */

#include "object-tree-view.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMimeData>
#include <QStyledItemDelegate>

#include "desktop.h"
#include "ui/contextmenu.h"
#include "layer-manager.h"
#include "object-tree-model.h"
#include "object/sp-item.h"
#include "qt/util/virtual-node-type.h"
#include "selection.h"

namespace Linea::UI {

/**
 * Custom delegate for rendering object tree items.
 */
class ObjectTreeDelegate : public QStyledItemDelegate {
public:
    explicit ObjectTreeDelegate(ObjectTreeView* view, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , _view(view) {}

    void setCurrentLayer(SPObject* layer) {
        _currentLayer = layer;
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        if (index.column() != ObjectTreeModel::ColumnLabel) return;
        if (auto vtype = static_cast<VirtualNodeType>(index.data(ObjectTreeModel::VirtualTypeRole).toInt()); vtype != VirtualNodeType::None) return;
        auto value = static_cast<QLineEdit*>(editor)->text();
        if (value.isEmpty()) return;
        if (auto obj = static_cast<ObjectTreeModel*>(model)->objectForIndex(index)) {
            Q_EMIT _view->objectLabelRequested(obj, value);
        }
    }

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                      const QModelIndex& index) const override {
        auto editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (auto lineEdit = qobject_cast<QLineEdit*>(editor)) {
            lineEdit->setFrame(true);
            // lineEdit->setFixedHeight(16);
            lineEdit->setStyleSheet("QLineEdit { padding: 0px; }");
            // lineEdit->setObjectName("ObjectTreeEditor");
        }
        return editor;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Check if virtual node
        auto role = static_cast<VirtualNodeType>(index.data(ObjectTreeModel::VirtualTypeRole).toInt());
        bool isVirtual = role != VirtualNodeType::None;

        // Virtual nodes (apart from top Document) get italic styling, as they don't correspond to actual svg elements
        if (isVirtual && role != VirtualNodeType::DocumentProps) {
            QFont font = opt.font;
            font.setItalic(true);
            opt.font = font;
        }

        // Check if layer and if it's the current layer; show current layer in bold
        bool isLayer = index.data(ObjectTreeModel::IsLayerRole).toBool();
        if (isLayer) {
            auto obj = index.data(ObjectTreeModel::ObjectRole).value<SPObject*>();
            if (obj == _currentLayer) {
                QFont font = opt.font;
                font.setBold(true);
                opt.font = font;
            }
        }

        // Check if hidden - render with strikethrough or grayed out
        bool isHidden = index.data(ObjectTreeModel::IsHiddenRole).toBool();
        if (isHidden) {
            opt.palette.setBrush(QPalette::Text, opt.palette.color(QPalette::Disabled, QPalette::Text));
        }

        QStyledItemDelegate::paint(painter, opt, index);
    }

private:
    ObjectTreeView* _view;
    SPObject* _currentLayer = nullptr;
};

ObjectTreeView::ObjectTreeView(QWidget* parent)
    : QTreeView(parent) {
    setObjectName("ObjectTreeView");

    // Create and set the model
    _model = new ObjectTreeModel(this);
    setModel(_model);

    // Set up the delegate for custom rendering
    _delegate = std::make_unique<ObjectTreeDelegate>(this, this);
    setItemDelegate(_delegate.get());

    // Configure tree view appearance
    setFrameShape(QFrame::NoFrame);
    setHeaderHidden(true);
    setUniformRowHeights(true);
    setAnimated(false);  // Disable animation for performance with large trees
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);

    // Label column stretches; icon columns are fixed-width
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(ObjectTreeModel::ColumnLabel, QHeaderView::Stretch);
    header()->setSectionResizeMode(ObjectTreeModel::ColumnVisible, QHeaderView::Fixed);
    header()->setSectionResizeMode(ObjectTreeModel::ColumnLocked, QHeaderView::Fixed);
    header()->resizeSection(ObjectTreeModel::ColumnVisible, 24);
    header()->resizeSection(ObjectTreeModel::ColumnLocked, 24);

    // Enable tooltips
    setMouseTracking(true);

    // Connect expand/collapse signals for lazy loading
    connect(this, &QTreeView::expanded, this, &ObjectTreeView::onExpanded);
    connect(this, &QTreeView::collapsed, this, &ObjectTreeView::onCollapsed);

    // See _modelMutationDepth's comment: block selectionChanged() from
    // forwarding to the desktop while the model is in the middle of a
    // structural change.
    auto const beginMutating = [this] { ++_modelMutationDepth; };
    auto const endMutating = [this] { --_modelMutationDepth; };
    connect(_model, &QAbstractItemModel::rowsAboutToBeInserted, this, beginMutating);
    connect(_model, &QAbstractItemModel::rowsInserted, this, endMutating);
    connect(_model, &QAbstractItemModel::rowsAboutToBeRemoved, this, beginMutating);
    connect(_model, &QAbstractItemModel::rowsRemoved, this, endMutating);
    connect(_model, &QAbstractItemModel::rowsAboutToBeMoved, this, beginMutating);
    connect(_model, &QAbstractItemModel::rowsMoved, this, endMutating);
    connect(_model, &QAbstractItemModel::modelAboutToBeReset, this, beginMutating);
    connect(_model, &QAbstractItemModel::modelReset, this, endMutating);
}

ObjectTreeView::~ObjectTreeView() = default;

void ObjectTreeView::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void ObjectTreeView::buildTree(SPDocument* document) {
    _model->buildTree(document);
    if (document) {
        // Restore expansion state from SPItem::isExpanded() flags.
        // This expands root-level children (replacing the old expandToDepth(0))
        // plus any previously-expanded sub-groups.
        _model->restoreExpanded(this);
    }
}

SPObject* ObjectTreeView::selectedObject() const {
    auto index = currentIndex();
    if (!index.isValid()) {
        return nullptr;
    }
    return _model->objectForIndex(index);
}

SPItem* ObjectTreeView::selectedItem() const {
    auto obj = selectedObject();
    if (!obj) {
        return nullptr;
    }
    return dynamic_cast<SPItem*>(obj);
}

VirtualNodeType ObjectTreeView::selectedVirtualNode() const {
    for (const auto& index : selectionModel()->selectedRows()) {
        auto node = static_cast<VirtualNodeType>(index.data(ObjectTreeModel::VirtualTypeRole).toInt());
        if (node != VirtualNodeType::None) {
            return node;
        }
    }
    return VirtualNodeType::None;
}

void ObjectTreeView::selectObject(SPObject* object, bool edit) {
    auto guard = _selectingProgrammatically.block();
    _model->selectObject(this, object, edit);
}

void ObjectTreeView::selectObjects(const Inkscape::RandomAccessIndex& objects) {
    auto guard = _selectingProgrammatically.block();
    _model->selectObjects(this, objects);
}

void ObjectTreeView::restoreSelection(const Inkscape::RandomAccessIndex& objects) {
    auto guard = _selectingProgrammatically.block();
    _model->selectObjects(this, objects);
    emitSelectionSignals();
}

void ObjectTreeView::restoreVirtualNode(VirtualNodeType type) {
    auto guard = _selectingProgrammatically.block();
    auto index = _model->indexForVirtualType(type);
    if (!index.isValid()) return;
    selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    emitSelectionSignals();
}

ObjectTreeModel* ObjectTreeView::objectModel() const {
    return _model;
}

void ObjectTreeView::setShowLayersOnly(bool layersOnly) {
    _model->setShowLayersOnly(layersOnly);
    _model->restoreExpanded(this);
}

bool ObjectTreeView::showLayersOnly() const {
    return _model->showLayersOnly();
}

void ObjectTreeView::setFilterText(const QString& text) {
    _model->setFilterText(text);
    _model->restoreExpanded(this);
}

QString ObjectTreeView::filterText() const {
    return _model->filterText();
}

void ObjectTreeView::setCurrentLayer(SPObject* layer) {
    auto delegate = static_cast<ObjectTreeDelegate*>(_delegate.get());
    delegate->setCurrentLayer(layer);
    viewport()->update();
}

void ObjectTreeView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        auto index = indexAt(event->pos());
        if (index.isValid()) {
            const int col = index.column();
            auto vtype = static_cast<VirtualNodeType>(index.data(ObjectTreeModel::VirtualTypeRole).toInt());

            if (col == ObjectTreeModel::ColumnVisible) {
                auto hidden = index.data(ObjectTreeModel::IsHiddenRole);
                if (hidden.isNull()) return;

                bool isHidden = hidden.toBool();
                if (vtype == VirtualNodeType::None) {
                    if (auto obj = _model->objectForIndex(index)) {
                        Q_EMIT objectVisibilityRequested(obj, !isHidden);
                    }
                } else {
                    Q_EMIT virtualNodeVisibilityRequested(vtype, !isHidden);
                }
                viewport()->update();
                return;
            }
            if (col == ObjectTreeModel::ColumnLocked) {
                auto locked = index.data(ObjectTreeModel::IsLockedRole);
                if (locked.isNull()) return;

                bool isLocked = locked.toBool();
                if (vtype == VirtualNodeType::None) {
                    if (auto obj = _model->objectForIndex(index)) {
                        Q_EMIT objectLockRequested(obj, !isLocked);
                    }
                } else {
                    Q_EMIT virtualNodeLockRequested(vtype, !isLocked);
                }
                viewport()->update();
                return;
            }
        }
    }
    QTreeView::mousePressEvent(event);
}

void ObjectTreeView::contextMenuEvent(QContextMenuEvent* event) {
    auto index = indexAt(event->pos());
    if (!index.isValid() || !_desktop) {
        QTreeView::contextMenuEvent(event);
        return;
    }

    // Skip virtual nodes (DocumentProps, Guides, Grids, etc.) — they don't
    // correspond to real SVG objects and the ContextMenu doesn't handle them.
    auto vtype = static_cast<VirtualNodeType>(index.data(ObjectTreeModel::VirtualTypeRole).toInt());
    if (vtype != VirtualNodeType::None) {
        QTreeView::contextMenuEvent(event);
        return;
    }

    auto object = _model->objectForIndex(index);
    if (!object) {
        QTreeView::contextMenuEvent(event);
        return;
    }

    // If the clicked object is not in the current selection, select it first
    // so the ContextMenu operates on the right object (matching the Objects
    // dialog behavior). Layers are not selected — they get their own menu.
    auto item = cast<SPItem>(object);
    auto selection = _desktop->getSelection();
    bool isLayer = item && Inkscape::LayerManager::asLayer(item);
    if (item && !isLayer && !selection->includes(object)) {
        selectObject(object);
        Q_EMIT objectsSelected({object});
    }

    std::vector<SPItem*> items;
    if (item) items.push_back(item);

    auto menu = new ContextMenu(_desktop, object, items);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}

void ObjectTreeView::startDrag(Qt::DropActions supportedActions) {
    auto index = currentIndex();
    if (!index.isValid()) {
        return;
    }

    // Check if we can drag this node
    auto obj = _model->objectForIndex(index);
    if (!obj) {
        return;
    }

    // Don't drag virtual nodes
    bool isVirtual = index.data(ObjectTreeModel::VirtualTypeRole).toInt() !=
                    static_cast<int>(VirtualNodeType::None);
    if (isVirtual) {
        return;
    }

    // Don't drag root
    if (!index.parent().isValid()) {
        return;
    }

    // Don't drag non-items
    auto item = dynamic_cast<SPItem*>(obj);
    if (!item) {
        return;
    }

    QTreeView::startDrag(supportedActions);
}

void ObjectTreeView::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-inkscape-object")) {
        event->ignore();
        return;
    }
    QTreeView::dragEnterEvent(event);
}

void ObjectTreeView::dragMoveEvent(QDragMoveEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-inkscape-object")) {
        event->ignore();
        return;
    }

    // Reject drops onto virtual nodes; container vs non-container is handled by flags().
    auto index = indexAt(event->position().toPoint());
    if (index.isValid()) {
        bool isVirtual = index.data(ObjectTreeModel::VirtualTypeRole).toInt() !=
                         static_cast<int>(VirtualNodeType::None);
        if (isVirtual) {
            event->ignore();
            return;
        }
    }

    QTreeView::dragMoveEvent(event);
}

void ObjectTreeView::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-inkscape-object")) {
        event->ignore();
        return;
    }

    // Compute row and parent from the drop indicator position ourselves,
    // then call dropMimeData directly. This avoids QAbstractItemView::dropEvent
    // calling removeRows() after the drop (the XML move is already done in dropMimeData).
    QModelIndex dropIndex = indexAt(event->position().toPoint());
    QModelIndex dropParent;
    int dropRow = -1;
    switch (dropIndicatorPosition()) {
        case QAbstractItemView::AboveItem:
            dropRow = dropIndex.row();
            dropParent = dropIndex.parent();
            break;
        case QAbstractItemView::BelowItem:
            dropRow = dropIndex.row() + 1;
            dropParent = dropIndex.parent();
            break;
        case QAbstractItemView::OnItem:
        case QAbstractItemView::OnViewport:
            dropRow = -1;
            dropParent = dropIndex;
            break;
    }

    if (_model->dropMimeData(event->mimeData(), Qt::MoveAction, dropRow, 0, dropParent)) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ObjectTreeView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    QTreeView::selectionChanged(selected, deselected);

    if (_selectingProgrammatically.pending() || _modelMutationDepth > 0) return;

    emitSelectionSignals();
}

void ObjectTreeView::emitSelectionSignals() {
    // Check if a single virtual node is selected
    auto selectedRows = selectionModel()->selectedRows();
    if (selectedRows.size() == 1) {
        auto virtualType = static_cast<VirtualNodeType>(
            selectedRows[0].data(ObjectTreeModel::VirtualTypeRole).toInt());
        if (virtualType != VirtualNodeType::None) {
            Q_EMIT virtualNodeSelected(virtualType);
            return;  // Don't emit objectsSelected for virtual nodes
        }
    }

    std::vector<SPObject*> objects;
    for (auto& idx : selectedRows) {
        if (auto obj = _model->objectForIndex(idx)) {
            objects.push_back(obj);
        }
    }
    Q_EMIT objectsSelected(std::move(objects));
}

void ObjectTreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const {
    QTreeView::drawRow(painter, options, index);

    // Could add custom overlay drawing here (e.g., lock icons, visibility indicators)
}

void ObjectTreeView::onExpanded(const QModelIndex& index) {
    _model->itemExpanded(index);
}

void ObjectTreeView::onCollapsed(const QModelIndex& index) {
    _model->itemCollapsed(index);
}

void ObjectTreeView::collapseAllExcept(SPObject* layer) {
    bool docExpanded = _model->isDocumentExpanded();
    collapseAll();
    if (docExpanded) {
        if (auto idx = _model->indexForVirtualType(VirtualNodeType::DocumentProps); idx.isValid()) {
            expand(idx);
        }
    }
    if (layer) {
        _model->expandToReveal(this, layer);
    }
}

} // namespace Linea::UI
