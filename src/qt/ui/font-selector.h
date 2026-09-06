// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FontSelector — widget with line edit and button for font selection.
 */

#ifndef LINEA_UI_FONT_SELECTOR_H
#define LINEA_UI_FONT_SELECTOR_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>

#include "popup-menu.h"
#include "font-list.h"
#include "icon-combobox.h"

namespace Linea::UI {

/**
 * Font selector widget consisting of a line edit with a button at the right.
 * The button displays an icon and opens a popup with a font list.
 */
class FontSelector : public QWidget {
    Q_OBJECT

public:
    explicit FontSelector(QWidget* parent = nullptr);
    ~FontSelector() override;

    // Get the line edit widget for direct access
    QLineEdit* lineEdit() const { return _lineEdit; }

    // Get the button widget for direct access
    QPushButton* button() const { return _button; }

Q_SIGNALS:
    // Emitted when a font is selected
    void fontSelected(const QString& fontFamily);

private:
    void setupUI();
    void setupPopup();
    void showPopup();

    QLineEdit* _lineEdit = nullptr;
    QPushButton* _button = nullptr;
    PopupMenu* _popup = nullptr;
    QWidget* _popupContent = nullptr;
    FontList* _fontList = nullptr;
    QLineEdit* _searchBox = nullptr;
    IconComboBox* _sortComboBox = nullptr;
    QPushButton* _optionsButton = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_FONT_SELECTOR_H
