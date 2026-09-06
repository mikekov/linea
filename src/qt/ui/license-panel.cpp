// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * License panel — Creative Commons / license selector (Qt version).
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "license-panel.h"

#include <cstring>
#include <glibmm/i18n.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSignalBlocker>

#include "document.h"
#include "document-undo.h"
#include "rdf.h"

namespace Linea::UI {

// Proprietary license (no details = proprietary)
static const rdf_license_t _proprietary_license =
  {_("Proprietary"), "", nullptr};

// Other/custom license
static const rdf_license_t _other_license =
  {Q_("MetadataLicence|Other"), "", nullptr};

LicensePanel::LicensePanel(QWidget* parent)
    : QWidget(parent)
    , _document(nullptr)
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

//debug:
// setProperty("class", "x-panel");
// setAttribute(Qt::WA_StyledBackground, true);

    // Title
    auto titleLabel = new QLabel(QString::fromUtf8(_("<b>License</b>")));
    titleLabel->setTextFormat(Qt::RichText);
    titleLabel->setAlignment(Qt::AlignLeft);
    mainLayout->addWidget(titleLabel);

    // Grid for license selector and URI
    _grid = new QGridLayout();
    _grid->setHorizontalSpacing(0);
    _grid->setVerticalSpacing(4);
    // _grid->setContentsMargins(0, 0, 0, 0);

    // License dropdown
    // auto licenseLabel = new QLabel(QString::fromUtf8(_("License")));
    // _grid->addWidget(licenseLabel, 0, 0);

    _licenseCombo = new QComboBox();
    _licenseCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _licenseCombo->setContentsMargins(0, 0, 0, 0);
    _grid->addWidget(_licenseCombo, 0, 0);

    // URI label and entry
    _uriLabel = new QLabel(QString::fromUtf8(_("URI")));
    _uriLabel->setProperty("class", "panel-label");
    _grid->addWidget(_uriLabel, 1, 0);

    _uriEntry = new QLineEdit();
    _uriEntry->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _grid->addWidget(_uriEntry, 2, 0);

    mainLayout->addLayout(_grid);
    mainLayout->addStretch();

    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    // Populate license list
    _licenses.clear();
    _licenses.push_back(&_proprietary_license);

    for (auto license = rdf_licenses; license && license->name; ++license) {
        _licenses.push_back(license);
    }

    _licenses.push_back(&_other_license);

    // Populate combo box
    for (auto license : _licenses) {
        _licenseCombo->addItem(QString::fromUtf8(_(license->name)));
    }
    _licenseCombo->setCurrentIndex(0);

    // Connect signals
    connect(_licenseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LicensePanel::onLicenseChanged);
    connect(_uriEntry, &QLineEdit::textChanged,
            this, &LicensePanel::onUriChanged);
}

void LicensePanel::setDocument(SPDocument* document) {
    _document = document;
}

const rdf_license_t* LicensePanel::getSelectedLicense() const {
    if (!_licenseCombo) return nullptr;
    int selected = _licenseCombo->currentIndex();
    if (selected < 0 || static_cast<size_t>(selected) >= _licenses.size()) return nullptr;
    return _licenses[selected];
}

void LicensePanel::setUriText(const char* text) {
    auto scoped(_update.block());
    _uriEntry->setText(QString::fromUtf8(text ? text : ""));
}

void LicensePanel::onLicenseChanged() {
    if (_update.pending() || !_document) return;

    const auto license = getSelectedLicense();
    if (!license) return;

    auto scoped(_update.block());

    // Set the license (nullptr for proprietary means no license RDF)
    rdf_set_license(_document, license->details ? license : nullptr);

    // Check undo sensitivity using DocumentUndo
    if (Inkscape::DocumentUndo::getUndoSensitive(_document)) {
        Inkscape::DocumentUndo::done(_document, RC_("Undo", "Document license updated"), "");
    }

    // Update URI entry
    setUriText(license->uri);

    // Also update the license_uri RDF entity
    rdf_work_entity_t *entity = rdf_find_entity("license_uri");
    if (!entity) return;
    rdf_set_work_entity(_document, entity, _uriEntry->text().toUtf8().constData());
}

void LicensePanel::onUriChanged() {
    if (_update.pending() || !_document) return;

    rdf_work_entity_t *entity = rdf_find_entity("license_uri");
    if (!entity) return;

    auto scoped(_update.block());

    QString text = _uriEntry->text();
    if (rdf_set_work_entity(_document, entity, text.toUtf8().constData())) {
        if (Inkscape::DocumentUndo::getUndoSensitive(_document)) {
            Inkscape::DocumentUndo::done(_document, RC_("Undo", "Document metadata updated"), "");
        }
    }
}

void LicensePanel::update(SPDocument* doc) {
    if (!doc || !_licenseCombo || _licenses.empty()) return;

    constexpr bool read_only = false;
    const auto license = rdf_get_license(doc, read_only);

    // Find the license in our list
    auto it = std::find(_licenses.begin(), _licenses.end(), license);
    if (it == _licenses.end()) {
        it = _licenses.begin();  // Default to proprietary
    }

    auto scoped(_update.block());

    int index = std::distance(_licenses.begin(), it);
    _licenseCombo->setCurrentIndex(index);

    // Get the license_uri entity text
    auto entity = rdf_find_entity("license_uri");
    const auto text = entity ? rdf_get_work_entity(doc, entity) : nullptr;
    setUriText(text);
}

} // namespace Linea::UI
