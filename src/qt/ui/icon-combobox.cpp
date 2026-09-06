// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * IconComboBox implementation
 */

#include "icon-combobox.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QStyle>
#include <QWidgetAction>
#include <QEvent>

namespace Linea::UI {

namespace {

int nextNavigationIndex(int currentIndex, int direction, int itemCount) {
    if (currentIndex < 0 || itemCount <= 0) {
        return -1;
    }

    const auto nextIndex = currentIndex + direction;
    return nextIndex >= 0 && nextIndex < itemCount ? nextIndex : -1;
}

} // namespace

IconComboBox::IconComboBox(QWidget* parent)
    : QPushButton(parent)
{
    setObjectName("IconComboBox");
    _menu = new QMenu(this);
    setMenu(_menu);
    setFocusPolicy(Qt::StrongFocus); // Allow tab focus and keyboard interaction

    // Clear hover state when menu is about to show and highlight current item
    connect(_menu, &QMenu::aboutToShow, this, [this]() {
        for (auto action : _menu->actions()) {
            if (auto widgetAction = qobject_cast<QWidgetAction*>(action)) {
                if (auto widget = widgetAction->defaultWidget()) {
                    widget->setProperty("class", "");
                    widget->style()->unpolish(widget);
                    widget->style()->polish(widget);
                    // Highlight the current item and give it focus
                    if (widgetAction->data().toInt() == _current_id) {
                        widget->setProperty("class", "selected-menu-item");
                        widget->style()->unpolish(widget);
                        widget->style()->polish(widget);
                        widget->setFocus();
                    }
                }
            }
        }
    });
}

void IconComboBox::addRow(const QString& iconName, const QString& label, int id) {
    addRow(iconName, label, label, id);
}

void IconComboBox::addRow(const QString& iconName, const QString& fullLabel, const QString& shortLabel, int id) {
    // Store the item data
    ItemData data;
    data.iconName = iconName;
    data.label = fullLabel;
    data.shortLabel = shortLabel;
    _item_data[id] = data;

    // Create a custom widget for the menu item
    auto widget = new QWidget();
    widget->setAttribute(Qt::WA_Hover); // Enable hover events
    widget->setFocusPolicy(Qt::StrongFocus); // Enable keyboard focus
    auto layout = new QHBoxLayout(widget);

    if (!iconName.isEmpty()) {
        layout->setContentsMargins(8, 4, 8, 4);
        auto iconLabel = new QLabel();
        QIcon icon(QString(":/icons/%1").arg(iconName));
        iconLabel->setPixmap(icon.pixmap(16, 16));
        layout->addWidget(iconLabel);
    }
    else {
        layout->setContentsMargins(16, 4, 16, 4);
    }

    auto textLabel = new QLabel(fullLabel);
    textLabel->setToolTip(shortLabel);

    layout->addWidget(textLabel);
    layout->addStretch();

    // Create QWidgetAction and set the custom widget
    auto action = new QWidgetAction(this);
    action->setDefaultWidget(widget);
    action->setData(id);

    // Make the widget clickable and focusable
    widget->setProperty("action", QVariant::fromValue(action));
    widget->installEventFilter(this);

    _menu->addAction(action);

    // If this is the first item, make it active
    if (_current_id == -1) {
        setActiveById(id);
    }
}

void IconComboBox::addPopupWidget(QWidget* widget, int index) {
    if (!widget) {
        return;
    }

    auto action = new QWidgetAction(this);
    action->setDefaultWidget(widget);

    const auto actions = _menu->actions();
    if (index < 0 || index >= actions.size()) {
        _menu->addAction(action);
    } else {
        _menu->insertAction(actions[index], action);
    }
}

void IconComboBox::setActiveById(int id) {
    if (_current_id == id) return;

    if (id < 0) {
        _current_id = -1;
        _current_label = _current_icon_name = QString();
        updateButtonDisplay();
        _signal_changed.emit(id);
        Q_EMIT currentChanged(id);
        return;
    }

    // Check if the id exists
    if (!_item_data.contains(id)) return;

    _current_id = id;
    _current_label = _item_data[id].label;
    _current_icon_name = _item_data[id].iconName;

    updateButtonDisplay();
    _signal_changed.emit(id);
    Q_EMIT currentChanged(id);
}

void IconComboBox::setHeaderType(HeaderType type) {
    _header_type = type;
    updateButtonDisplay();
}

IconComboBox::HeaderType IconComboBox::getHeaderType() const {
    return _header_type;
}

int IconComboBox::getActiveRowId() const {
    return _current_id;
}

void IconComboBox::setRowVisible(int id, bool visible) {
    for (auto action : _menu->actions()) {
        if (auto widgetAction = qobject_cast<QWidgetAction*>(action)) {
            if (widgetAction->data().toInt() == id) {
                action->setVisible(visible);
                break;
            }
        }
    }
}

sigc::signal<void(int)>& IconComboBox::signalChanged() {
    return _signal_changed;
}

void IconComboBox::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        if (_item_data.isEmpty()) return;

        // Get the list of IDs in order
        QList<int> ids = _item_data.keys();
        int currentIndex = ids.indexOf(_current_id);

        const auto direction = event->key() == Qt::Key_Up ? -1 : 1;
        const auto newIndex = nextNavigationIndex(currentIndex, direction, ids.size());
        if (newIndex < 0) {
            event->accept();
            return;
        }

