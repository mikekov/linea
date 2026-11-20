// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main toolbar containing the primary tool buttons.
 */

#ifndef LINEA_UI_MAIN_TOOLBAR_H
#define LINEA_UI_MAIN_TOOLBAR_H

#include "toolbar.h"

class QHideEvent;
class QToolButton;
class QVariantAnimation;

/**
 * The main application toolbar with all the primary tool buttons.
 */
namespace Linea::UI {
class PaintIndicator;

class MainToolbar : public Toolbar {
    Q_OBJECT

public:
    explicit MainToolbar(QWidget* parent = nullptr);

    void setStretch(bool stretch);

    // Show/hide the shape-builder toggle button in the slot.
    void setShapeBuilderVisible(bool visible);

    // Show/hide the shape-builder accept/cancel buttons in the slot.
    void setShapeBuilderCancelVisible(bool visible);

    // Show/hide the path-operations button in the slot. pathOpsButton()
    // exposes the raw button for wiring up its custom popup.
    void setPathOpsVisible(bool visible);
    QToolButton* pathOpsButton() const { return _pathOps; }

    // Show/hide the mask-operations button in the slot.
    void setMaskOpsVisible(bool visible);

    // Show/hide the exit-isolation button in the slot.
    void setIsolationExitVisible(bool visible);

    Linea::UI::PaintIndicator* paintIndicator() const { return _paintIndicator; }

protected:
    void hideEvent(QHideEvent* event) override;

private:
    void applyExtraWidth(int width);
    void updateExtraVisibility();

    QAction* _current_shape_action = nullptr;
    QAction* _current_drawing_action = nullptr;
    QAction* _current_other_action = nullptr;
    QAction* _current_extra_action = nullptr;
    Linea::UI::PaintIndicator* _paintIndicator = nullptr;
    QWidget* _extraSlot = nullptr;
    QToolButton* _shapeBuilder = nullptr;
    QToolButton* _shapeBuilderOK = nullptr;
    QToolButton* _shapeBuilderCancel = nullptr;
    QToolButton* _pathOps = nullptr;
    QToolButton* _maskOps = nullptr;
    QToolButton* _exitGroupEditing = nullptr;
    QVariantAnimation* _extraAnimation = nullptr;
    bool _shapeBuilderVisible = false;
    bool _shapeBuilderCancelVisible = false;
    bool _pathOpsVisible = false;
    bool _maskOpsVisible = false;
    bool _isolationExitVisible = false;
    int _extraMaxWidth = 0;
    int _extraTargetWidth = 0;
    int _currentExtraWidth = 0;
    int _minWidth = 0;
    int _maxWidth = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_MAIN_TOOLBAR_H
