// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Custom menu helper implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 */

#include "custom-menu.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QScreen>
#include <QStyle>
#include <QWidgetAction>
#include <functional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "actions/action-registry.h"
#include "qt/ui/icon-widget.h"

namespace Linea::UI {

namespace {

// std::string normalizeActionId(std::string_view action_id) {
//     for (auto prefix : {std::string_view{"app."}, std::string_view{"win."}, std::string_view{"doc."},
//                         std::string_view{"ctx."}}) {
//         if (action_id.starts_with(prefix)) {
//             return std::string(action_id.substr(prefix.size()));
//         }
//     }
//     return std::string(action_id);
// }

QString stripGtkMnemonics(QString label) {
    QString result;
    result.reserve(label.size());
    for (int i = 0; i < label.size(); ++i) {
        if (label[i] == QLatin1Char('_')) {
            if (i + 1 < label.size() && label[i + 1] == QLatin1Char('_')) {
                result.append(QLatin1Char('_'));
                ++i;
            }
            continue;
        }
        result.append(label[i]);
    }
    return result;
}

void repolishWithChildren(QWidget* widget) {
    if (!widget) return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    for (auto child : widget->findChildren<QWidget*>()) {
        child->style()->unpolish(child);
        child->style()->polish(child);
    }
}

// Event filter for custom menu item frames. Mirrors hover state into the
// "hovered" dynamic property (used by the QSS for highlight colors). When
// `action` and `rootMenu` are set it also triggers the action and closes the
// popup on click; for submenu entries both are null so the click is left for
// Qt to handle (opening the attached submenu).
class MenuItemEventFilter : public QObject {
    QAction* action;
    QMenu* rootMenu;
    std::function<void()> onTrigger;

public:
    MenuItemEventFilter(QAction* a, QMenu* root, QObject* parent = nullptr, std::function<void()> trigger = {})
        : QObject(parent)
        , action(a)
        , rootMenu(root)
        , onTrigger(std::move(trigger)) {}

    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
            if (auto widget = qobject_cast<QWidget*>(obj)) {
                widget->setProperty("hovered", event->type() == QEvent::HoverEnter);
                repolishWithChildren(widget);
            }
        } else if (event->type() == QEvent::MouseButtonRelease && rootMenu) {
            if (action) {
                action->trigger();
            } else if (onTrigger) {
                onTrigger();
            }
            rootMenu->close();
            return true;
        }
        return QObject::eventFilter(obj, event);
    }
};

