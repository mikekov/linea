// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt dialog for editing XML files like preferences.xml.
 */

#ifndef LINEA_UI_DIALOG_XML_SETTINGS_DIALOG_H
#define LINEA_UI_DIALOG_XML_SETTINGS_DIALOG_H

#include <QDialog>
#include <QDomDocument>
#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QStandardItem;
class QStandardItemModel;
class QTableWidget;
class QTreeView;
QT_END_NAMESPACE

namespace Linea::UI {

class XmlSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit XmlSettingsDialog(QWidget* parent = nullptr);
    ~XmlSettingsDialog() override;

    bool load(const QString& filePath);
    bool save(const QString& filePath = QString());

private Q_SLOTS:
    void onTreeSelectionChanged();
    void onAttrCellChanged(int row, int column);
    void addAttribute();
    void removeAttribute();
    void onSearchTextChanged(const QString &text);

private:
    void buildUi();
    void populateTree(QStandardItem* parentItem, const QDomElement& element);
    void refreshAttrTable();
    void setStatus(const QString& message);
    bool filterBranch(const QModelIndex& parent, const QString& text);

    static constexpr int ElementIndexRole = Qt::UserRole + 1;

    QDomDocument _doc;
    QString _filePath;
    QList<QDomElement> _elements;
    QDomElement _currentElement;
    QString _currentPrefPath;

    QTreeView* _treeView = nullptr;
    QStandardItemModel* _treeModel = nullptr;
    QTableWidget* _attrTable = nullptr;
    // QPushButton* _addButton = nullptr;
    // QPushButton* _removeButton = nullptr;
    // QPushButton* _saveButton = nullptr;
    QLineEdit* _searchEdit = nullptr;
    QLabel* _statusLabel = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_DIALOG_XML_SETTINGS_DIALOG_H
