// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A Qt widget showing the abstracted object tree.
 */

#include "object-tree-view.h"

#include <QApplication>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolTip>

#include "desktop.h"
#include "ui/contextmenu.h"
#include "layer-manager.h"
#include "object-tree-model.h"
#include "object/sp-item.h"
#include "qt/util/virtual-node-type.h"
#include "selection.h"
#include "widget-utils.h"

namespace Linea::UI {

// True when an eye/lock cell paints no icon for an un-raised row: neither
// the item's own nor an ancestor's hidden/locked state is set, and the row
// is not virtual (virtual icons always paint).
static bool iconCellSuppressed(const QModelIndex& index) {
    const int col = index.column();
    if (col != ObjectTreeModel::ColumnVisible && col != ObjectTreeModel::ColumnLocked) return false;
    if (index.data(ObjectTreeModel::VirtualTypeRole).toInt() != static_cast<int>(VirtualNodeType::None)) return false;
    if (col == ObjectTreeModel::ColumnVisible) {
        return !index.data(ObjectTreeModel::IsHiddenRole).toBool() &&
               !index.data(ObjectTreeModel::AncestorHiddenRole).toBool();
    }
    return !index.data(ObjectTreeModel::IsLockedRole).toBool() &&
           !index.data(ObjectTreeModel::AncestorLockedRole).toBool();
}

/**
 * Custom delegate for rendering object tree items.
 */
class ObjectTreeDelegate : public QStyledItemDelegate {
public:
    explicit ObjectTreeDelegate(ObjectTreeView* view, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , _view(view) {}

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

        auto role = static_cast<VirtualNodeType>(index.data(ObjectTreeModel::VirtualTypeRole).toInt());
        bool isVirtual = role != VirtualNodeType::None;

        opt.font = labelFont(index, opt.font);

        // Check if hidden - render with strikethrough or grayed out
        bool isHidden = index.data(ObjectTreeModel::IsHiddenRole).toBool();
        if (isHidden) {
            opt.palette.setBrush(QPalette::Text, opt.palette.color(QPalette::Disabled, QPalette::Text));
        }

        // The eye/lock icons only appear on hover or selection; the
        // hidden/locked icons are always shown, as are faint icons on children
        // of hidden/locked parents. Virtual nodes are exempt.
        const int col = index.column();
        const bool isIconColumn = !isVirtual &&
            (col == ObjectTreeModel::ColumnVisible || col == ObjectTreeModel::ColumnLocked);
        const bool ownState = isIconColumn &&
            ((col == ObjectTreeModel::ColumnVisible && isHidden) ||
             (col == ObjectTreeModel::ColumnLocked && index.data(ObjectTreeModel::IsLockedRole).toBool()));
        const bool ancestorState = isIconColumn &&
            index.data(col == ObjectTreeModel::ColumnVisible ? ObjectTreeModel::AncestorHiddenRole
                                                            : ObjectTreeModel::AncestorLockedRole)
                      .toBool();
        const bool raised = opt.state & (QStyle::State_MouseOver | QStyle::State_Selected);
        const bool gossamer = isIconColumn && ancestorState && !ownState && !raised;
        if (!raised && iconCellSuppressed(index)) {
            opt.icon = QIcon();
            opt.features &= ~QStyleOptionViewItem::HasDecoration;
        }

        const auto style = opt.widget ? opt.widget->style() : QApplication::style();

        // The label column fades overflowing text out at the right edge
        // (like ElidingLabel) instead of eliding with "...". Clearing
        // opt.text keeps the style from eliding it; it is drawn manually
        // below. QStyledItemDelegate::paint() would re-run initStyleOption()
        // and undo both modifications; draw the item directly instead.
        const QString labelText = col == ObjectTreeModel::ColumnLabel ? opt.text : QString();
        if (!labelText.isEmpty()) {
            opt.text.clear();
        }

        painter->save();
        if (gossamer) {
            // Upstream paints ancestor-state icons at 0.2; the model's icon
            // already bakes in 0.5, so 0.4 here gives ~0.2 net.
            painter->setOpacity(0.4);
        }
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
        painter->restore();

        // Hover confirmation over an icon cell: the model's
        // visible/unlocked icons bake in 0.5 alpha, so a second pass
        // brings the one under the cursor to ~0.75.
        if (isIconColumn && !opt.icon.isNull() && _view->_hotIconIndex == index) {
            auto iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
            opt.icon.paint(painter, iconRect);
        }

        if (!labelText.isEmpty()) {
            painter->save();
            painter->setFont(opt.font);
            const auto colorRole = opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text;
            paintFadedText(*painter, labelTextRect(opt, index), labelText, opt.palette.color(colorRole),
                           opt.displayAlignment | Qt::AlignVCenter);
            painter->restore();
        }
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override {
        if (index.column() != ObjectTreeModel::ColumnLabel) {
            return QStyledItemDelegate::helpEvent(event, view, option, index);
        }
        auto opt = option;
        initStyleOption(&opt, index);
        // viewOptions() leaves option.rect empty; the measurement needs the
        // item's actual cell rect.
        opt.rect = view->visualRect(index);
        if (opt.text.isEmpty()) return false;
        opt.font = labelFont(index, opt.font);
        const auto textWidth = QFontMetrics(opt.font).horizontalAdvance(opt.text);
        // Too long even without icons (extension into icon cells allowed).
        bool truncated = textWidth > labelTextRect(opt, index).width();
        if (!truncated && view->selectionModel()->isSelected(index)) {
            // Selected rows show icons, which takes label space back —
            // remeasure against the raised-row rect.
            opt.state |= QStyle::State_MouseOver;
            truncated = textWidth > labelTextRect(opt, index).width();
        }
        if (!truncated) return false;
        QToolTip::showText(event->globalPos(), opt.text, view->viewport());
        return true;
    }

private:
    // Italic for virtual nodes (apart from top Document), as they don't
    // correspond to actual svg elements; bold for the current layer.
    QFont labelFont(const QModelIndex& index, QFont font) const {
        auto vtype = static_cast<VirtualNodeType>(index.data(ObjectTreeModel::VirtualTypeRole).toInt());
        if (vtype != VirtualNodeType::None && vtype != VirtualNodeType::DocumentProps) {
            font.setItalic(true);
        }
        if (index.data(ObjectTreeModel::IsLayerRole).toBool()) {
            auto obj = index.data(ObjectTreeModel::ObjectRole).value<SPObject*>();
            if (_view->isCurrentLayer(obj)) {
                font.setBold(true);
            }
        }
        return font;
    }

    // Same rect and margin viewItemDrawText() uses for the label text.
    // Overflowing labels may extend into icon cells that paint no icon; the
    // eye column sits left of the lock column, so extension stops at the
    // first painted icon — text can't cross it.
    QRect labelTextRect(const QStyleOptionViewItem& opt, const QModelIndex& index) const {
        const auto style = opt.widget ? opt.widget->style() : QApplication::style();
        auto textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) + 1;
        textRect.adjust(textMargin, 0, -textMargin, 0);
        if (!(opt.state & (QStyle::State_MouseOver | QStyle::State_Selected))) {
            for (const int iconCol : {ObjectTreeModel::ColumnVisible, ObjectTreeModel::ColumnLocked}) {
                const auto iconIndex = index.siblingAtColumn(iconCol);
                if (!iconIndex.isValid() || !iconCellSuppressed(iconIndex)) break;
                textRect.setRight(_view->visualRect(iconIndex).right());
            }
        }
        return textRect;
    }

