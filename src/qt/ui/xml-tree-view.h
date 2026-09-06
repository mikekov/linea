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

#ifndef LINEA_UI_XMLTREEVIEW_H
#define LINEA_UI_XMLTREEVIEW_H

#include <QTreeView>
#include <memory>

QT_BEGIN_NAMESPACE
class QStyledItemDelegate;
QT_END_NAMESPACE

class SPDocument;
class SPDesktop;

namespace Inkscape {
namespace XML {
class Node;
} // namespace XML
} // namespace Inkscape

namespace Linea::UI::Syntax {
class XMLStyles;
} // namespace Linea::UI::Syntax

namespace Linea::UI {

class XmlTreeModel;

/**
 * A QTreeView widget for displaying and editing the XML tree structure.
 *
 * This widget provides a hierarchical view of the XML document with:
 * - Syntax-highlighted node display
 * - Drag and drop for reordering nodes
 * - Selection synchronization with the document
 */
class XmlTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit XmlTreeView(QWidget* parent = nullptr);
    ~XmlTreeView() override;

    // Prevent copying
    XmlTreeView(const XmlTreeView&) = delete;
    XmlTreeView& operator=(const XmlTreeView&) = delete;

    // For context menu
    void setDesktop(SPDesktop* desktop);

    // Build the tree from a document
    void buildTree(SPDocument* document);

    // Get the XML node for the current selection
    Inkscape::XML::Node* selectedNode() const;

    // Select a node in the tree
    void selectNode(Inkscape::XML::Node* node, bool edit = false);

    // Get the XML node at a specific index
    Inkscape::XML::Node* nodeAt(const QModelIndex& index) const;

    // Set the syntax highlighting style
    void setStyle(const Linea::UI::Syntax::XMLStyles& newStyle);

    // Access the model
    XmlTreeModel* xmlModel() const;

Q_SIGNALS:
    void nodeSelected(Inkscape::XML::Node* node);

protected:
    // Drag and drop
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    // Selection handling
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;

    // Context menu
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    SPDesktop* _desktop = nullptr;
    XmlTreeModel* _model = nullptr;
    std::unique_ptr<QStyledItemDelegate> _delegate;
};

} // namespace Linea::UI

#endif // LINEA_UI_XMLTREEVIEW_H
