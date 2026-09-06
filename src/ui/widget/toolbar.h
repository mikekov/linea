// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Generic reusable toolbar widget.
 */

#ifndef LINEA_UI_WIDGET_TOOLBAR_H
#define LINEA_UI_WIDGET_TOOLBAR_H

#include <QWidget>
#include <memory>
#include <span>
#include <string>
#include "ui/widget/custom-menu.h"

class FlowLayout;
class QToolButton;
class QAction;

QT_BEGIN_NAMESPACE
namespace Ui {
class Toolbar;
}
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * A generic toolbar widget that provides helpers for adding action-bound
 * buttons and tool groups with popup menus.
 * Subclass this to define specific toolbar content.
 */
class Toolbar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool rounded READ rounded WRITE setRounded)

public:
    explicit Toolbar(QWidget* parent = nullptr);
    ~Toolbar() override;

    // set button size before adding any buttons
    void setDefaultButtonSize(int size);

    // set margins around the toolbar
    void setMargins(int left, int top, int right, int bottom);

    // Add a single action-bound button at the current insertion position.
    QToolButton* addButton(const std::string& action_id);

    QToolButton* addPushButton(const QString& label);

    // Add a tool group (main button + arrow popup) for a set of actions.
    // current_action tracks which action is currently active for that group.
    void addToolGroup(std::span<const char* const> action_ids, QAction*& current_action);

    // Add a combo button (main button + arrow popup) for a set of actions.
    // button_action is the action that will be triggered when the main button is clicked.
    void addSplitMenuButton(std::span<const char* const> action_ids, const char* button_action);

    // Add a menu button (drop down menu) for a set of actions.
    QToolButton* addMenuButton(std::span<const char* const> action_ids, const QString& label);

    // Add a menu drop down button.
    QToolButton* addDropDownButton(std::span<const Linea::UI::CustomMenuItem> items, const QString& label);

    // Add a drop down button whose menu is rebuilt from `builder` each time
    // it is about to show. Use for dynamic content (e.g. recent files).
    QToolButton* addDynamicDropDownButton(std::function<std::vector<Linea::UI::CustomMenuItem>()> builder);

    // spring to push buttons to the right edge
    void addStretch();

    // add some space after current widget
    void addSpace();

    void addWidget(QWidget* widget);

    // Access the underlying layout insert position (for subclass customisation).
    int insertPos() const { return _insert_pos; }

    // Call after all buttons are added to size the toolbar to fit.
    void finalizeLayout();

    void setRounded(bool rounded);
    bool rounded() const;

    void setCentered(bool centered);

    int heightForWidth(int width) const override;
    QSize sizeForWidth(int width) const;

private:
    std::unique_ptr<Ui::Toolbar> _ui;
    FlowLayout* _flow_layout = nullptr;
    int _insert_pos = 0;
    bool _rounded = true;
    int _button_size = 0;

    void setButtonSize(QToolButton* button);
    int buttonSize() const { return _button_size; }

    QAction* getAction(QToolButton* button) const;
    void disconnectAction(QToolButton* button);
    void connectAction(QToolButton* button, QAction* action);
    void setActionTooltip(QToolButton* button, QAction* action);
    void reconnectAction(QToolButton* button, QAction* action);
    void bindAction(QToolButton* button, QAction* action);

    QToolButton* createMenuButton(std::span<const char* const> action_ids, const QString& label);
    QToolButton* createArrowButton(QWidget* parent);
    QToolButton* createComboButton(std::span<const char* const> action_ids, QAction* button_action,
                                   bool supportCheckMarks);
    void createToolGroup(std::span<const char* const> action_ids, QAction*& current_action);
};

} // namespace Linea::UI

#endif // LINEA_UI_WIDGET_TOOLBAR_H
