// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * LpeWidget — Qt widget for managing Live Path Effects on an item.
 *
 * Provides an "Add" button that pops up a searchable list of applicable
 * LPEs, and a list of currently-applied effects with remove buttons.
 * Clicking an applied effect opens a PopupMenu showing its parameter widget.
 */

#ifndef LINEA_UI_LPE_WIDGET_H
#define LINEA_UI_LPE_WIDGET_H

#include <QWidget>
#include <memory>

#include "ui/operation-blocker.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QListWidget;
class QListWidgetItem;
QT_END_NAMESPACE

class SPObject;
class SPLPEItem;
class SPDesktop;
class SPDocument;

namespace Inkscape::LivePathEffect {
class Effect;
}

namespace Linea::Props {
class Binder;
}

namespace Ui {
class LpeWidget;
}

namespace Linea::UI {

class PopupMenu;

/**
 * Widget for managing Live Path Effects on a selected item.
 *
 * Mimics the GTK AttributesPanel::add_lpes functionality:
 *  - "Add" button opens a PopupMenu with a searchable list of applicable LPEs
 *  - Applied LPEs are listed with icon, name, and a remove button
 *  - Clicking an applied LPE opens a PopupMenu with its parameter widget
 */
class LpeWidget : public QWidget {
    Q_OBJECT

public:
    explicit LpeWidget(QWidget* parent = nullptr);
    ~LpeWidget() override;

    /// Set the object whose LPEs are managed. Pass nullptr to clear.
    void setObject(SPObject* object);

    /// Set the desktop (needed for icon loading and document access).
    void setDesktop(SPDesktop* desktop);

    /// Refresh the list of applied LPEs from the current object.
    void refreshAppliedLpes();

    /// Declarative binding: tracks selection changes and drives visibility.
    void bind(Linea::Props::Binder& binder);

private Q_SLOTS:
    void onAddButtonClicked();
    void onSearchChanged();

private:
    void populateAddPopup();
    void applyLpe(int lpeTypeInt);
    void removeLpe(int index);
    void showLpeParams(int index, QWidget* anchorWidget);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::unique_ptr<Ui::LpeWidget> _ui;

    // Add-LPE popup
    PopupMenu* _addPopup = nullptr;
    QLineEdit* _searchEdit = nullptr;
    QListWidget* _addList = nullptr;

    // Parameter popup (one at a time)
    std::unique_ptr<PopupMenu> _paramPopup;

    SPObject* _object = nullptr;
    SPDesktop* _desktop = nullptr;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_LPE_WIDGET_H
