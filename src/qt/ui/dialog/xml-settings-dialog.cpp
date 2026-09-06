// SPDX-License-Identifier: GPL-2.0-or-later
#include "xml-settings-dialog.h"

#include "preferences.h"

#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStringConverter>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeView>
#include <QVBoxLayout>

static QString buildPrefNodePath(const QDomElement& element) {
    QStringList ids;
    for (QDomNode node = element; !node.isNull(); node = node.parentNode()) {
        QDomElement e = node.toElement();
        if (e.isNull()) {
            continue;
        }
        QString id = e.attribute("id");
        if (!id.isEmpty()) {
            ids.prepend(id);
        }
    }
    return ids.isEmpty() ? QString() : ("/" + ids.join("/"));
}

namespace Linea::UI {

XmlSettingsDialog::XmlSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("XML Settings Editor"));
    setMinimumSize(800, 600);
    buildUi();

    connect(_treeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &XmlSettingsDialog::onTreeSelectionChanged);
    connect(_attrTable, &QTableWidget::cellChanged, this, &XmlSettingsDialog::onAttrCellChanged);
    // connect(_addButton, &QPushButton::clicked, this, &XmlSettingsDialog::addAttribute);
    // connect(_removeButton, &QPushButton::clicked, this, &XmlSettingsDialog::removeAttribute);
    // connect(_saveButton, &QPushButton::clicked, [this]() { save(); });
}

XmlSettingsDialog::~XmlSettingsDialog() = default;

void XmlSettingsDialog::buildUi() {
    _treeModel = new QStandardItemModel(this);
    _treeModel->setHorizontalHeaderLabels(QStringList{tr("Preference / Element")});

    _treeView = new QTreeView(this);
    _treeView->setModel(_treeModel);
    _treeView->setHeaderHidden(true);

    _attrTable = new QTableWidget(0, 2, this);
    _attrTable->setHorizontalHeaderLabels(QStringList{tr("Attribute"), tr("Value")});
    _attrTable->horizontalHeader()->setStretchLastSection(true);
    _attrTable->setColumnWidth(0, 200);

    // _addButton = new QPushButton(tr("Add attribute"), this);
    // _removeButton = new QPushButton(tr("Remove attribute"), this);
    // _saveButton = new QPushButton(tr("Save"), this);

    // auto attrButtonLayout = new QHBoxLayout;
    // attrButtonLayout->addWidget(_addButton);
    // attrButtonLayout->addWidget(_removeButton);
    // attrButtonLayout->addStretch();
    // attrButtonLayout->addWidget(_saveButton);

    _statusLabel = new QLabel(this);
    _statusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    _searchEdit = new QLineEdit(this);
    _searchEdit->setPlaceholderText(tr("Search..."));
    _searchEdit->setClearButtonEnabled(true);
    connect(_searchEdit, &QLineEdit::textChanged, this, &XmlSettingsDialog::onSearchTextChanged);

    auto* grid = new QGridLayout(this);
    grid->setSpacing(6);
    grid->setContentsMargins(6, 6, 6, 6);

    // Search bar across the top
    grid->addWidget(_searchEdit, 0, 0, 1, 2);
    // Tree on the left, spanning the upper rows so it is full height
    grid->addWidget(_treeView, 1, 0, 2, 1);
    // Attributes and buttons on the right
    grid->addWidget(_attrTable, 1, 1, 2, 1);
    // grid->addLayout(attrButtonLayout, 2, 1);
    // Status at the bottom, spanning both columns and not stretched vertically
    grid->addWidget(_statusLabel, 3, 0, 1, 2);

    grid->setRowStretch(0, 0);
    grid->setRowStretch(1, 1);
    grid->setRowStretch(2, 0);
    grid->setRowStretch(3, 0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 2);
}

