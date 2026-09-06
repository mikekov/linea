// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * DescriptionWidget - Widget for editing object title, description and ID.
 */

#ifndef LINEA_UI_DESCRIPTION_WIDGET_H
#define LINEA_UI_DESCRIPTION_WIDGET_H

#include <QWidget>
#include <memory>

#include "ui/operation-blocker.h"

class SPDocument;
class SPItem;

QT_BEGIN_NAMESPACE
namespace Ui {
class DescriptionWidget;
}
QT_END_NAMESPACE

namespace Linea {

namespace Props {
class Binder;
class Editor;
}

namespace UI {

/**
 * Widget for editing object title, description and ID.
 *
 * Uses a grid layout:
 * - Row 0: "Title" label
 * - Row 1: QLineEdit for title
 * - Row 2: "Description" label
 * - Row 3: QPlainTextEdit for description
 * - Row 4: "ID" label
 * - Row 5: QLineEdit for ID + "Set" QPushButton
 */
class DescriptionWidget : public QWidget {
    Q_OBJECT

public:
    explicit DescriptionWidget(QWidget* parent = nullptr);
    ~DescriptionWidget() override;

    void bind(Props::Binder& binder);

private:
    void validateId();
    void setId();

    std::unique_ptr<Ui::DescriptionWidget> _ui;
    OperationBlocker _update;
    Props::Editor* _editor = nullptr;
    SPDocument* _document = nullptr;
    SPItem* _item = nullptr;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_DESCRIPTION_WIDGET_H
