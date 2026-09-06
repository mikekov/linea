// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main toolbar containing the primary tool buttons.
 */

#ifndef LINEA_UI_MAIN_TOOLBAR_H
#define LINEA_UI_MAIN_TOOLBAR_H

#include "toolbar.h"

/**
 * The main application toolbar with all the primary drawing tool buttons.
 */
namespace Linea::UI {
class PaintIndicator;

class MainToolbar : public Toolbar {
    Q_OBJECT

public:
    explicit MainToolbar(QWidget* parent = nullptr);

    void setStretch(bool stretch);

    Linea::UI::PaintIndicator* paintIndicator() const { return _paintIndicator; }

private:
    QAction* _current_shape_action = nullptr;
    QAction* _current_drawing_action = nullptr;
    QAction* _current_other_action = nullptr;
    QAction* _current_extra_action = nullptr;
    Linea::UI::PaintIndicator* _paintIndicator = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_MAIN_TOOLBAR_H