        setActiveById(ids[newIndex]);
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // Open the popup on Enter/Return
        showMenu();
        event->accept();
        return;
    }

    QPushButton::keyPressEvent(event);
}

void IconComboBox::updateButtonDisplay() {
    if (_current_id == -1) {
        setText(tr("Select..."));
        setIcon(QIcon());
        return;
    }

    // Display based on header type
    QString displayText;
    QIcon displayIcon;

    switch (_header_type) {
        case ImageLabel:
            displayText = _current_label;
            displayIcon = QIcon(QString(":/icons/%1").arg(_current_icon_name));
            break;
        case ImageOnly:
            displayText = "";
            displayIcon = QIcon(QString(":/icons/%1").arg(_current_icon_name));
            break;
        case LabelOnly:
            displayText = _current_label;
            displayIcon = QIcon();
            break;
    }

    // Elide text to leave room for arrow indicator and icon
    if (!displayText.isEmpty()) {
        QFontMetrics metrics(font());
        int availableWidth = width() - 20; // 20px for arrow indicator
        if (!displayIcon.isNull()) {
            availableWidth -= iconSize().width() + 5; // Subtract icon width and spacing
        }

        if (availableWidth > 0 && metrics.horizontalAdvance(displayText) > availableWidth) {
            displayText = metrics.elidedText(displayText, Qt::ElideRight, availableWidth);
        }
    }

    setText(displayText);
    setIcon(displayIcon);
}

void IconComboBox::resizeEvent(QResizeEvent* event) {
    QPushButton::resizeEvent(event);
    updateButtonDisplay();
}

bool IconComboBox::eventFilter(QObject* watched, QEvent* event) {
    if (auto widget = qobject_cast<QWidget*>(watched)) {
        switch (event->type()) {
            case QEvent::MouseButtonPress:
                return handleMousePress(widget);
            case QEvent::KeyPress:
                return handleKeyPress(widget, static_cast<QKeyEvent*>(event));
            case QEvent::Enter:
                return handleHoverEnter(widget);
            case QEvent::Leave:
                return handleHoverLeave(widget);
            default:
                break;
        }
    }
    return QPushButton::eventFilter(watched, event);
}

bool IconComboBox::handleMousePress(QWidget* widget) {
    if (auto action = widget->property("action").value<QWidgetAction*>()) {
        int id = action->data().toInt();
        setActiveById(id);
        setHighlight(widget, false);
        _menu->close();
        return true;
    }
    return false;
}

bool IconComboBox::handleKeyPress(QWidget* widget, QKeyEvent* keyEvent) {
    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
        if (auto action = widget->property("action").value<QWidgetAction*>()) {
            int id = action->data().toInt();
            setActiveById(id);
            setHighlight(widget, false);
            _menu->close();
            return true;
        }
    } else if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
        int direction = (keyEvent->key() == Qt::Key_Up) ? -1 : 1;
        int currentIndex = findCurrentHighlightedIndex();
        if (currentIndex >= 0) {
            navigateToItem(currentIndex, direction);
        }
        return true;
    } else if (keyEvent->key() == Qt::Key_Escape) {
        _menu->close();
        return true;
    }
    return false;
}

bool IconComboBox::handleHoverEnter(QWidget* widget) {
    clearAllHighlights();
    setHighlight(widget, true);
    return true;
}

bool IconComboBox::handleHoverLeave(QWidget* widget) {
    setHighlight(widget, false);
    return true;
}

void IconComboBox::clearAllHighlights() {
    for (auto action : _menu->actions()) {
        if (auto widgetAction = qobject_cast<QWidgetAction*>(action)) {
            if (auto widget = widgetAction->defaultWidget()) {
                setHighlight(widget, false);
            }
        }
    }
}

void IconComboBox::setHighlight(QWidget* widget, bool highlight) {
    widget->setProperty("class", highlight ? "selected-menu-item" : "");
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

int IconComboBox::findCurrentHighlightedIndex() {
    auto actions = _menu->actions();

    // Find the currently selected item (with selected-menu-item class)
    for (int i = 0; i < actions.size(); ++i) {
        if (auto widgetAction = qobject_cast<QWidgetAction*>(actions[i])) {
            if (auto widget = widgetAction->defaultWidget()) {
                if (widget->property("class").toString() == "selected-menu-item") {
                    return i;
                }
            }
        }
    }

    // If no item is selected, start from the current _current_id
    for (int i = 0; i < actions.size(); ++i) {
        if (auto widgetAction = qobject_cast<QWidgetAction*>(actions[i])) {
            if (widgetAction->data().toInt() == _current_id) {
                return i;
            }
        }
    }

    return -1;
}

void IconComboBox::navigateToItem(int currentIndex, int direction) {
    auto actions = _menu->actions();
    const auto newIndex = nextNavigationIndex(currentIndex, direction, actions.size());
    if (newIndex < 0) {
        return;
    }

    if (auto newWidgetAction = qobject_cast<QWidgetAction*>(actions[newIndex])) {
        if (auto newWidget = newWidgetAction->defaultWidget()) {
            clearAllHighlights();
            setHighlight(newWidget, true);
            newWidget->setFocus();
        }
    }
}

void IconComboBox::enableSearch(bool enable) {
    // TODO: Implement search functionality
    // _search_enabled = enable;
}

} // namespace Linea::UI
