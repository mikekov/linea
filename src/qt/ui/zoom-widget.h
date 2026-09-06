// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ZoomWidget - Widget for zoom controls.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 */

#ifndef LINEA_UI_ZOOM_WIDGET_H
#define LINEA_UI_ZOOM_WIDGET_H

#include <QWidget>

#include <memory>

class SPDesktop;

namespace Ui {
class ZoomWidget;
}

namespace Linea::UI {

class ZoomWidget : public QWidget {
    Q_OBJECT

public:
    explicit ZoomWidget(QWidget* parent = nullptr);
    ~ZoomWidget() override;

    void setDesktop(SPDesktop* desktop);

private:
    std::unique_ptr<Ui::ZoomWidget> _ui;
    SPDesktop* _desktop = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_ZOOM_WIDGET_H
