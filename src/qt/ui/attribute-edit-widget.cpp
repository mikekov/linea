// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt widget for editing XML attributes.
 */

#include "attribute-edit-widget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <glib.h>

#include "xml/node.h"

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

    // Replace any previous editor widget in the layout.
    while (_editorLayout->count() > 0) {
        QLayoutItem* item = _editorLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    _editorLayout->addWidget(&_editor->getEditor(), 1);
    _editor->getEditor().installEventFilter(this);

    // Position and size the popup near the attribute value cell.
    QWidget* anchor = parentWidget() ? parentWidget() : this;
    int width = std::min(600, std::max(300, anchor->width() - 20));
    resize(width, 200);
    move(pos);

    show();
    raise();
    _editor->getEditor().setFocus();
}

QString AttributeEditPopup::value() const {
    return _editor ? _editor->getText() : QString();
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

    _model = new QStandardItemModel(0, 2, this);
    _model->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
    _treeView->setModel(_model);
    _treeView->setSortingEnabled(true);
    _treeView->header()->setStretchLastSection(true);
    _treeView->header()->setSectionResizeMode(0, QHeaderView::Interactive);

    _stack->addWidget(_treePage);

    // Page 1: text/comment content.
    _contentEdit = new QPlainTextEdit(this);
    _contentEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    _stack->addWidget(_contentEdit);

    connect(_addButton, &QToolButton::clicked, this, &AttributeEditWidget::onAddAttribute);
    connect(_deleteButton, &QToolButton::clicked, this, &AttributeEditWidget::onDeleteAttribute);
    connect(_treeView, &QTreeView::doubleClicked, this, &AttributeEditWidget::onEditValue);
    connect(_model, &QStandardItemModel::itemChanged, this, &AttributeEditWidget::onNameEdited);
    connect(_contentEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (!_repr || _update.pending()) {
            return;
        }
        auto scoped = _update.block();
        _repr->setContent(_contentEdit->toPlainText().toUtf8().constData());
    });

    _treeView->installEventFilter(this);
    _treeView->setFocus();

    _popup = std::make_unique<AttributeEditPopup>(this);
    connect(_popup.get(), &QDialog::accepted, this, &AttributeEditWidget::onPopupAccepted);
    connect(_popup.get(), &QDialog::rejected, this, &AttributeEditWidget::onPopupRejected);
}

AttributeEditWidget::~AttributeEditWidget() {
    setRepr(nullptr);
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
        _stack->setCurrentWidget(_contentEdit);
        _contentEdit->setPlainText(QString::fromUtf8(_repr->content() ? _repr->content() : ""));
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
    valueItem->setEditable(false);
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
    _editingIndex = index;
    const QRect rect = _treeView->visualRect(index);
    const QPoint pos = _treeView->viewport()->mapToGlobal(rect.topLeft());
    _popup->edit(nameItem->text(), valueItem->text(), pos);
}

void AttributeEditWidget::onNameEdited(QStandardItem* item) {
    if (!_repr || _update.pending() || item->column() != 0) {
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
        } else {
            auto nameItem = new QStandardItem(attrName);
            nameItem->setEditable(true);
            nameItem->setData(attrName, Qt::UserRole);
            auto valueItem = new QStandardItem(value);
            valueItem->setEditable(false);
            _model->appendRow({nameItem, valueItem});
        }
    } else if (row >= 0) {
        _model->removeRow(row);
    }
}

void AttributeEditWidget::notifyContentChanged(Inkscape::XML::Node& /*node*/,
                                               Inkscape::Util::ptr_shared /*old_content*/,
                                               Inkscape::Util::ptr_shared new_content) {
    if (!_contentEdit || !_repr || _update.pending()) {
        return;
    }
    if (!_contentEdit->document()->isModified()) {
        const QString text = new_content ? QString::fromUtf8(new_content.pointer()) : QString();
        auto scoped = _update.block();
        _contentEdit->setPlainText(text);
    }
    _contentEdit->document()->setModified(false);
}

} // namespace Linea::UI
