// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * LabelWidget - Widget for editing object label.
 *
 */

#ifndef LINEA_UI_LABEL_WIDGET_H
#define LINEA_UI_LABEL_WIDGET_H

#include <QWidget>
#include <memory>

#include "ui/operation-blocker.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class LabelWidget;
}
QT_END_NAMESPACE

class SPDesktop;

namespace Linea {

namespace Props {
class Binder;
}

namespace UI {

class DescriptionWidget;
class PopupMenu;
class TransformPanel;

/**
 * Widget for editing object label.
 *
 * Uses a grid layout:
 * - Row 0: "Selection" label
 * - Row 1: QLineEdit for editable label
 */
class LabelWidget : public QWidget {
    Q_OBJECT

public:
    explicit LabelWidget(QWidget* parent = nullptr);
    ~LabelWidget() override;

    void bind(Props::Binder& binder);

private Q_SLOTS:
    void onTransformClicked();

private:
    void refreshLabel();

    std::unique_ptr<Ui::LabelWidget> _ui;
    PopupMenu* _popup = nullptr;
    DescriptionWidget* _descriptionWidget = nullptr;
    PopupMenu* _transformPopup = nullptr;
    TransformPanel* _transformPanel = nullptr;
    SPDesktop* _desktop = nullptr;
    OperationBlocker _update;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_LABEL_WIDGET_H
