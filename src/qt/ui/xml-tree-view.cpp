// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A Qt widget showing the XML tree.
 *
 * Authors:
 *   Mike Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "xml-tree-view.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QStyledItemDelegate>

#include "ui/contextmenu.h"
#include "document.h"
#include "desktop.h"
#include "object/sp-item.h"
#include "selection.h"
#include "syntax.h"
#include "xml-tree-model.h"
#include "xml/node.h"

namespace Linea::UI {

/**
 * Custom delegate for rendering XML tree items with syntax highlighting.
 */
class XmlTreeDelegate : public QStyledItemDelegate {
public:
    explicit XmlTreeDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        // Get the plain text for the item
        QString text = index.data(Qt::DisplayRole).toString();

        // Check if selected
        bool selected = option.state & QStyle::State_Selected;

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Use plain text when selected for better readability
        if (selected) {
            opt.text = text;
        }

        // Let the base class do the actual painting
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

XmlTreeView::XmlTreeView(QWidget* parent)
    : QTreeView(parent) {
    setObjectName("XmlTreeView");

    // Create and set the model
    _model = new XmlTreeModel(this);
    setModel(_model);

    // Set up the delegate for custom rendering
    _delegate = std::make_unique<XmlTreeDelegate>(this);
    setItemDelegate(_delegate.get());

    // Configure tree view appearance
    setFrameShape(QFrame::NoFrame);
    setHeaderHidden(true);
    setUniformRowHeights(true);
    setAnimated(true);
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);

    // Enable tooltips
    setMouseTracking(true);
}

XmlTreeView::~XmlTreeView() = default;

void XmlTreeView::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void XmlTreeView::buildTree(SPDocument* document) {
    _model->buildTree(document);
    if (document) {
        expandToDepth(0);
    }
}

Inkscape::XML::Node* XmlTreeView::selectedNode() const {
    auto index = currentIndex();
    if (!index.isValid()) {
        return nullptr;
    }
    return _model->nodeForIndex(index);
}

void XmlTreeView::selectNode(Inkscape::XML::Node* node, bool edit) {
    _model->selectNode(this, node, edit);
}

Inkscape::XML::Node* XmlTreeView::nodeAt(const QModelIndex& index) const {
    return _model->nodeForIndex(index);
}

void XmlTreeView::setStyle(const Linea::UI::Syntax::XMLStyles& /*newStyle*/) {
    // TODO: Store the style and update the delegate to use it for syntax highlighting
    // For now, the delegate uses basic text rendering
    viewport()->update();
}

XmlTreeModel* XmlTreeView::xmlModel() const {
    return _model;
}

void XmlTreeView::startDrag(Qt::DropActions supportedActions) {
    auto index = currentIndex();
    if (!index.isValid()) {
        return;
    }

    // Check if we can drag this node
    auto node = nodeAt(index);
    if (!node) {
        return;
    }

    // Don't drag root or special nodes
    if (!index.parent().isValid()) {
        return;
    }

    static const GQuark CODE_sodipodi_namedview = g_quark_from_static_string("sodipodi:namedview");
    static const GQuark CODE_svg_defs = g_quark_from_static_string("svg:defs");

    if (node->code() == CODE_sodipodi_namedview || node->code() == CODE_svg_defs) {
        return;
    }

    QTreeView::startDrag(supportedActions);
}

void XmlTreeView::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-inkscape-xmlnode")) {
        event->ignore();
        return;
    }
    QTreeView::dragEnterEvent(event);
}

void XmlTreeView::dragMoveEvent(QDragMoveEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-inkscape-xmlnode")) {
        event->ignore();
        return;
    }

    // Check if the target is a valid drop location
    auto index = indexAt(event->position().toPoint());
    if (index.isValid()) {
        auto node = nodeAt(index);
        if (node && node->type() != Inkscape::XML::NodeType::ELEMENT_NODE) {
            // Only element nodes can have children
            event->ignore();
            return;
        }
    }

    QTreeView::dragMoveEvent(event);
}

void XmlTreeView::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-inkscape-xmlnode")) {
        event->ignore();
        return;
    }

    QTreeView::dropEvent(event);
}

void XmlTreeView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    QTreeView::selectionChanged(selected, deselected);

    // Emit signal with the selected node
    auto indexes = selected.indexes();
    if (!indexes.isEmpty()) {
        auto node = nodeAt(indexes.first());
        Q_EMIT nodeSelected(node);
    } else {
        Q_EMIT nodeSelected(nullptr);
    }
}

void XmlTreeView::contextMenuEvent(QContextMenuEvent* event) {
    if (!_desktop || !_desktop->getDocument()) {
        QTreeView::contextMenuEvent(event);
        return;
    }

    auto index = indexAt(event->pos());
    if (!index.isValid()) {
        QTreeView::contextMenuEvent(event);
        return;
    }

    auto node = nodeAt(index);
    if (!node) {
        QTreeView::contextMenuEvent(event);
        return;
    }

    // Map the XML node to its SPObject; ContextMenu operates on objects, not raw XML nodes.
    auto object = _desktop->getDocument()->getObjectByRepr(node);
    if (!object) {
        QTreeView::contextMenuEvent(event);
        return;
    }

    auto item = cast<SPItem>(object);
    auto selection = _desktop->getSelection();

    // Select the clicked object if not already selected (matching ObjectTreeView behavior).
    if (item && !selection->includes(object)) {
        selection->set(object);
    }

    std::vector<SPItem*> items;
    if (item) items.push_back(item);

    auto menu = new ContextMenu(_desktop, object, items);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}

} // namespace Linea::UI
