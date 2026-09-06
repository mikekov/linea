// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt widget combining the XML tree view with the attribute editor.
 */

#include "xml-tree-widget.h"

#include <QAction>
#include <QActionGroup>
#include <QInputDialog>
#include <QMenu>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
#include <functional>
#include <glibmm/i18n.h>

#include "attribute-edit-widget.h"
#include "document-undo.h"
#include "document.h"
#include "object/sp-object.h"
#include "preferences.h"
#include "ui/icon-names.h"
#include "ui_xml-tree-widget.h"
#include "util-string/context-string.h"
#include "xml-tree-view.h"
#include "xml/node.h"
#include "xml/repr.h"

namespace Linea::UI {

namespace {

QString extractTagName(const QString& text) {
    static const QRegularExpression re("^<?\\s*(\\w[\\w:\\-\\d]*)");
    const auto match = re.match(text.trimmed());
    return match.hasMatch() ? match.captured(1) : QString();
}

} // namespace

XmlTreeWidget::XmlTreeWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::XmlTreeWidget>()) {
    _ui->setupUi(this);

    _treeView = new XmlTreeView(this);
    _treeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _treeView->setProperty("class", "widget-frame");
    _ui->treeContainer->layout()->addWidget(_treeView);

    _attrEdit = new AttributeEditWidget(this);
    _attrEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _ui->attrContainer->layout()->addWidget(_attrEdit);

    _ui->splitter->setStretchFactor(0, 2);
    _ui->splitter->setStretchFactor(1, 1);

    setupLayoutMenu();
    restoreLayout();

    connect(_ui->newElementButton, &QToolButton::clicked, this, &XmlTreeWidget::onNewElement);
    connect(_ui->newTextButton, &QToolButton::clicked, this, &XmlTreeWidget::onNewText);
    connect(_ui->duplicateButton, &QToolButton::clicked, this, &XmlTreeWidget::onDuplicateNode);
    connect(_ui->deleteButton, &QToolButton::clicked, this, &XmlTreeWidget::onDeleteNode);
    connect(_ui->unindentButton, &QToolButton::clicked, this, &XmlTreeWidget::onUnindentNode);
    connect(_ui->indentButton, &QToolButton::clicked, this, &XmlTreeWidget::onIndentNode);
    connect(_ui->raiseButton, &QToolButton::clicked, this, &XmlTreeWidget::onRaiseNode);
    connect(_ui->lowerButton, &QToolButton::clicked, this, &XmlTreeWidget::onLowerNode);
    connect(_ui->splitter, &QSplitter::splitterMoved, this, &XmlTreeWidget::savePanePosition);

    connect(_treeView, &XmlTreeView::nodeSelected, this, &XmlTreeWidget::onNodeSelected);

    updateButtons();
}

XmlTreeWidget::~XmlTreeWidget() = default;

void XmlTreeWidget::setDesktop(SPDesktop* desktop) {
    _treeView->setDesktop(desktop);
}

void XmlTreeWidget::setDocument(SPDocument* document) {
    if (document == _document) {
        return;
    }
    _document = document;
    _selectedNode = nullptr;
    _attrEdit->setRepr(nullptr);
    updateButtons();

    buildXmlTree();
}

void XmlTreeWidget::setSelectedNode(Inkscape::XML::Node* node) {
    _treeView->selectNode(node);
}

void XmlTreeWidget::setSyntaxStyle(const Linea::UI::Syntax::XMLStyles& style) {
    _treeView->setStyle(style);
}

void XmlTreeWidget::setFilterText(const QString& text) {
    auto model = _treeView->model();
    if (!model) return;

    auto term = text.trimmed().toLower();

    // Recursively show/hide rows. A row is visible if it matches or any
    // descendant matches. Returns true if this subtree contains a match.
    std::function<bool(QModelIndex)> filterRecursive = [&](QModelIndex parent) -> bool {
        bool anyMatch = false;
        for (int i = 0; i < model->rowCount(parent); ++i) {
            auto idx = model->index(i, 0, parent);
            auto display = model->data(idx, Qt::DisplayRole).toString().toLower();
            bool selfMatch = term.isEmpty() || display.contains(term);

            bool childMatch = filterRecursive(idx);
            bool visible = term.isEmpty() || selfMatch || childMatch;
            _treeView->setRowHidden(idx.row(), parent, !visible);
            if (visible) anyMatch = true;
        }
        return anyMatch;
    };

    filterRecursive(QModelIndex());
}

