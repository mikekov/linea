// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * OpacityWidget - Widget for editing object opacity.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_OPACITY_WIDGET_H
#define LINEA_UI_OPACITY_WIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Linea {

namespace Props {
class Binder;
}

namespace UI {

class SpinScale;

/**
 * Widget for editing object opacity.
 *
 * Uses a vertical layout:
 * - Row 1: "Opacity" label
 * - Row 2: SpinScale widget for opacity control
 */
class OpacityWidget : public QWidget {
    Q_OBJECT

public:
    explicit OpacityWidget(QWidget* parent = nullptr);
    ~OpacityWidget() override;

    // Bind opacity percentage to the 0..1 model; also installs the visibility
    // rule (shown when any items are selected).
    void bind(Props::Binder& binder);

private:
    QVBoxLayout* _layout = nullptr;
    QLabel* _label = nullptr;
    SpinScale* _spinScale = nullptr;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_OPACITY_WIDGET_H
