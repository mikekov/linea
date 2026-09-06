// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Shared document-template items for menus and welcome-page lists.
 */

#include "document-templates-menu.h"

namespace Linea::UI {

std::vector<CustomMenuItem> newDocumentFromTemplateMenu() {
    return {
        CustomMenuItem{
            .action = "document-new-from-template-1",
            .title = "Display",
            .description = "1024\u00d7768 px",
            .icon = QIcon(":/icons/big-display"),
            .iconSize = QSize(24, 24),
        },
        {
            .action = "document-new-from-template-2",
            .title = "A4",
            .description = "210\u00d7297 mm",
            .icon = QIcon(":/icons/big-page"),
            .iconSize = QSize(24, 24),
        },
        {
            .action = "document-new-from-template-3",
            .title = "US Letter",
            .description = "8.5\u00d711 in",
            .icon = QIcon(":/icons/big-page"),
            .iconSize = QSize(24, 24),
        },
    };
}

} // namespace Linea::UI
