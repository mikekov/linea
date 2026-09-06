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

#ifndef LINEA_UI_LICENSE_PANEL_H
#define LINEA_UI_LICENSE_PANEL_H

#include <vector>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QGridLayout;
class QLineEdit;
class QLabel;
QT_END_NAMESPACE

class SPDocument;
struct rdf_license_t;

#include "ui/operation-blocker.h"

namespace Linea::UI {

/**
 * Passive panel for selecting document license (Qt version).
 *
 * setDocument(SPDocument*) must be called on document changes.
 * update(SPDocument*) refreshes the UI from the document's RDF data.
 */
class LicensePanel : public QWidget {
    Q_OBJECT

public:
    LicensePanel(QWidget* parent = nullptr);
    ~LicensePanel() override = default;

    void setDocument(SPDocument* document);
    void update(SPDocument* document);

    QGridLayout* gridLayout() const { return _grid; }

private:
    void onLicenseChanged();
    void onUriChanged();
    const rdf_license_t* getSelectedLicense() const;
    void setUriText(const char* text);

    QLabel* _uriLabel = nullptr;
    QLineEdit* _uriEntry = nullptr;
    QComboBox* _licenseCombo = nullptr;
    QGridLayout* _grid = nullptr;
    std::vector<const rdf_license_t*> _licenses;
    OperationBlocker _update;
    SPDocument* _document = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_LICENSE_PANEL_H
