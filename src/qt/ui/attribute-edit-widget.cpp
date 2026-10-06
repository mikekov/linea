// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt widget for editing XML attributes.
 */

#include "attribute-edit-widget.h"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <cstring>
#include <glib.h>

#include "xml/node.h"
#include "widget-utils.h"

namespace Linea::UI {

namespace {

bool isTextOrCommentNode(const Inkscape::XML::Node& node) {
    const auto t = node.type();
    return t == Inkscape::XML::NodeType::TEXT_NODE || t == Inkscape::XML::NodeType::COMMENT_NODE;
}

Linea::UI::Syntax::SyntaxMode modeForAttribute(const QString& name) {
    if (name == "style") {
        return Linea::UI::Syntax::SyntaxMode::InlineCss;
    }
    if (name == "d" || name == "inkscape:original-d") {
        return Linea::UI::Syntax::SyntaxMode::SvgPathData;
    }
    if (name == "points") {
        return Linea::UI::Syntax::SyntaxMode::SvgPolyPoints;
    }
    return Linea::UI::Syntax::SyntaxMode::PlainText;
}

// Content of a text node under <svg:style> is CSS, under <svg:script> is
// JavaScript (as in upstream's AttrDialog); everything else is plain text.
Linea::UI::Syntax::SyntaxMode modeForContent(const Inkscape::XML::Node& node) {
    const auto parent = node.parent();
    if (!node.name() || std::strcmp(node.name(), "string") != 0 || !parent || !parent->name()) {
        return Linea::UI::Syntax::SyntaxMode::PlainText;
    }
    if (std::strcmp(parent->name(), "svg:style") == 0) {
        return Linea::UI::Syntax::SyntaxMode::CssStyle;
    }
    if (std::strcmp(parent->name(), "svg:script") == 0) {
        return Linea::UI::Syntax::SyntaxMode::JavaScript;
    }
    return Linea::UI::Syntax::SyntaxMode::PlainText;
}

// Remove any widgets from the layout and put the given one in their place.
// The previous widget is not deleted: its owner (e.g. a TextEditView) does that.
void replaceLayoutWidget(QVBoxLayout& layout, QWidget& widget) {
    while (QLayoutItem* item = layout.takeAt(0)) {
        delete item;
    }
    layout.addWidget(&widget, 1);
}

// Structured or multiline values need the popup editor; everything else is
// edited in place, like upstream's AttrDialog.
bool needsPopupEditor(const QString& attrName, const QString& value) {
    return modeForAttribute(attrName) != Linea::UI::Syntax::SyntaxMode::PlainText ||
           value.contains(QLatin1Char('\n'));
}

} // namespace

// ---------------------------------------------------------------------------
// AttributeEditPopup
// ---------------------------------------------------------------------------

AttributeEditPopup::AttributeEditPopup(QWidget* parent)
    : QDialog(parent) {
    setupUi();
}

AttributeEditPopup::~AttributeEditPopup() = default;

void AttributeEditPopup::setupUi() {
    setWindowFlags(Qt::Popup);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    _editorLayout = new QVBoxLayout;
    _editorLayout->setSpacing(0);
    _editorLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(_editorLayout, 1);

    auto buttonLayout = new QHBoxLayout;
    auto info = new QLabel("Shift+Return to close");
    info->setProperty("class", "info-text");
    buttonLayout->addWidget(info);
    buttonLayout->addStretch();
    _cancelButton = new QPushButton(tr("Cancel"), this);
    _okButton = new QPushButton(tr("OK"), this);
    _okButton->setDefault(true);
    buttonLayout->addWidget(_cancelButton);
    buttonLayout->addWidget(_okButton);
    mainLayout->addLayout(buttonLayout);

    connect(_okButton, &QPushButton::clicked, this, &AttributeEditPopup::onOk);
    connect(_cancelButton, &QPushButton::clicked, this, &AttributeEditPopup::onCancel);

    setMinimumSize(200, 120);
}

void AttributeEditPopup::edit(const QString& attrName, const QString& value, const QPoint& pos) {
    if (_editor) {
        _editor->getEditor().removeEventFilter(this);
    }

    auto mode = modeForAttribute(attrName);
    _editor = Linea::UI::Syntax::TextEditView::create(mode);
    _editor->setStyle(QString());
    _editor->setText(value);
    if (_monoFont) {
        Syntax::setMonoFont(_editor->getEditor(), true);
    }

    // Replace any previous editor widget in the layout.
    replaceLayoutWidget(*_editorLayout, _editor->getEditor());
    _editor->getEditor().installEventFilter(this);

    // Position and size the popup near the attribute value cell, keeping it
    // fully on screen.
    QWidget* anchor = parentWidget() ? parentWidget() : this;
    int width = std::min(600, std::max(300, anchor->width() - 20));
    resize(width, 200);
    QPoint popupPos = pos;
    ensurePopupOnScreen(popupPos, size());
    move(popupPos);

    show();
    raise();
    _editor->getEditor().setFocus();
}

QString AttributeEditPopup::value() const {
    return _editor ? _editor->getText() : QString();
}

void AttributeEditPopup::setMonoFont(bool enabled) {
    _monoFont = enabled;
    if (_editor) {
        Syntax::setMonoFont(_editor->getEditor(), enabled);
    }
}

void AttributeEditPopup::onOk() {
    accept();
}

void AttributeEditPopup::onCancel() {
    reject();
}

bool AttributeEditPopup::eventFilter(QObject* watched, QEvent* event) {
    if (_editor && watched == &_editor->getEditor() && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
            (keyEvent->modifiers() & Qt::ShiftModifier)) {
            accept();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void AttributeEditPopup::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    const int width = std::max(_okButton->sizeHint().width(), _cancelButton->sizeHint().width()) + 20;
    _okButton->setMinimumWidth(width);
    _cancelButton->setMinimumWidth(width);
}

// ---------------------------------------------------------------------------
// AttributeEditWidget
// ---------------------------------------------------------------------------

AttributeEditWidget::AttributeEditWidget(QWidget* parent)
    : QWidget(parent) {
    auto outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    _stack = new QStackedWidget(this);
    outerLayout->addWidget(_stack);

    // Page 0: attribute list.
    _treePage = new QWidget(this);
    auto treeLayout = new QVBoxLayout(_treePage);
    treeLayout->setContentsMargins(0, 0, 0, 0);
    treeLayout->setSpacing(4);

    auto toolbar = new QHBoxLayout;
    toolbar->setSpacing(4);
    _addButton = new QToolButton(this);
    _addButton->setText(tr("+"));
    _addButton->setToolTip(tr("Add a new attribute"));
    _deleteButton = new QToolButton(this);
    _deleteButton->setText(tr("-"));
    _deleteButton->setToolTip(tr("Delete selected attribute"));
    toolbar->addWidget(_addButton);
    toolbar->addWidget(_deleteButton);
    toolbar->addStretch();
    treeLayout->addLayout(toolbar);

    _treeView = new QTreeView(this);
    _treeView->setRootIsDecorated(false);
    _treeView->setAlternatingRowColors(true);
    _treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    _treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    _treeView->setUniformRowHeights(true);
    _treeView->setProperty("class", "styled-header");
    treeLayout->addWidget(_treeView);

    _attrDelegate = new Syntax::FixedFontDelegate(_treeView);
    _treeView->setItemDelegate(_attrDelegate);

    _model = new QStandardItemModel(0, 2, this);
    _model->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
    _treeView->setModel(_model);
    _treeView->setSortingEnabled(true);
    _treeView->header()->setStretchLastSection(true);
    _treeView->header()->setSectionResizeMode(0, QHeaderView::Interactive);

    _stack->addWidget(_treePage);

    // Page 1: text/comment content; the editor view is created on demand by
    // ensureContentView() so it can match the node's syntax mode.
    _contentPage = new QWidget(this);
    _contentLayout = new QVBoxLayout(_contentPage);
    _contentLayout->setContentsMargins(0, 0, 0, 0);
    _stack->addWidget(_contentPage);

    connect(_addButton, &QToolButton::clicked, this, &AttributeEditWidget::onAddAttribute);
    connect(_deleteButton, &QToolButton::clicked, this, &AttributeEditWidget::onDeleteAttribute);
    connect(_treeView, &QTreeView::doubleClicked, this, &AttributeEditWidget::onEditValue);
    connect(_model, &QStandardItemModel::itemChanged, this, &AttributeEditWidget::onItemEdited);

    _treeView->installEventFilter(this);
    _treeView->setFocus();

    _popup = std::make_unique<AttributeEditPopup>(this);
    connect(_popup.get(), &QDialog::accepted, this, &AttributeEditWidget::onPopupAccepted);
    connect(_popup.get(), &QDialog::rejected, this, &AttributeEditWidget::onPopupRejected);
}

AttributeEditWidget::~AttributeEditWidget() {
    setRepr(nullptr);
}

void AttributeEditWidget::ensureContentView(Syntax::SyntaxMode mode) {
    if (_contentView && _contentMode == mode) {
        return;
    }
    _contentMode = mode;
    _contentView = Syntax::TextEditView::create(mode);
    replaceLayoutWidget(*_contentLayout, _contentView->getEditor());
    if (_monoFont) {
        Syntax::setMonoFont(_contentView->getEditor(), true);
    }
    connect(&_contentView->getEditor(), &QPlainTextEdit::textChanged, this, [this]() {
        if (!_repr || _update.pending()) {
            return;
        }
        auto scoped = _update.block();
        _repr->setContent(_contentView->getText().toUtf8().constData());
    });
}

void AttributeEditWidget::setMonoFont(bool enabled) {
    const QFont font = enabled ? Syntax::fixedFont(_treeView->font()) : QFont();
    _treeView->setFont(font);
    // The view's font can be reverted when the application style sheet is
    // re-applied (the view matches stylesheet rules with font properties),
    // so the delegate carries the display font as well.
    if (enabled) {
        _attrDelegate->setFixedFont(font);
    } else {
        _attrDelegate->clearFixedFont();
    }
    _monoFont = enabled;
    if (_contentView) {
        Syntax::setMonoFont(_contentView->getEditor(), enabled);
    }
    _popup->setMonoFont(enabled);
}

void AttributeEditWidget::setRepr(Inkscape::XML::Node* repr) {
    if (repr == _repr) {
        return;
    }
    if (_repr) {
        _repr->removeObserver(*this);
        Inkscape::GC::release(_repr);
    }
    _repr = repr;
    if (_repr) {
        Inkscape::GC::anchor(_repr);
        _repr->addObserver(*this);
    }

    if (_repr && isTextOrCommentNode(*_repr)) {
        ensureContentView(modeForContent(*_repr));
        {
            auto scoped = _update.block();
            _contentView->setText(QString::fromUtf8(_repr->content() ? _repr->content() : ""));
        }
        _stack->setCurrentWidget(_contentPage);
    } else {
        _stack->setCurrentWidget(_treePage);
        buildAttributeList();
    }
}

void AttributeEditWidget::buildAttributeList() {
    const int sortColumn = _treeView->header()->sortIndicatorSection();
    const auto sortOrder = _treeView->header()->sortIndicatorOrder();
    {
        auto scoped = _update.block();
        _model->clear();
        _model->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
    }
    if (_repr) {
        _repr->synthesizeEvents(*this);
    }
    if (sortColumn >= 0) {
        _treeView->sortByColumn(sortColumn, sortOrder);
    }
}

void AttributeEditWidget::onAddAttribute() {
    if (!_repr) {
        return;
    }
    auto scoped = _update.block();
    auto nameItem = new QStandardItem("");
    nameItem->setEditable(true);
    nameItem->setData(QString(), Qt::UserRole);
    auto valueItem = new QStandardItem("");
    valueItem->setEditable(!needsPopupEditor(QString(), QString()));
    _model->appendRow({nameItem, valueItem});
    const QModelIndex index = _model->index(_model->rowCount() - 1, 0);
    _treeView->setCurrentIndex(index);
    _treeView->edit(index);
}

void AttributeEditWidget::onDeleteAttribute() {
    if (!_repr) {
        return;
    }
    const QModelIndex index = _treeView->currentIndex();
    if (!index.isValid()) {
        return;
    }
    const QString name = _model->item(index.row(), 0)->text();
    auto scoped = _update.block();
    _model->removeRow(index.row());
    _repr->removeAttribute(name.toUtf8().constData());
}

void AttributeEditWidget::onEditValue(const QModelIndex& index) {
    if (!_repr || !index.isValid() || index.column() != 1) {
        return;
    }
    QStandardItem* nameItem = _model->item(index.row(), 0);
    QStandardItem* valueItem = _model->item(index.row(), 1);
    if (!nameItem || !valueItem) {
        return;
    }
    if (valueItem->isEditable()) {
        _treeView->edit(index);
        return;
    }
    _editingIndex = index;
    const QRect rect = _treeView->visualRect(index);
    const QPoint pos = _treeView->viewport()->mapToGlobal(rect.topLeft());
    _popup->edit(nameItem->text(), valueItem->text(), pos);
}

void AttributeEditWidget::onItemEdited(QStandardItem* item) {
    if (!_repr || _update.pending()) {
        return;
    }
    if (item->column() == 1) {
        const QString name = _model->item(item->row(), 0)->text();
        // Rows without a name yet keep the value until the name is set.
        if (name.isEmpty()) {
            return;
        }
        auto scoped = _update.block();
        _repr->setAttributeOrRemoveIfEmpty(name.toUtf8().constData(), item->text().toUtf8().constData());
        return;
    }
    const QString newName = item->text();
    const QString oldName = item->data(Qt::UserRole).toString();

    if (newName == oldName) {
        return;
    }
    if (newName.isEmpty() || std::any_of(newName.begin(), newName.end(), [](QChar c) { return c.isSpace(); })) {
        item->setText(oldName);
        return;
    }
    for (int r = 0; r < _model->rowCount(); ++r) {
        if (r == item->row()) {
            continue;
        }
        if (_model->item(r, 0)->text() == newName) {
            item->setText(oldName);
            return;
        }
    }

    const QString value = _model->item(item->row(), 1)->text();
    auto scoped = _update.block();
    if (!oldName.isEmpty()) {
        _repr->removeAttribute(oldName.toUtf8().constData());
    }
    _repr->setAttributeOrRemoveIfEmpty(newName.toUtf8().constData(), value.toUtf8().constData());
    item->setData(newName, Qt::UserRole);
    // The editor mode depends on the attribute name.
    _model->item(item->row(), 1)->setEditable(!needsPopupEditor(newName, value));
}

void AttributeEditWidget::onPopupAccepted() {
    if (!_editingIndex.isValid() || !_repr) {
        _editingIndex = QModelIndex();
        return;
    }
    QStandardItem* nameItem = _model->item(_editingIndex.row(), 0);
    QStandardItem* valueItem = _model->item(_editingIndex.row(), 1);
    if (!nameItem || !valueItem) {
        _editingIndex = QModelIndex();
        return;
    }
    const QString newValue = _popup->value();
    const QString name = nameItem->text();
    auto scoped = _update.block();
    valueItem->setText(newValue);
    valueItem->setEditable(!needsPopupEditor(name, newValue));
    _repr->setAttributeOrRemoveIfEmpty(name.toUtf8().constData(), newValue.toUtf8().constData());
    _editingIndex = QModelIndex();
}

void AttributeEditWidget::onPopupRejected() {
    _editingIndex = QModelIndex();
}

bool AttributeEditWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _treeView && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        const QModelIndex current = _treeView->currentIndex();
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) && current.isValid() &&
            current.column() == 1) {
            onEditValue(current);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Delete && keyEvent->modifiers() == Qt::NoModifier) {
            onDeleteAttribute();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Plus || keyEvent->key() == Qt::Key_Insert) {
            onAddAttribute();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AttributeEditWidget::notifyAttributeChanged(Inkscape::XML::Node& /*node*/, GQuark name,
                                                 Inkscape::Util::ptr_shared /*old_value*/,
                                                 Inkscape::Util::ptr_shared new_value) {
    if (_update.pending() || !_repr) {
        return;
    }
    const QString attrName = QString::fromUtf8(g_quark_to_string(name));
    const QString value = new_value ? QString::fromUtf8(new_value.pointer()) : QString();

    int row = -1;
    for (int r = 0; r < _model->rowCount(); ++r) {
        if (_model->item(r, 0)->text() == attrName) {
            row = r;
            break;
        }
    }

    auto scoped = _update.block();
    if (new_value) {
        if (row >= 0) {
            _model->item(row, 1)->setText(value);
            _model->item(row, 1)->setEditable(!needsPopupEditor(attrName, value));
        } else {
            auto nameItem = new QStandardItem(attrName);
            nameItem->setEditable(true);
            nameItem->setData(attrName, Qt::UserRole);
            auto valueItem = new QStandardItem(value);
            valueItem->setEditable(!needsPopupEditor(attrName, value));
            _model->appendRow({nameItem, valueItem});
        }
    } else if (row >= 0) {
        _model->removeRow(row);
    }
}

void AttributeEditWidget::notifyContentChanged(Inkscape::XML::Node& /*node*/,
                                               Inkscape::Util::ptr_shared /*old_content*/,
                                               Inkscape::Util::ptr_shared new_content) {
    if (!_contentView || !_repr || _update.pending()) {
        return;
    }
    auto& editor = _contentView->getEditor();
    if (!editor.document()->isModified()) {
        const QString text = new_content ? QString::fromUtf8(new_content.pointer()) : QString();
        auto scoped = _update.block();
        _contentView->setText(text);
    }
    editor.document()->setModified(false);
}

} // namespace Linea::UI