bool XmlSettingsDialog::load(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file:\n%1").arg(filePath));
        return false;
    }

    auto result = _doc.setContent(&file);
    if (!result) {
        QMessageBox::critical(this, tr("XML Error"),
                              tr("Parse error at line %1: %2").arg(result.errorLine).arg(result.errorMessage));
        return false;
    }
    file.close();

    _filePath = filePath;
    _treeModel->clear();
    _elements.clear();
    _currentElement = QDomElement();
    refreshAttrTable();

    QDomElement root = _doc.documentElement();
    populateTree(nullptr, root);
    _treeView->expandToDepth(1);

    setStatus(tr("Loaded %1").arg(filePath));
    return true;
}

void XmlSettingsDialog::populateTree(QStandardItem* parentItem, const QDomElement& element) {
    for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
        if (!node.isElement()) {
            continue;
        }
        QDomElement child = node.toElement();

        int index = _elements.size();
        _elements.append(child);

        QString display = child.attribute("id");
        if (display.isEmpty()) {
            display = child.tagName();
        }

        auto* item = new QStandardItem(display);
        item->setData(index, ElementIndexRole);
        item->setEditable(false);

        if (parentItem) {
            parentItem->appendRow(item);
        } else {
            _treeModel->appendRow(item);
        }

        populateTree(item, child);
    }
}

void XmlSettingsDialog::onTreeSelectionChanged() {
    _currentPrefPath.clear();

    QModelIndex index = _treeView->currentIndex();
    if (!index.isValid()) {
        _currentElement = QDomElement();
        refreshAttrTable();
        return;
    }

    QStandardItem* item = _treeModel->itemFromIndex(index);
    if (!item) {
        _currentElement = QDomElement();
        refreshAttrTable();
        return;
    }

    int elementIndex = item->data(ElementIndexRole).toInt();
    if (elementIndex < 0 || elementIndex >= _elements.size()) {
        _currentElement = QDomElement();
        refreshAttrTable();
        return;
    }

    _currentElement = _elements[elementIndex];
    _currentPrefPath = buildPrefNodePath(_currentElement);
    refreshAttrTable();
}

void XmlSettingsDialog::refreshAttrTable() {
    QSignalBlocker blocker(_attrTable);
    _attrTable->clearContents();
    _attrTable->setRowCount(0);

    if (_currentElement.isNull()) {
        _attrTable->setEnabled(false);
        // _addButton->setEnabled(false);
        // _removeButton->setEnabled(false);
        return;
    }

    _attrTable->setEnabled(true);
    // _addButton->setEnabled(true);
    // _removeButton->setEnabled(true);

    QDomNamedNodeMap attrs = _currentElement.attributes();
    int row = 0;
    for (int i = 0; i < attrs.count(); ++i) {
        QDomAttr attr = attrs.item(i).toAttr();
        if (attr.name() == "id") {
            continue;
        }

        _attrTable->insertRow(row);
        auto nameItem = new QTableWidgetItem(attr.name());
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(ElementIndexRole, attr.name());
        _attrTable->setItem(row, 0, nameItem);

        auto valueItem = new QTableWidgetItem(attr.value());
        _attrTable->setItem(row, 1, valueItem);
        ++row;
    }
}

void XmlSettingsDialog::onAttrCellChanged(int row, int column) {
    if (_currentElement.isNull() || column != 1) {
        return;
    }

    QTableWidgetItem* nameItem = _attrTable->item(row, 0);
    QTableWidgetItem* valueItem = _attrTable->item(row, 1);
    if (!nameItem || !valueItem) {
        return;
    }

    QString name = nameItem->data(ElementIndexRole).toString();
    if (name.isEmpty()) {
        name = nameItem->text();
    }
    if (name == "id") {
        return;
    }

    _currentElement.setAttribute(name, valueItem->text());
    if (!_currentPrefPath.isEmpty()) {
        Inkscape::Preferences::get()->setString(
            Glib::ustring((_currentPrefPath + "/" + name).toUtf8().constData()),
            Glib::ustring(valueItem->text().toUtf8().constData()));
    }
    setStatus(tr("Updated '%1' = '%2'").arg(name, valueItem->text()));
}

