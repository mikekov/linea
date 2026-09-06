// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * TransformPanel — move/scale/rotate/skew and matrix transform panel
 */
/*
 * Authors:
 *   Mike Kowalski
 *
 * Copyright (C) 2025-2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_TRANSFORM_PANEL_H
#define LINEA_UI_TRANSFORM_PANEL_H

#include <memory>
#include <QWidget>

class SPDesktop;

QT_BEGIN_NAMESPACE
namespace Ui {
class TransformPanel;
}
QT_END_NAMESPACE

namespace Inkscape {
class Selection;
}

namespace Linea::UI {

class TabStrip;

/**
 * Panel for applying move, scale, rotate, skew, and affine matrix transforms
 * to the current selection.
 *
 * Two pages (tabs):
 *   - Transforms: Move / Scale / Rotate / Skew controls
 *   - Matrix:     Direct 2-D affine matrix (A–F) editor
 *
 * The caller is responsible for wiring:
 *   - setDesktop()  — once on creation / desktop switch
 *   - updateUi()    — whenever the selection changes
 */
class TransformPanel : public QWidget {
    Q_OBJECT

public:
    explicit TransformPanel(QWidget* parent = nullptr);
    ~TransformPanel() override;

    void setDesktop(SPDesktop* desktop);
    void updateUi(Inkscape::Selection& selection);

    // Select visible page (0 = transforms, 1 = matrix)
    void setPage(int page);

private:
    static constexpr int PageTransforms = 0;
    static constexpr int PageMatrix     = 1;

    void setupTabStrip();
    void setupConnections();
    void setupIcons();

    void showPage(int page);
    void resetToDefaults();
    void clearMatrix();
    void onApply(bool duplicate);

    std::unique_ptr<Ui::TransformPanel> _ui;
    QWidget* _tabTransform = nullptr;
    QWidget* _tabMatrix    = nullptr;
    SPDesktop* _desktop    = nullptr;
    int _curPage           = PageTransforms;
    bool _linkedScale      = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_TRANSFORM_PANEL_H
