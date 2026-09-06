// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt widget combining the XML tree view with the attribute editor.
 */

#ifndef LINEA_UI_XML_TREE_WIDGET_H
#define LINEA_UI_XML_TREE_WIDGET_H

#include <memory>
#include <QWidget>

#include "syntax.h"
#include "ui/operation-blocker.h"

QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QMenu;
QT_END_NAMESPACE

class SPDocument;
class SPDesktop;

namespace Inkscape::XML {
class Node;
}

namespace Ui {
class XmlTreeWidget;
}

namespace Linea::UI {

class XmlTreeView;
class AttributeEditWidget;

/**
 * A widget that shows an XML tree alongside the attribute editor.
 *
 * Modeled after the GTK XML Editor dialog (src/ui/dialog/xml-tree.h).
 */
class XmlTreeWidget : public QWidget {
    Q_OBJECT

public:
    enum class DialogLayout : int { Auto = 0, Horizontal = 1, Vertical = 2 };

    explicit XmlTreeWidget(QWidget* parent = nullptr);
    ~XmlTreeWidget() override;

    // for context menu
    void setDesktop(SPDesktop* desktop);

    void setDocument(SPDocument* document);
    SPDocument* document() const { return _document; }

    void setSelectedNode(Inkscape::XML::Node* node);
    Inkscape::XML::Node* selectedNode() const { return _selectedNode; }

    void setSyntaxStyle(const Linea::UI::Syntax::XMLStyles& style);

    /// Filter tree rows by text (case-insensitive match on node name/content).
    void setFilterText(const QString& text);

Q_SIGNALS:
    void nodeSelected(Inkscape::XML::Node* node);

private Q_SLOTS:
    void onNodeSelected(Inkscape::XML::Node* node);
    void onNewElement();
    void onNewText();
    void onDuplicateNode();
    void onDeleteNode();
    void onRaiseNode();
    void onLowerNode();
    void onIndentNode();
    void onUnindentNode();
    void onLayoutChanged(QAction* action);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void updateButtons();
    void updateLayoutIcon();
    void setupLayoutMenu();
    void setLayout(DialogLayout layout);
    void applyLayout();
    void autoLayout();
    void saveLayout();
    void savePanePosition();
    void restoreLayout();
    void restorePanePosition();
    bool isMutable(Inkscape::XML::Node* node) const;
    bool isTreeRoot(Inkscape::XML::Node* node) const;
    void buildXmlTree();

    SPDocument* _document = nullptr;
    XmlTreeView* _treeView = nullptr;
    AttributeEditWidget* _attrEdit = nullptr;
    QMenu* _layoutMenu = nullptr;
    QActionGroup* _layoutGroup = nullptr;
    std::unique_ptr<Ui::XmlTreeWidget> _ui;
    OperationBlocker _update;
    Inkscape::XML::Node* _selectedNode = nullptr;
    DialogLayout _layout = DialogLayout::Vertical;
    int _minWidth = 0;
    int _savedPanePosition = 200;
    bool _panePositionRestored = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_XML_TREE_WIDGET_H