void XmlSettingsDialog::addAttribute() {
    if (_currentElement.isNull()) {
        return;
    }

    bool ok = false;
    QString name =
        QInputDialog::getText(this, tr("Add attribute"), tr("Attribute name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) {
        return;
    }
    if (name == "id") {
        QMessageBox::warning(this, tr("Invalid name"), tr("'id' is reserved for the preference name."));
        return;
    }

    _currentElement.setAttribute(name, QString());
    if (!_currentPrefPath.isEmpty()) {
        Inkscape::Preferences::get()->setString(
            Glib::ustring((_currentPrefPath + "/" + name).toUtf8().constData()),
            Glib::ustring(""));
    }
    refreshAttrTable();

    for (int row = 0; row < _attrTable->rowCount(); ++row) {
        QTableWidgetItem* nameItem = _attrTable->item(row, 0);
        if (nameItem && nameItem->data(ElementIndexRole).toString() == name) {
            _attrTable->setCurrentCell(row, 1);
            _attrTable->editItem(_attrTable->item(row, 1));
            break;
        }
    }
}

void XmlSettingsDialog::removeAttribute() {
    if (_currentElement.isNull()) {
        return;
    }

    int row = _attrTable->currentRow();
    if (row < 0) {
        return;
    }

    QTableWidgetItem* nameItem = _attrTable->item(row, 0);
    if (!nameItem) {
        return;
    }

    QString name = nameItem->data(ElementIndexRole).toString();
    if (name.isEmpty()) {
        name = nameItem->text();
    }
    if (name == "id") {
        return;
    }

    _currentElement.removeAttribute(name);
    if (!_currentPrefPath.isEmpty()) {
        Inkscape::Preferences::get()->remove(
            Glib::ustring((_currentPrefPath + "/" + name).toUtf8().constData()));
    }
    refreshAttrTable();
}

bool XmlSettingsDialog::save(const QString& filePath) {
    QString path = filePath.isEmpty() ? _filePath : filePath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save XML settings"), QString(),
                                            tr("XML files (*.xml);;All files (*)"));
        if (path.isEmpty()) {
            return false;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot write file:\n%1").arg(path));
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << _doc.toString(2);
    out.flush();
    file.close();

    _filePath = path;
    setStatus(tr("Saved %1").arg(path));
    return true;
}

void XmlSettingsDialog::setStatus(const QString& message) {
    _statusLabel->setText(message);
}

void XmlSettingsDialog::onSearchTextChanged(const QString& text) {
    filterBranch(QModelIndex(), text.toLower());
    if (!text.isEmpty()) {
        _treeView->expandAll();
    }
}

bool XmlSettingsDialog::filterBranch(const QModelIndex& parent, const QString& text) {
    bool anyVisible = false;
    int rows = _treeModel->rowCount(parent);
    for (int i = 0; i < rows; ++i) {
        QModelIndex index = _treeModel->index(i, 0, parent);
        bool childVisible = filterBranch(index, text);

        bool matches = false;
        if (text.isEmpty()) {
            matches = true;
        } else {
            QString display = index.data(Qt::DisplayRole).toString().toLower();
            if (display.contains(text)) {
                matches = true;
            } else {
                int elemIdx = _treeModel->itemFromIndex(index)->data(ElementIndexRole).toInt();
                if (elemIdx >= 0 && elemIdx < _elements.size()) {
                    QDomNamedNodeMap attrs = _elements[elemIdx].attributes();
                    for (int a = 0; a < attrs.count(); ++a) {
                        if (attrs.item(a).toAttr().name().toLower().contains(text)) {
                            matches = true;
                            break;
                        }
                    }
                }
            }
        }

        bool visible = matches || childVisible;
        _treeView->setRowHidden(i, parent, !visible);
        if (visible) {
            anyVisible = true;
        }
    }
    return anyVisible;
}

} // namespace Linea::UI