void XmlTreeWidget::buildXmlTree() {
    _treeView->buildTree(_document);
}

void XmlTreeWidget::onNodeSelected(Inkscape::XML::Node* node) {
    _selectedNode = node;
    _attrEdit->setRepr(node);
    updateButtons();
    Q_EMIT nodeSelected(node);
}

void XmlTreeWidget::updateButtons() {
    auto node = _selectedNode;
    const bool hasNode = node != nullptr;
    const bool isElement = hasNode && node->type() == Inkscape::XML::NodeType::ELEMENT_NODE;
    const bool mutableNode = isMutable(node);
    const bool rootNode = isTreeRoot(node);

    _ui->newElementButton->setEnabled(isElement);
    _ui->newTextButton->setEnabled(isElement);
    _ui->duplicateButton->setEnabled(mutableNode);
    _ui->deleteButton->setEnabled(hasNode && !rootNode && mutableNode);

    // Raise/lower.
    _ui->raiseButton->setEnabled(hasNode && node->parent() && node->parent()->firstChild() != node);
    _ui->lowerButton->setEnabled(hasNode && node->next() != nullptr);

    // Unindent: node is at least three levels deep.
    bool unindentable = false;
    if (hasNode) {
        auto parent = node->parent();
        auto grandparent = parent ? parent->parent() : nullptr;
        if (grandparent && grandparent->parent()) {
            unindentable = true;
        }
    }
    _ui->unindentButton->setEnabled(unindentable);

    // Indent: previous sibling is an element.
    bool indentable = false;
    if (hasNode && mutableNode && node->parent()) {
        auto parent = node->parent();
        if (parent->firstChild() != node) {
            for (auto prev = parent->firstChild(); prev; prev = prev->next()) {
                if (prev->next() == node) {
                    indentable = prev->type() == Inkscape::XML::NodeType::ELEMENT_NODE;
                    break;
                }
            }
        }
    }
    _ui->indentButton->setEnabled(indentable);
}

bool XmlTreeWidget::isMutable(Inkscape::XML::Node* node) const {
    if (!node) {
        return false;
    }
    auto parent = node->parent();
    if (!parent) {
        return false;
    }
    if (parent->parent()) {
        return true;
    }
    // At the base level, protect defs and namedview.
    const char* name = node->name();
    if (name && (strcmp(name, "svg:defs") == 0 || strcmp(name, "sodipodi:namedview") == 0)) {
        return false;
    }
    return true;
}

bool XmlTreeWidget::isTreeRoot(Inkscape::XML::Node* node) const {
    if (!node) {
        return true;
    }
    auto parent = node->parent();
    return !parent || !parent->parent();
}

