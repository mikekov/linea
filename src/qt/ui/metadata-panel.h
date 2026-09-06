// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Metadata panel — Dublin Core RDF entities (Qt version).
 */

#ifndef LINEA_UI_METADATA_PANEL_H
#define LINEA_UI_METADATA_PANEL_H

#include <vector>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QGridLayout;
class QLabel;
class QLineEdit;
class QTextEdit;
class QScrollArea;
class QPushButton;
QT_END_NAMESPACE

class SPDocument;
struct rdf_work_entity_t;

#include "ui/operation-blocker.h"

namespace Linea::UI {

/**
 * Passive panel showing Dublin Core RDF entities for the document (Qt version).
 */
class MetadataPanel : public QWidget {
    Q_OBJECT

public:
    MetadataPanel(bool enable_scrollbar, QWidget* parent = nullptr);
    ~MetadataPanel() override = default;

    void setDocument(SPDocument* document);
    void update(SPDocument* document);

    QGridLayout* gridLayout() const { return _grid; }

private:
    struct Row {
        rdf_work_entity_t* entity = nullptr;
        QLabel* label = nullptr;
        QWidget* editor = nullptr;
        QLineEdit* entry = nullptr;
        QTextEdit* textedit = nullptr;
    };

    void onEntryChanged(rdf_work_entity_t* entity, QLineEdit* entry);
    void onTextChanged(rdf_work_entity_t* entity, QTextEdit* textedit);
    void saveToPreferences(const Row& row);
    void loadFromPreferences(const Row& row);
    void updateRow(Row& row, SPDocument* document);

    SPDocument*      _document = nullptr;
    OperationBlocker _update;
    QGridLayout*     _grid = nullptr;
    std::vector<Row> _rows;

    // Default buttons
    QPushButton* _btnLoadDefault = nullptr;
    QPushButton* _btnSaveDefault = nullptr;

private Q_SLOTS:
    void onLoadDefaultClicked();
    void onSaveDefaultClicked();
};

} // namespace Linea::UI

#endif // LINEA_UI_METADATA_PANEL_H
