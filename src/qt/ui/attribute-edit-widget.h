// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt widget for editing XML attributes.
 */

#ifndef LINEA_UI_ATTRIBUTE_EDIT_WIDGET_H
#define LINEA_UI_ATTRIBUTE_EDIT_WIDGET_H

#include <QDialog>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>
#include <memory>

#include "syntax.h"
#include "ui/operation-blocker.h"
#include "xml/node-observer.h"

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Inkscape::XML {
class Node;
}

namespace Linea::UI {

/**
 * A popup dialog for editing an attribute value with syntax highlighting.
 */
class AttributeEditPopup : public QDialog {
    Q_OBJECT

public:
    explicit AttributeEditPopup(QWidget* parent = nullptr);
    ~AttributeEditPopup() override;

    void edit(const QString& attrName, const QString& value, const QPoint& pos);
    QString value() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void onOk();
    void onCancel();

    QVBoxLayout* _editorLayout = nullptr;
    QPushButton* _okButton = nullptr;
    QPushButton* _cancelButton = nullptr;
    std::unique_ptr<Linea::UI::Syntax::TextEditView> _editor;
};

/**
 * A widget for viewing and editing XML attributes of a single node.
 *
 * For element nodes it shows a two-column list (Name, Value).  Editing a
 * value opens a popup with a QSyntaxHighlighter-based editor appropriate for
 * the attribute type (CSS, SVG path data, etc.).
 */
class AttributeEditWidget : public QWidget
    , private Inkscape::XML::NodeObserver {
    Q_OBJECT

public:
    explicit AttributeEditWidget(QWidget* parent = nullptr);
    ~AttributeEditWidget() override;

    void setRepr(Inkscape::XML::Node* repr);

    QTreeView* treeView() const { return _treeView; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:
    void onAddAttribute();
    void onDeleteAttribute();
    void onEditValue(const QModelIndex& index);
    void onNameEdited(QStandardItem* item);
    void onPopupAccepted();
    void onPopupRejected();

private:
    void buildAttributeList();
    void clearAttributeList();

    void notifyAttributeChanged(Inkscape::XML::Node& node, GQuark name,
                                Inkscape::Util::ptr_shared old_value,
                                Inkscape::Util::ptr_shared new_value) override;
    void notifyContentChanged(Inkscape::XML::Node& node,
                              Inkscape::Util::ptr_shared old_content,
                              Inkscape::Util::ptr_shared new_content) override;

    Inkscape::XML::Node* _repr = nullptr;
    QStackedWidget* _stack = nullptr;
    QWidget* _treePage = nullptr;
    QTreeView* _treeView = nullptr;
    QStandardItemModel* _model = nullptr;
    QToolButton* _addButton = nullptr;
    QToolButton* _deleteButton = nullptr;
    QPlainTextEdit* _contentEdit = nullptr;
    std::unique_ptr<AttributeEditPopup> _popup;
    QModelIndex _editingIndex;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_ATTRIBUTE_EDIT_WIDGET_H