void XmlTreeWidget::onNewElement() {
    if (!_document || !_selectedNode) {
        return;
    }
    if (_selectedNode->type() != Inkscape::XML::NodeType::ELEMENT_NODE) {
        return;
    }
    auto xmlDoc = _document->getReprDoc();
    if (!xmlDoc) {
        return;
    }

    bool ok = false;
    QString input =
        QInputDialog::getText(this, tr("New Element"), tr("Element name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }

    QString tagName = extractTagName(input);
    if (tagName.isEmpty()) {
        return;
    }
    if (!tagName.contains(':')) {
        tagName = "svg:" + tagName;
    }

    auto newNode = xmlDoc->createElement(tagName.toUtf8().constData());
    _selectedNode->appendChild(newNode);
    Inkscape::GC::release(newNode);
    _treeView->selectNode(newNode);

    Inkscape::DocumentUndo::done(_document, RC_("Undo/XML Editor", "Create new element node"),
                                 Glib::ustring(INKSCAPE_ICON("dialog-xml-editor")));
}

void XmlTreeWidget::onNewText() {
    if (!_document || !_selectedNode) {
        return;
    }
    if (_selectedNode->type() != Inkscape::XML::NodeType::ELEMENT_NODE) {
        return;
    }
    auto xmlDoc = _document->getReprDoc();
    if (!xmlDoc) {
        return;
    }

    auto textNode = xmlDoc->createTextNode("");
    _selectedNode->appendChild(textNode);
    Inkscape::GC::release(textNode);
    _treeView->selectNode(textNode);

    Inkscape::DocumentUndo::done(_document, RC_("Undo/XML Editor", "Create new text node"),
                                 Glib::ustring(INKSCAPE_ICON("dialog-xml-editor")));
}

void XmlTreeWidget::onDuplicateNode() {
    if (!_document || !_selectedNode) {
        return;
    }
    auto parent = _selectedNode->parent();
    if (!parent) {
        return;
    }
    auto xmlDoc = _document->getReprDoc();
    if (!xmlDoc) {
        return;
    }

    auto dup = _selectedNode->duplicate(xmlDoc);
    parent->addChild(dup, _selectedNode);
    Inkscape::GC::release(dup);
    _treeView->selectNode(dup);

    Inkscape::DocumentUndo::done(_document, RC_("Undo/XML Editor", "Duplicate node"),
                                 Glib::ustring(INKSCAPE_ICON("dialog-xml-editor")));
}

void XmlTreeWidget::onDeleteNode() {
    if (!_document || !_selectedNode) {
        return;
    }
    if (isTreeRoot(_selectedNode)) {
        return;
    }

    auto parent = _selectedNode->parent();
    sp_repr_unparent(_selectedNode);
    _selectedNode = nullptr;

    if (parent) {
        if (auto parentObject = _document->getObjectByRepr(parent)) {
            parentObject->requestDisplayUpdate(SP_OBJECT_CHILD_MODIFIED_FLAG);
        }
    }

    Inkscape::DocumentUndo::done(_document, RC_("Undo/XML Editor", "Delete node"),
                                 Glib::ustring(INKSCAPE_ICON("dialog-xml-editor")));
}

void XmlTreeWidget::onRaiseNode() {
    if (!_document || !_selectedNode) {
        return;
    }
    auto parent = _selectedNode->parent();
    if (!parent || parent->firstChild() == _selectedNode) {
        return;
    }

    Inkscape::XML::Node* ref = nullptr;
    for (auto before = parent->firstChild(); before; before = before->next()) {
        if (before->next() == _selectedNode) {
            ref = before;
            break;
        }
    }

    parent->changeOrder(_selectedNode, ref);
    _treeView->selectNode(_selectedNode);

    Inkscape::DocumentUndo::done(_document, RC_("Undo/XML Editor", "Raise node"),
                                 Glib::ustring(INKSCAPE_ICON("dialog-xml-editor")));
}

void XmlTreeWidget::onLowerNode() {
    if (!_document || !_selectedNode) {
        return;
    }
    if (!_selectedNode->next()) {
        return;
    }
    auto parent = _selectedNode->parent();
    if (!parent) {
        return;
    }

    parent->changeOrder(_selectedNode, _selectedNode->next());
    _treeView->selectNode(_selectedNode);

    Inkscape::DocumentUndo::done(_document, RC_("Undo/XML Editor", "Lower node"),
                                 Glib::ustring(INKSCAPE_ICON("dialog-xml-editor")));
}

void XmlTreeWidget::onIndentNode() {
    if (!_document || !_selectedNode) {
        return;
    }
    auto parent = _selectedNode->parent();
    if (!parent || parent->firstChild() == _selectedNode) {
        return;
    }

    Inkscape::XML::Node* prev = nullptr;
    for (auto it = parent->firstChild(); it; it = it->next()) {
        if (it->next() == _selectedNode) {
            prev = it;
            break;
        }
    }
    if (!prev || prev->type() != Inkscape::XML::NodeType::ELEMENT_NODE) {
        return;
    }

    Inkscape::XML::Node* ref = nullptr;
    if (prev->firstChild()) {
        for (ref = prev->firstChild(); ref->next(); ref = ref->next()) {
        }
    }

    parent->removeChild(_selectedNode);
    prev->addChild(_selectedNode, ref);
    _treeView->selectNode(_selectedNode);

    Inkscape::DocumentUndo::done(_document, RC_("Undo/XML Editor", "Indent node"),
                                 Glib::ustring(INKSCAPE_ICON("dialog-xml-editor")));
}

void XmlTreeWidget::onUnindentNode() {
    if (!_document || !_selectedNode) {
        return;
    }
    auto parent = _selectedNode->parent();
    if (!parent) {
        return;
    }
    auto grandparent = parent->parent();
    if (!grandparent) {
        return;
    }

    parent->removeChild(_selectedNode);
    grandparent->addChild(_selectedNode, parent);
    _treeView->selectNode(_selectedNode);

    Inkscape::DocumentUndo::done(_document, RC_("Undo/XML Editor", "Unindent node"),
                                 Glib::ustring(INKSCAPE_ICON("dialog-xml-editor")));
}

void XmlTreeWidget::setupLayoutMenu() {
    _layoutMenu = new QMenu(this);
    _layoutGroup = new QActionGroup(this);

    auto autoAction = _layoutMenu->addAction(tr("Automatic Layout"));
    auto horizontalAction = _layoutMenu->addAction(tr("Horizontal Layout"));
    auto verticalAction = _layoutMenu->addAction(tr("Vertical Layout"));

    for (auto action : {autoAction, horizontalAction, verticalAction}) {
        action->setCheckable(true);
        _layoutGroup->addAction(action);
    }

    connect(_layoutMenu, &QMenu::triggered, this, &XmlTreeWidget::onLayoutChanged);
    connect(_ui->layoutButton, &QToolButton::clicked, this, [this]() {
        _layoutMenu->popup(_ui->layoutButton->mapToGlobal(QPoint(0, _ui->layoutButton->height())));
    });
}

void XmlTreeWidget::onLayoutChanged(QAction* action) {
    const int index = _layoutGroup->actions().indexOf(action);
    if (index >= 0) {
        setLayout(static_cast<DialogLayout>(index));
    }
}

void XmlTreeWidget::setLayout(DialogLayout layout) {
    _layout = layout;
    saveLayout();
    applyLayout();
    updateLayoutIcon();
}

void XmlTreeWidget::applyLayout() {
    if (_layout == DialogLayout::Auto) {
        autoLayout();
    } else {
        _ui->splitter->setOrientation(_layout == DialogLayout::Vertical ? Qt::Vertical : Qt::Horizontal);
    }
}

void XmlTreeWidget::autoLayout() {
    if (width() < 10 || height() < 10) {
        return;
    }
    // Same heuristic as the GTK dialog: use vertical layout when the widget
    // is narrower than 1.5 times its minimum natural width.
    const bool narrow = width() < _minWidth * 1.5;
    _ui->splitter->setOrientation(narrow ? Qt::Vertical : Qt::Horizontal);
}

void XmlTreeWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (_minWidth == 0) {
        _minWidth = minimumSizeHint().width();
    }
    if (_layout == DialogLayout::Auto) {
        autoLayout();
    }
}

void XmlTreeWidget::updateLayoutIcon() {
    const char* icon = "layout-auto";
    switch (_layout) {
        case DialogLayout::Horizontal:
            icon = "layout-horizontal";
            break;
        case DialogLayout::Vertical:
            icon = "layout-vertical";
            break;
        default:
            break;
    }
    _ui->layoutButton->setIcon(QIcon(QString::fromUtf8(":/icons/") + icon));

    const auto actions = _layoutGroup->actions();
    const int index = static_cast<int>(_layout);
    if (index >= 0 && index < actions.size()) {
        actions[index]->setChecked(true);
    }
}

void XmlTreeWidget::saveLayout() {
    auto prefs = Inkscape::Preferences::get();
    prefs->setInt("/dialogs/xml/layout", static_cast<int>(_layout));
}

void XmlTreeWidget::savePanePosition() {
    auto prefs = Inkscape::Preferences::get();
    const int pos = _ui->splitter->sizes().value(0);
    prefs->setInt("/dialogs/xml/panedpos", pos);
}

void XmlTreeWidget::restoreLayout() {
    auto prefs = Inkscape::Preferences::get();

    _savedPanePosition = prefs->getInt("/dialogs/xml/panedpos", 200);
/* fixed layout
    const int layout =
        prefs->getIntLimited("/dialogs/xml/layout", static_cast<int>(DialogLayout::Auto),
                             static_cast<int>(DialogLayout::Auto), static_cast<int>(DialogLayout::Vertical));
    _layout = static_cast<DialogLayout>(layout);
*/
    applyLayout();
    updateLayoutIcon();
}

void XmlTreeWidget::restorePanePosition() {
    const bool vertical = _ui->splitter->orientation() == Qt::Vertical;
    const int total = vertical ? _ui->splitter->height() : _ui->splitter->width();
    if (total <= 0) {
        return;
    }
    const int pos = std::min(_savedPanePosition, total);
    const int second = std::max(0, total - pos);
    _ui->splitter->setSizes({pos, second});
}

void XmlTreeWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!_panePositionRestored) {
        _panePositionRestored = true;
        QTimer::singleShot(0, this, &XmlTreeWidget::restorePanePosition);
    }
}

} // namespace Linea::UI
