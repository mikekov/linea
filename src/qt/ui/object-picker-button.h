// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Reusable object picker button for LPE parameter widgets.
 *
 * Creates a toggle button that activates the object picker tool.
 * When the user clicks an object on canvas, the callback is invoked
 * with the picked SPObject*. The button auto-deactivates after picking
 * or if the user switches to another tool.
 */

#ifndef QT_UI_OBJECT_PICKER_BUTTON_H
#define QT_UI_OBJECT_PICKER_BUTTON_H

#include <functional>

class QPushButton;
class SPObject;

namespace Linea::UI {

/// Create a picker toggle button. Add it to your layout.
/// @param on_picked  Called when user picks an object on canvas.
/// @param tooltip    Optional tooltip text (defaults to "Pick object on canvas").
QPushButton* create_object_picker_button(std::function<void(SPObject*)> on_picked, const char* tooltip = nullptr);

} // namespace Linea::UI

#endif // QT_UI_OBJECT_PICKER_BUTTON_H
