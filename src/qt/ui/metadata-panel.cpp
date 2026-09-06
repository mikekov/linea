// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Metadata panel — Dublin Core RDF entities (Qt version).
 */

#include "metadata-panel.h"

#include <cstring>
#include <glibmm/i18n.h>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QFrame>

#include "document.h"
#include "document-undo.h"
#include "preferences.h"
#include "rdf.h"
#include "streq.h"
#include "object/sp-root.h"

namespace Linea::UI {

MetadataPanel::MetadataPanel(bool enable_scrollbar, QWidget* parent)
    : QWidget(parent)
    , _document(nullptr)
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    QWidget* container;
    if (enable_scrollbar) {
        // Scroll area for the metadata content
        auto scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        container = new QWidget(scrollArea);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        scrollArea->setWidget(container);
        mainLayout->addWidget(scrollArea, 1);
    } else {
        // Direct container without scroll area
        container = new QWidget(this);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        mainLayout->addWidget(container, 1);
    }

    _grid = new QGridLayout();
    _grid->setHorizontalSpacing(4);
    _grid->setVerticalSpacing(4);
    _grid->setContentsMargins(0, 0, 0, 0);

    auto containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    containerLayout->addLayout(_grid);
    containerLayout->addStretch();

    // Populate entity rows — entries are built once; update() refreshes values.
    int row = 0;
    for (auto entity = rdf_work_entities; entity && entity->name; ++entity) {
        if (entity->editable != RDF_EDIT_GENERIC) continue;

        Row entryRow;
        entryRow.entity = entity;

        auto label = new QLabel(tr(entity->title));
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setProperty("class", "panel-label");
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        _grid->addWidget(label, row++, 0, 1, 2);
        entryRow.label = label;

        if (entity->format == RDF_FORMAT_MULTILINE) {
            auto textedit = new QTextEdit();
            textedit->setAcceptRichText(false);
            textedit->setLineWrapMode(QTextEdit::WidgetWidth);
            textedit->setToolTip(tr(entity->tip));
            textedit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            textedit->setMaximumHeight(9999);

            // let "description" field grow taller than other fields; it's the one that typically needs more space
            if (streq(entity->name, "description")) {
                textedit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            } else {
                // other multiline fields should stay at fixed height with good scrollbar
                textedit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                textedit->setMinimumHeight(30);
                textedit->setMaximumHeight(60);
            }

            _grid->addWidget(textedit, row, 0, 1, 2);

            entryRow.editor = textedit;
            entryRow.textedit = textedit;

            connect(textedit, &QTextEdit::textChanged, [this, entity, textedit]() {
                onTextChanged(entity, textedit);
            });
        } else {
            auto entry = new QLineEdit();
            entry->setToolTip(tr(entity->tip));
            entry->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            _grid->addWidget(entry, row, 0, 1, 2);

            entryRow.editor = entry;
            entryRow.entry = entry;

            connect(entry, &QLineEdit::textChanged, [this, entity, entry]() {
                onEntryChanged(entity, entry);
            });
        }

        _rows.push_back(entryRow);
        ++row;
    }

    // load/save default metadata
    auto defaultsLabel = new QLabel(tr("Defaults"));
    defaultsLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    defaultsLabel->setProperty("class", "panel-label");
    _grid->addWidget(defaultsLabel, row++, 0, 1, 2);

    _btnLoadDefault = new QPushButton(tr("Load"));
    _btnLoadDefault->setToolTip(tr("Use the previously saved default metadata here"));
    _btnLoadDefault->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _btnSaveDefault = new QPushButton(tr("Save"));
    _btnSaveDefault->setToolTip(tr("Save this metadata as the default metadata"));
    _btnSaveDefault->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _grid->addWidget(_btnLoadDefault, row, 0);
    _grid->addWidget(_btnSaveDefault, row, 1);

    connect(_btnSaveDefault, &QPushButton::clicked, this, &MetadataPanel::onSaveDefaultClicked);
    connect(_btnLoadDefault, &QPushButton::clicked, this, &MetadataPanel::onLoadDefaultClicked);

    // Set column stretch
    _grid->setColumnStretch(0, 1);
    _grid->setColumnStretch(1, 1);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

void MetadataPanel::setDocument(SPDocument* document) {
    _document = document;
}

void MetadataPanel::update(SPDocument* document) {
    _document = document;
    if (!document) return;

    for (auto& row : _rows) {
        updateRow(row, document);
    }
}

void MetadataPanel::onSaveDefaultClicked() {
    if (!_document) return;

    for (const auto& row : _rows) {
        saveToPreferences(row);
    }
}

void MetadataPanel::onLoadDefaultClicked() {
    for (const auto& row : _rows) {
        loadFromPreferences(row);
    }
}

void MetadataPanel::onEntryChanged(rdf_work_entity_t* entity, QLineEdit* entry) {
    if (_update.pending() || !_document) return;

    auto scoped(_update.block());

    QString text = entry->text();
    if (rdf_set_work_entity(_document, entity, text.toUtf8().constData())) {
        if (Inkscape::DocumentUndo::getUndoSensitive(_document)) {
            Inkscape::DocumentUndo::done(_document, RC_("Undo", "Document metadata updated"), "");
        }
    }
}

void MetadataPanel::onTextChanged(rdf_work_entity_t* entity, QTextEdit* textedit) {
    if (_update.pending() || !_document) return;

    auto scoped(_update.block());

    QString text = textedit->toPlainText();
    if (rdf_set_work_entity(_document, entity, text.toUtf8().constData())) {
        if (Inkscape::DocumentUndo::getUndoSensitive(_document)) {
            Inkscape::DocumentUndo::done(_document, RC_("Undo", "Document metadata updated"), "");
        }
    }
}

void MetadataPanel::saveToPreferences(const Row& row) {
    if (!_document || !row.entity) return;

    auto prefs = Inkscape::Preferences::get();
    auto text = rdf_get_work_entity(_document, row.entity);
    prefs->setString(PREFS_METADATA + Glib::ustring(row.entity->name), Glib::ustring(text ? text : ""));
}

void MetadataPanel::loadFromPreferences(const Row& row) {
    if (!row.entity) return;

    auto scoped(_update.block());

    auto prefs = Inkscape::Preferences::get();
    auto text = prefs->getString(PREFS_METADATA + Glib::ustring(row.entity->name));
    if (text.empty()) return;

    QString qtext = QString::fromUtf8(text.c_str());

    if (row.entry) {
        row.entry->setText(qtext);
        return;
    }

    if (row.textedit) {
        row.textedit->setPlainText(qtext);
    }
}

void MetadataPanel::updateRow(Row& row, SPDocument* document) {
    if (!row.entity || !document) return;

    // "title" can also come from the document <title> element
    auto text = rdf_get_work_entity(document, row.entity);
    if (!text && std::strcmp(row.entity->name, "title") == 0 && document->getRoot()) {
        text = document->getRoot()->title();
    }

    if (!text) {
        text = "";
    }

    QString qtext = QString::fromUtf8(text);

    if (row.entry) {
        QSignalBlocker blocker(row.entry);
        row.entry->setText(qtext);
    }
    else if (row.textedit) {
        QSignalBlocker blocker(row.textedit);
        row.textedit->setPlainText(qtext);
    }
}

} // namespace Linea::UI
