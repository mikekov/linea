// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Shared document-template items for menus and welcome-page lists.
 */

#ifndef LINEA_UI_DESKTOP_DOCUMENT_TEMPLATES_MENU_H
#define LINEA_UI_DESKTOP_DOCUMENT_TEMPLATES_MENU_H

#include <vector>

#include <QString>

#include "ui/widget/custom-menu.h"

namespace Linea::UI {

std::vector<CustomMenuItem> newDocumentFromTemplateMenu();

// The template to use for plain "new document" creation, persisted in
// preferences. Index corresponds to the document-new-from-template-N actions.
int defaultTemplateIndex();
void setDefaultTemplateIndex(int index);

// Template file for the given index (1-based); unknown indices yield the
// plain default template.
const char* templateFilenameForIndex(int index);

// Index encoded in a "document-new-from-template-N" action id, -1 if none.
int templateIndexForAction(const QString& action);

} // namespace Linea::UI

#endif // LINEA_UI_DESKTOP_DOCUMENT_TEMPLATES_MENU_H