class RightAlignedMenuFilter : public QObject {
    QMenu* menu;
    int arrow_width;

public:
    RightAlignedMenuFilter(QMenu* m, int w)
        : QObject(m)
        , menu(m)
        , arrow_width(w) {}

    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Show) {
            auto button = qobject_cast<QWidget*>(menu->parentWidget());
            if (button) {
                QPoint br = button->mapToGlobal(QPoint(button->width(), button->height()));

                int x = br.x() - menu->width() + arrow_width;
                int y = br.y() + 1;

                if (auto screen = button->screen()) {
                    const QRect geo = screen->availableGeometry();

                    if (x + menu->width() > geo.right()) x = geo.right() - menu->width();
                    if (x < geo.left()) x = geo.left();

                    if (y + menu->height() > geo.bottom()) y = br.y() - menu->height() - 1;
                    if (y < geo.top()) y = geo.top();
                }

                menu->move(x, y);
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

} // namespace

CustomMenuItem actionToCustomMenuItem(std::string_view action_id, QString label) {
    if (action_id == "-") {
        return {.title = "-"};
    }

    // Close the current submenu.
    if (action_id == "<") {
        return {.submenuEnd = true};
    }

    // Open a new submenu: ">Title" starts a submenu labelled "Title".
    if (action_id.starts_with(">")) {
        CustomMenuItem item;
        item.submenuStart = true;
        item.title = QString::fromUtf8(action_id.substr(1));
        return item;
    }

    auto& registry = ActionRegistry::get();
    std::string id(action_id);
    auto action = registry.action(id);

    return {
        .action = std::move(id),
        .title = label.isEmpty() ? action->text() : stripGtkMnemonics(std::move(label)),
        .shortcut = action->shortcut().toString(QKeySequence::NativeText),
        .icon = action->icon(),
    };
}

namespace {

// Shared state across the recursive build: the root menu (used by click
// handlers so any action closes the whole popup chain), the check-mark
// widgets to refresh on show, every menu created (for aboutToShow hooks),
// and the check-mark policy flag.
struct MenuBuildContext {
    QMenu* rootMenu;
    bool supportCheckMarks;
    std::vector<std::pair<QAction*, IconWidget*>> checkIcons;
    std::vector<QMenu*> menus;
};

// Shared frame result: the widget action, its frame, the hbox layout to
// append trailing widgets (stretch/arrow for submenus, shortcut for actions),
// and the check-mark icon (null if none).
struct MenuItemFrame {
    QWidgetAction* widgetAction;
    QFrame* frame;
    QHBoxLayout* hbox;
    IconWidget* checkIcon = nullptr;
};

// Creates a styled menu item frame (QWidgetAction + QFrame) with the standard
// cursor/hover/class styling, click event filter, hbox layout, and leading
// check mark, icon, and text. Adds the action to `menu` and returns the
// frame components for further customization (e.g. submenu arrow, shortcut).
MenuItemFrame createStyledMenuItemFrame(QMenu* menu, QMenu* rootMenu, bool supportCheckMarks,
                                        QAction* action, const QIcon& icon, QSize iconSize,
                                        const QString& title, const QString& description,
                                        std::function<void()> onTrigger = {}) {
    auto widgetAction = new QWidgetAction(menu);
    auto frame = new QFrame(menu);
    frame->setCursor(Qt::PointingHandCursor);
    frame->setAttribute(Qt::WA_Hover);
    frame->setProperty("class", "custom-tool-item");
    frame->installEventFilter(new MenuItemEventFilter(action, rootMenu, frame, std::move(onTrigger)));

    auto hbox = new QHBoxLayout(frame);
    hbox->setContentsMargins(8, 6, 12, 6);
    hbox->setSpacing(10);

    widgetAction->setDefaultWidget(frame);
    menu->addAction(widgetAction);

    // check mark
    IconWidget* checkIcon = nullptr;
    if (action && supportCheckMarks && action->isCheckable()) {
        checkIcon = new IconWidget(frame);
        checkIcon->setIcon(action->isChecked() ? QIcon(":/icons/check-mark") : QIcon());
        hbox->addWidget(checkIcon);
    }

    // icon
    if (!icon.isNull()) {
        auto iconWidget = new IconWidget(frame);
        iconWidget->setIcon(icon);
        iconWidget->setIconSize(iconSize);
        hbox->addWidget(iconWidget);
    }

    // text (expands to fill space)
    auto textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(0);
    auto textLabel = new QLabel(title, frame);
    textLabel->setProperty("class", "menu-item-label");
    textLayout->addWidget(textLabel);

    if (!description.isEmpty()) {
        auto descriptionLabel = new QLabel(description, frame);
        descriptionLabel->setProperty("class", "menu-item-description");
        textLayout->addWidget(descriptionLabel);
    }

    hbox->addLayout(textLayout);

    return {widgetAction, frame, hbox, checkIcon};
}

// Adds items to `menu` starting at index `i`, recursing into submenus.
// Returns the index of the next item to process (just past a matching
// submenuEnd marker, or items.size() when the input runs out).
size_t buildMenuItems(QMenu* menu, std::span<const CustomMenuItem> items, size_t i, MenuBuildContext& ctx) {
    auto& registry = ActionRegistry::get();

    while (i < items.size()) {
        auto& item = items[i++];

        if (item.submenuEnd) {
            return i;
        }

        if (item.submenuStart) {
            auto subMenu = new QMenu(item.title, menu);
            subMenu->setObjectName("customToolMenu");
            ctx.menus.push_back(subMenu);

            // Render the submenu entry as a custom frame so it matches the
            // height/indentation of the other items. The submenu is attached
            // via setMenu(); Qt opens it on hover/click over the action.
            auto [widgetAction, frame, hbox, checkIcon] =
                createStyledMenuItemFrame(menu, ctx.rootMenu, ctx.supportCheckMarks, nullptr, {}, {}, item.title, {});

            hbox->addStretch();

            auto arrow = new IconWidget(frame);
            arrow->setIcon(QIcon(":/icons/pan-end"));
            arrow->setIconSize(QSize(16, 16));
            hbox->addWidget(arrow);

            widgetAction->setMenu(subMenu);

            i = buildMenuItems(subMenu, items, i, ctx);
            continue;
        }

        if (item.title == "-") {
            menu->addSeparator();
            continue;
        }

        auto action_id = item.action;
        auto action = action_id.empty() ? nullptr : registry.action(action_id);
        auto [widgetAction, frame, hbox, checkIcon] =
            createStyledMenuItemFrame(menu, ctx.rootMenu, ctx.supportCheckMarks,
                                      action, item.icon, item.iconSize, item.title, item.description, item.onTrigger);

        if (checkIcon) {
            ctx.checkIcons.emplace_back(action, checkIcon);
        }

        // Shortcut (right-aligned, consistent width)
        if (!item.shortcut.isEmpty()) {
            auto shortcutLabel = new QLabel(item.shortcut, frame);
            shortcutLabel->setProperty("class", "menu-shortcut");
            shortcutLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            shortcutLabel->setMinimumWidth(40);
            hbox->addWidget(shortcutLabel);
        } else {
            hbox->addSpacing(40);
        }
    }

    return i;
}

} // namespace

void createCustomMenuInto(QMenu* menu, std::span<const CustomMenuItem> items, bool supportCheckMarks) {
    if (!menu) return;

    MenuBuildContext ctx{menu, supportCheckMarks, {}, {menu}};
    buildMenuItems(menu, items, 0, ctx);

    // Refresh check marks for every menu just before it is shown.
    for (auto m : ctx.menus) {
        QObject::connect(m, &QMenu::aboutToShow, m, [checkIcons = ctx.checkIcons] {
            for (auto [action, icon] : checkIcons) {
                icon->setIcon(action->isChecked() ? QIcon(":/icons/check-mark") : QIcon());
            }
        });
    }
}

void appendCustomAction(QMenu* menu, const CustomMenuItem& item) {
    if (!menu) return;

    createCustomMenuInto(menu, std::span<const CustomMenuItem>(&item, 1), true);
}

void appendCustomSeparator(QMenu* menu) {
    if (!menu) return;

    menu->addSeparator();
}

QMenu* appendCustomSubmenu(QMenu* menu, const QString& title) {
    if (!menu) return nullptr;

    auto submenu = new QMenu(title, menu);
    submenu->setObjectName("customToolMenu");

    // Render the submenu entry as a custom frame so it matches the styling
    // of normal items. The submenu is attached via setMenu(); Qt opens it
    // on hover/click over the action.
    auto [widgetAction, frame, hbox, checkIcon] =
        createStyledMenuItemFrame(menu, menu, true, nullptr, {}, {}, title, {});

    hbox->addStretch();

    auto arrow = new IconWidget(frame);
    arrow->setIcon(QIcon(":/icons/pan-end"));
    arrow->setIconSize(QSize(16, 16));
    hbox->addWidget(arrow);

    widgetAction->setMenu(submenu);
    return submenu;
}

QMenu* createCustomMenu(QWidget* parent, std::span<const CustomMenuItem> items, bool supportCheckMarks) {
    auto menu = new QMenu(parent);
    menu->setObjectName("customToolMenu");
    createCustomMenuInto(menu, items, supportCheckMarks);
    return menu;
}

QMenu* createCustomMenu(QWidget* parent, std::span<const char* const> action_ids, bool supportCheckMarks) {
    std::vector<CustomMenuItem> items;
    for (auto action_id : action_ids) {
        items.push_back(actionToCustomMenuItem(action_id));
    }
    return createCustomMenu(parent, items, supportCheckMarks);
}

void installRightAlignedMenuFilter(QMenu* menu, int arrow_width) {
    menu->installEventFilter(new RightAlignedMenuFilter(menu, arrow_width));
}

} // namespace Linea::UI
