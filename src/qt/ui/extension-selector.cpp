// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ExtensionSelector implementation.
 */

#include "extension-selector.h"

#include <QRegularExpression>
#include <QString>

#include "extension/db.h"
#include "extension/output.h"

namespace Linea::UI {

ExtensionSelector::ExtensionSelector(QWidget* parent)
    : IconComboBox(parent)
{
    setHeaderType(IconComboBox::LabelOnly);
    populate();
}

Inkscape::Extension::Output* ExtensionSelector::selectedExtension() const {
    const auto id = getActiveRowId();
    if (id < 0 || id >= static_cast<int>(_extensions.size())) {
        return nullptr;
    }

    return _extensions[id];
}

void ExtensionSelector::populate() {
    Inkscape::Extension::DB::OutputList extensions;
    Inkscape::Extension::db.get_output_list(extensions);

    int id = 0;
    for (auto extension : extensions) {
        if (extension->deactivated()) continue;

        if (!extension->is_exported() && !extension->is_raster()) continue;
    // printf("ext: %s\n", extension->get_id());

        QString label;
        if (strcmp(extension->get_id(), "org.inkscape.output.pdf.cairorenderer") == 0) {
            label = "PDF";
        }
        else if (strcmp(extension->get_id(), "org.inkscape.output.svg.plain") == 0) {
            label = "SVG";
        }
        else if (strcmp(extension->get_id(), "org.inkscape.output.svg.inkscape") == 0) {
            continue;
        }
        else if (strcmp(extension->get_id(), "org.inkscape.raster.tiff_output") == 0) {
            continue;
        }
        else {
            label = QString::fromUtf8(extension->get_filetypename(true));
            label.remove(QRegularExpression(QStringLiteral(R"(\s+\(\*\.\w+\)$)")));
        }

        addRow(QString(), label, id++);
        _extensions.push_back(extension);
    }
}

} // namespace Linea::UI