    ObjectTreeView* _view;
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
    auto id = layer ? layer->getId() : nullptr;
    _currentLayerId = id ? id : "";
    viewport()->update();
}

bool ObjectTreeView::isCurrentLayer(SPObject* obj) const {
    return obj && !_currentLayerId.empty() && obj->getId() && _currentLayerId == obj->getId();
}

void ObjectTreeView::mousePressEvent(QMouseEvent* event) {
    QModelIndex index;
    QModelIndexList before;
    QModelIndex cursorBefore;
    if (event->button() == Qt::LeftButton) {
        before = selectionModel()->selectedRows();
        cursorBefore = currentIndex();
        index = indexAt(event->pos());
        if (index.isValid()) {
            const int col = index.column();
            auto vtype = static_cast<VirtualNodeType>(index.data(ObjectTreeModel::VirtualTypeRole).toInt());

            // A null state role means the icon doesn't apply in this cell;
            // fall through so the click selects the row like a label click.
            if (col == ObjectTreeModel::ColumnVisible) {
                auto hidden = index.data(ObjectTreeModel::IsHiddenRole);
                if (!hidden.isNull()) {
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
            }
            if (col == ObjectTreeModel::ColumnLocked) {
                auto locked = index.data(ObjectTreeModel::IsLockedRole);
                if (!locked.isNull()) {
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
    }
    // selectionChanged() is suppressed while the event is dispatched; the
    // resulting selection is emitted here with the press's own modifiers.
    // A re-click of the cursor row (currentIndex survives selection changes,
    // like GTK's cursor row) emits with reselected=true even when the view's
    // selection didn't change — e.g. a second click selects the current layer.
    auto guard = _inputEvent.block();
    QTreeView::mousePressEvent(event);
    const bool reclicked = index.isValid() && cursorBefore.isValid()
        && index.row() == cursorBefore.row() && index.parent() == cursorBefore.parent();
    if (reclicked || selectionModel()->selectedRows() != before) {
        emitSelectionSignals(event->modifiers(), reclicked);
    }
}

void ObjectTreeView::mouseMoveEvent(QMouseEvent* event) {
    auto index = indexAt(event->pos());
    const int col = index.column();
    // Same condition as the click handlers: the cell is only an actionable
    // icon when the corresponding state role is present.
    const bool isIcon = index.isValid() &&
        ((col == ObjectTreeModel::ColumnVisible && !index.data(ObjectTreeModel::IsHiddenRole).isNull()) ||
         (col == ObjectTreeModel::ColumnLocked && !index.data(ObjectTreeModel::IsLockedRole).isNull()));
    const QPersistentModelIndex hot = isIcon ? index : QModelIndex();
    if (hot != _hotIconIndex) {
        const auto prev = _hotIconIndex;
        _hotIconIndex = hot;
        if (prev.isValid()) {
            viewport()->update(visualRect(prev));
        }
        if (hot.isValid()) {
            viewport()->update(visualRect(hot));
        }
    }
    if (!event->buttons()) {
        viewport()->setCursor(isIcon ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    QTreeView::mouseMoveEvent(event);
}

void ObjectTreeView::leaveEvent(QEvent* event) {
    if (_hotIconIndex.isValid()) {
        viewport()->update(visualRect(_hotIconIndex));
        _hotIconIndex = QPersistentModelIndex();
    }
    viewport()->unsetCursor();
    QTreeView::leaveEvent(event);
}

void ObjectTreeView::keyPressEvent(QKeyEvent* event) {
    const auto before = selectionModel()->selectedRows();
    auto guard = _inputEvent.block();
    QTreeView::keyPressEvent(event);
    if (selectionModel()->selectedRows() != before) {
        emitSelectionSignals(event->modifiers());
    }
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
        Q_EMIT objectsSelected({object}, event->modifiers(), false);
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

    if (_selectingProgrammatically.pending() || _modelMutationDepth > 0 || _inputEvent.pending()) return;

    // Selection changes not driven by an input event (programmatic selectAll,
    // model-driven updates) are forwarded with no modifier context.
    emitSelectionSignals();
}

bool ObjectTreeView::pruneVirtualSelection() {
    auto rows = selectionModel()->selectedRows();
    if (rows.size() <= 1) return false;

    QItemSelection virtualRows;
    for (const auto& idx : rows) {
        if (idx.data(ObjectTreeModel::VirtualTypeRole).toInt() !=
            static_cast<int>(VirtualNodeType::None)) {
            virtualRows.select(idx, idx);
        }
    }
    if (virtualRows.isEmpty()) return false;

    auto guard = _pruningVirtuals.block();
    selectionModel()->select(virtualRows,
                             QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    return true;
}

void ObjectTreeView::emitSelectionSignals(Qt::KeyboardModifiers modifiers, bool reselected) {
    // Virtual nodes (DocumentProps, Guides, Grids, ...) may be selected
    // singly but are never part of a multi-selection: drag-select, range
    // select and Select All sweep them in, so strip them before emitting.
    // Outside an input event the deselect re-triggers selectionChanged,
    // which re-enters here and emits with the pruned selection — return
    // early to avoid a duplicate signal.
    if (!_pruningVirtuals.pending() && pruneVirtualSelection() && !_inputEvent.pending()) {
        return;
    }

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
    Q_EMIT objectsSelected(std::move(objects), modifiers, reselected);
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
