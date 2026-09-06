// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ExportWidget — dedicated export panel with a tab strip and per-mode pages.
 *
 * The top row is a centered TabStrip that switches between export modes
 * (single, batch, …). Each page is laid out in a QGridLayout defined in the
 * accompanying .ui file.
 */
/*
 * Authors:
 *   Mike Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_EXPORT_WIDGET_H
#define LINEA_UI_EXPORT_WIDGET_H

#include <memory>
#include <QWidget>

class SPDesktop;

QT_BEGIN_NAMESPACE
namespace Ui {
class ExportWidget;
}
QT_END_NAMESPACE

namespace Linea::Props {
class Binder;
}

namespace Linea::UI {

class TabStrip;

/**
 * Panel for exporting the document, selection, pages, or custom areas.
 *
 * The caller is responsible for wiring:
 *   - setDesktop()  — once on creation / desktop switch
 */
class ExportWidget : public QWidget {
    Q_OBJECT

public:
    explicit ExportWidget(QWidget* parent = nullptr);
    ~ExportWidget() override;

    void bind(Props::Binder& binder);

private:
    void addSingleExport();
    void removeSingleExport(QWidget* task);
    void updateExportButtonVisibility();

    std::unique_ptr<Ui::ExportWidget> _ui;
    SPDesktop* _desktop = nullptr;
    int _curPage = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_EXPORT_WIDGET_H
