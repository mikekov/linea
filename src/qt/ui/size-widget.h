// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SizeWidget - Widget for editing position and dimensions.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_SIZE_WIDGET_H
#define LINEA_UI_SIZE_WIDGET_H

#include <QWidget>

#include <memory>

#include "ui/operation-blocker.h"

class SPDesktop;

namespace Ui {
class SizeWidget;
}

namespace Linea {

namespace Props {
class Binder;
}

namespace UI {
class PopupMenu;
class TransformPanel;

/**
 * Widget for editing position (x, y) and dimensions (width, height).
 *
 * Uses a 4-row grid layout:
 * - Row 1: "Position" label
 * - Row 2: x and y NumberEdit widgets
 * - Row 3: "Dimensions" label
 * - Row 4: width and height NumberEdit widgets
 */
class SizeWidget : public QWidget {
    Q_OBJECT

public:
    explicit SizeWidget(QWidget* parent = nullptr);
    ~SizeWidget() override;

    void bind(Props::Binder& binder);
    void setDesktop(SPDesktop* desktop);

private Q_SLOTS:
    void onTransformClicked();

private:
    std::unique_ptr<Ui::SizeWidget> _ui;
    PopupMenu* _transformPopup = nullptr;
    TransformPanel* _transformPanel = nullptr;
    SPDesktop* _desktop = nullptr;
    OperationBlocker _update;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_SIZE_WIDGET_H
