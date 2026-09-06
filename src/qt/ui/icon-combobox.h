// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * IconComboBox — push button with popup menu showing icon+label items.
 */

#ifndef LINEA_UI_ICON_COMBOBOX_H
#define LINEA_UI_ICON_COMBOBOX_H

#include <QIcon>
#include <QMap>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <sigc++/signal.h>

namespace Linea::UI {

/**
 * A push button that shows a popup menu with icon+label items.
 */
class IconComboBox : public QPushButton {
    Q_OBJECT

public:
    // What to show in the closed button
    enum HeaderType { ImageLabel, ImageOnly, LabelOnly };

    explicit IconComboBox(QWidget* parent = nullptr);
    ~IconComboBox() override = default;

    // Set what to display in the button when closed
    void setHeaderType(HeaderType type);
    HeaderType getHeaderType() const;

    // Add a row with icon name, label, and id
    void addRow(const QString& iconName, const QString& label, int id);
    // Add a row with icon name, full label, short label, and id
    void addRow(const QString& iconName, const QString& fullLabel, const QString& shortLabel, int id);
    // Add a custom widget to the popup, optionally at a specific position.
    void addPopupWidget(QWidget* widget, int index = -1);

    // Set the active item by id; -1 clears the selection (mixed state).
    void setActiveById(int id);
    // Get the current active item's id
    int getActiveRowId() const;

    // Show/hide a specific row by id
    void setRowVisible(int id, bool visible = true);

    // Enable/disable search in the popup menu
    void enableSearch(bool enable = true);

    // Signal emitted when selection changes (reports id)
    sigc::signal<void(int)>& signalChanged();

Q_SIGNALS:
    // Qt signal for selection changes
    void currentChanged(int id);

private:
    void updateButtonDisplay();
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    bool handleMousePress(QWidget* widget);
    bool handleKeyPress(QWidget* widget, QKeyEvent* keyEvent);
    bool handleHoverEnter(QWidget* widget);
    bool handleHoverLeave(QWidget* widget);
    void clearAllHighlights();
    void setHighlight(QWidget* widget, bool highlight);
    int findCurrentHighlightedIndex();
    void navigateToItem(int currentIndex, int direction);

    struct ItemData {
        QString iconName;
        QString label;
        QString shortLabel;
    };
    QMap<int, ItemData> _item_data;

    QMenu* _menu;
    int _current_id = -1;
    QString _current_label;
    QString _current_icon_name;
    HeaderType _header_type = ImageLabel;
    sigc::signal<void(int)> _signal_changed;
};

} // namespace Linea::UI

#endif // LINEA_UI_ICON_COMBOBOX_H
