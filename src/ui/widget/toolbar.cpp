// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Generic reusable toolbar widget implementation.
 */

#include "toolbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStyle>
#include <QToolButton>
#include <QWidgetAction>
#include <memory>
#include <span>

#include "actions/action-registry.h"
#include "custom-menu.h"
#include "desktop.h"
#include "flow-layout.h"
#include "ui_toolbar.h"

namespace {

constexpr int BTN_H = 30;
constexpr int ARROW_BTN_W = 16;
constexpr int MARGIN = 5;

} // namespace

namespace Linea::UI {

// Note: setDefaultAction would have been simpler, but it adds popup menu indicator
// to the group button and there's no way to get rid of it.

QAction* Toolbar::getAction(QToolButton* button) const {
    auto a = button->property("action");
    return a.isValid() ? qvariant_cast<QAction*>(a) : nullptr;
}

void Toolbar::disconnectAction(QToolButton* button) {
    auto action = getAction(button);
    if (!action) return;

    QObject::disconnect(button, &QToolButton::clicked, action, &QAction::trigger);
    QObject::disconnect(action, &QAction::toggled, button, &QToolButton::setChecked);
}

void Toolbar::connectAction(QToolButton* button, QAction* action) {
    QObject::connect(button, &QToolButton::clicked, action, [button, action]() {
        action->trigger();
        button->setChecked(action->isChecked());
    });
    if (action->property("iconToggle").toBool()) {
        QObject::connect(action, &QAction::changed, button, [button, action]() { button->setIcon(action->icon()); });
    } else {
        QObject::connect(action, &QAction::toggled, button, &QToolButton::setChecked);
    }
    button->setProperty("action", QVariant::fromValue(action));
}

void Toolbar::setActionTooltip(QToolButton* button, QAction* action) {
    QString tooltip = action->toolTip();
    if (!action->shortcut().isEmpty()) {
        tooltip += QString(" (%1)").arg(action->shortcut().toString(QKeySequence::NativeText));
    }
    button->setToolTip(tooltip);
}

void Toolbar::reconnectAction(QToolButton* button, QAction* action) {
    auto current_action = getAction(button);
    if (current_action != action) {
        disconnectAction(button);
        connectAction(button, action);

        button->setIcon(action->icon());
        setActionTooltip(button, action);
    }

    button->setChecked(action->isChecked());
}

void Toolbar::setButtonSize(QToolButton* button) {
    button->setFixedSize(_button_size, _button_size);
    button->setIconSize(QSize(16, 16));
}

void Toolbar::bindAction(QToolButton* button, QAction* action) {
    button->setIcon(action->icon());
    bool iconToggle = action->property("iconToggle").toBool();
    button->setCheckable(action->isCheckable() && !iconToggle);
    button->setChecked(!iconToggle && action->isChecked());
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    setButtonSize(button);
    // button->setDefaultAction(action);
    // button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connectAction(button, action);

    setActionTooltip(button, action);

    // Listen for shortcut changes using sigc++
    ShortcutManager::instance().shortcutChanged.connect([this, button, action](const QString& actionId, const QKeySequence&) {
        if (actionId == action->objectName()) {
            setActionTooltip(button, action);
        }
    });
}

QToolButton* Toolbar::createMenuButton(std::span<const char* const> action_ids, const QString& label) {
    // Menu button
    auto menu_button = new QToolButton(this);
    menu_button->setText(label);
    _flow_layout->insertWidget(_insert_pos++, menu_button);

    if (!label.isEmpty()) {
        menu_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        menu_button->setLayoutDirection(Qt::RightToLeft);
        menu_button->setIcon(QIcon(":/icons/pan-down"));
        menu_button->setIconSize(QSize(16, 16));
        menu_button->setProperty("class", "tool-group-arrow");
    }
    auto menu = Linea::UI::createCustomMenu(menu_button, action_ids);
    menu->setLayoutDirection(Qt::LeftToRight);
    menu_button->setMenu(menu);
    menu_button->setFixedHeight(_button_size);
    menu_button->setPopupMode(QToolButton::InstantPopup);
    Linea::UI::installRightAlignedMenuFilter(menu, ARROW_BTN_W);

    return menu_button;
}

QToolButton* Toolbar::createArrowButton(QWidget* parent) {
    // Arrow button - just opens the menu
    auto arrow_button = new QToolButton(parent);
    arrow_button->setFixedSize(ARROW_BTN_W, _button_size);
    arrow_button->setProperty("class", "tool-group-arrow");
    arrow_button->setPopupMode(QToolButton::InstantPopup);
    arrow_button->setIcon(QIcon(":/icons/pan-down"));
    arrow_button->setIconSize(QSize(16, 16));
    arrow_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    return arrow_button;
}

/**
 * Creates a combo button:
 * - A normal push button (32x32) for main action
 * - A separate arrow button (16x32) for opening the popup menu
 */
QToolButton* Toolbar::createComboButton(std::span<const char* const> action_ids, QAction* button_action,
                                        bool supportCheckMarks) {
    // Group the two related buttons so the flow layout keeps them on one row.
    auto group = new QWidget(this);
    auto hbox = new QHBoxLayout(group);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(0);

    // Main tool button - normal 32x32 radio button
    auto tool_button = new QToolButton(group);
    bindAction(tool_button, button_action);
    hbox->addWidget(tool_button);

    // Arrow button - just opens the menu
    auto arrow_button = createArrowButton(group);
    // Create custom styled menu with aligned shortcuts
    auto menu = Linea::UI::createCustomMenu(arrow_button, action_ids, supportCheckMarks);
    arrow_button->setMenu(menu);

    Linea::UI::installRightAlignedMenuFilter(menu, ARROW_BTN_W);
    hbox->addWidget(arrow_button);

    _flow_layout->insertWidget(_insert_pos++, group);

    return tool_button;
}

/**
 * Creates a tool group with two separate buttons:
 * - A normal radio button (32x32) showing the active tool
 * - A separate arrow button (16x32) for opening the popup menu
 */
void Toolbar::createToolGroup(std::span<const char* const> action_ids, QAction*& current_action) {
    auto& a = ActionRegistry::get();

    auto tool_button = createComboButton(action_ids, current_action, false);

    _flow_layout->insertSpacing(_insert_pos++, MARGIN);

    // Connect menu actions: update combo button when one becomes checked.
    // toggled fires both from user interaction and from syncAllActions
    // (setChecked), so the combo button stays in sync regardless of how
    // the tool changed.
    for (auto action_id : action_ids) {
        if (std::string_view(action_id) == "-") continue;

        auto action = a.action(action_id);

        QObject::connect(action, &QAction::toggled, tool_button, [this, tool_button, action, &current_action](bool checked) {
            if (checked) {
                current_action = action;
                reconnectAction(tool_button, action);
            }
        });
    }
}

Toolbar::Toolbar(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::Toolbar>())
    , _button_size(BTN_H) {
    _ui->setupUi(this);
    _ui->containerWidget->setProperty("class", "toolbar-widget");
    _ui->containerWidget->setProperty("rounded", true);

    _flow_layout = new FlowLayout(_ui->containerWidget, -1, 0, 0);
    _flow_layout->setContentsMargins(5, 5, 5, 5);
    _ui->containerWidget->setLayout(_flow_layout);

    _insert_pos = _flow_layout->count();
}

void Toolbar::setMargins(int left, int top, int right, int bottom) {
    _flow_layout->setContentsMargins(left, top, right, bottom);
}

void Toolbar::setDefaultButtonSize(int size) {
    _button_size = size;
}

void Toolbar::setRounded(bool rounded) {
    if (_rounded == rounded) {
        return;
    }
    _rounded = rounded;
    _ui->containerWidget->setProperty("rounded", rounded);
    _ui->containerWidget->style()->unpolish(_ui->containerWidget);
    _ui->containerWidget->style()->polish(_ui->containerWidget);
    _ui->containerWidget->update();
}

bool Toolbar::rounded() const {
    return _rounded;
}

void Toolbar::setCentered(bool centered) {
    _flow_layout->setCentering(centered);
}

int Toolbar::heightForWidth(int width) const {
    return _ui->containerWidget ? _ui->containerWidget->heightForWidth(width) : -1;
}

QSize Toolbar::sizeForWidth(int width) const {
    return _flow_layout->sizeForWidth(width);
}

Toolbar::~Toolbar() = default;

QToolButton* Toolbar::addButton(const std::string& action_id) {
    auto& a = ActionRegistry::get();
    auto button = new QToolButton(this);
    bindAction(button, a.action(action_id));
    _flow_layout->insertWidget(_insert_pos++, button);
    return button;
}

void Toolbar::addToolGroup(std::span<const char* const> action_ids, QAction*& current_action) {
    createToolGroup(action_ids, current_action);
}

QToolButton* Toolbar::addMenuButton(std::span<const char* const> action_ids, const QString& label) {
    return createMenuButton(action_ids, label);
}

QToolButton* Toolbar::addDropDownButton(std::span<const Linea::UI::CustomMenuItem> items, const QString& label) {
    if (label.isEmpty()) {
        auto button = createArrowButton(this);
        auto menu = Linea::UI::createCustomMenu(button, items, true);
        button->setMenu(menu);
        Linea::UI::installRightAlignedMenuFilter(menu, ARROW_BTN_W);
        _flow_layout->insertWidget(_insert_pos++, button);
        return button;
    }
    else {
        //TODO: add normal btn
        qDebug() << "Toolbar::addDropDownButton: label is not empty, not implemented yet";
    }

    return nullptr;
}

QToolButton* Toolbar::addDynamicDropDownButton(std::function<std::vector<Linea::UI::CustomMenuItem>()> builder) {
    auto button = createArrowButton(this);
    auto menu = new QMenu(button);
    menu->setObjectName("customToolMenu");
    button->setMenu(menu);
    Linea::UI::installRightAlignedMenuFilter(menu, ARROW_BTN_W);

    // Rebuild the menu from the builder each time it's about to show.
    QObject::connect(menu, &QMenu::aboutToShow, menu, [menu, builder = std::move(builder)] {
        // Clear previous content.
        for (auto action : menu->actions()) {
            menu->removeAction(action);
            delete action;
        }

        auto items = builder();
        if (items.empty()) {
            auto placeholder = new QWidgetAction(menu);
            auto frame = new QFrame(menu);
            frame->setProperty("class", "custom-tool-item");
            auto hbox = new QHBoxLayout(frame);
            hbox->setContentsMargins(8, 6, 12, 6);
            auto label = new QLabel(QObject::tr("No recent files"), frame);
            label->setProperty("class", "menu-item-label");
            hbox->addWidget(label);
            placeholder->setDefaultWidget(frame);
            menu->addAction(placeholder);
            return;
        }

        createCustomMenuInto(menu, items, true);
    });

    _flow_layout->insertWidget(_insert_pos++, button);
    return button;
}

void Toolbar::addSpace() {
    _flow_layout->insertSpacing(_insert_pos++, MARGIN);
}

void Toolbar::addStretch() {
    _flow_layout->insertStretch(_insert_pos++);
}

void Toolbar::addWidget(QWidget* widget) {
    _flow_layout->insertWidget(_insert_pos++, widget);
}

QToolButton* Toolbar::addPushButton(const QString& label) {
    auto button = new QToolButton(this);
    button->setText(label);
    setButtonSize(button);
    _flow_layout->insertWidget(_insert_pos++, button);
    return button;
}

void Toolbar::addSplitMenuButton(std::span<const char* const> action_ids, const char* button_action) {
    auto& a = ActionRegistry::get();
    auto action = a.action(button_action);
    createComboButton(action_ids, action, true);
}

void Toolbar::finalizeLayout() {
    auto width = _flow_layout->sizeHint().width();
    auto minWidth = sizeForWidth(1).width();
    setMinimumWidth(minWidth);
    setMaximumWidth(width);
}

} // namespace Linea::UI
