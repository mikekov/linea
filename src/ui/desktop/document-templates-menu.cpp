// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Shared document-template items for menus and welcome-page lists.
 */

#include "document-templates-menu.h"

#include <cstring>

#include "preferences.h"

namespace Linea::UI {

namespace {
constexpr const char* kDefaultTemplatePref = "/options/defaulttemplate/value";
constexpr const char* kTemplateActionPrefix = "document-new-from-template-";
} // namespace

int defaultTemplateIndex() {
    return Inkscape::Preferences::get()->getIntLimited(kDefaultTemplatePref, 1, 1, 9);
}

void setDefaultTemplateIndex(int index) {
    Inkscape::Preferences::get()->setInt(kDefaultTemplatePref, index);
}

const char* templateFilenameForIndex(int index) {
    switch (index) {
        case 2:
            return "default-wide.svg";
        case 3:
            return "default-phone.svg";
        case 4:
            return "default-a4.svg";
        case 5:
            return "default-us-letter.svg";
        default:
            return "default.svg";
    }
}

int templateIndexForAction(const QString& action) {
    if (!action.startsWith(kTemplateActionPrefix)) return -1;
    bool ok = false;
    const int index = action.mid(std::strlen(kTemplateActionPrefix)).toInt(&ok);
    return ok ? index : -1;
}

std::vector<CustomMenuItem> newDocumentFromTemplateMenu() {
    return {
        CustomMenuItem{
            .action = "document-new-from-template-1",
            .title = "Display 4:3",
            .description = "1024\u00d7768 px",
            .icon = QIcon(":/icons/big-display"),
            .iconSize = QSize(24, 24),
        },
        {
            .action = "document-new-from-template-2",
            .title = "Display 16:10",
            .description = "1920\u00d71200 px",
            .icon = QIcon(":/icons/big-display-wide"),
            .iconSize = QSize(24, 24),
        },
        {
            .action = "document-new-from-template-3",
            .title = "Phone",
            .description = "360\u00d7640 px",
            .icon = QIcon(":/icons/big-phone-1"),
            .iconSize = QSize(24, 24),
        },
        {
            .action = "document-new-from-template-4",
            .title = "A4",
            .description = "210\u00d7297 mm",
            .icon = QIcon(":/icons/big-page-a4"),
            .iconSize = QSize(24, 24),
        },
        {
            .action = "document-new-from-template-5",
            .title = "US Letter",
            .description = "8.5\u00d711 in",
            .icon = QIcon(":/icons/big-page"),
            .iconSize = QSize(24, 24),
        },
    };
}

} // namespace Linea::UI
