// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A color selector with RGB, CMYK, CMS, HSL, and Wheel pages
 */

#ifndef LINEA_UI_COLOR_NOTEBOOK_H
#define LINEA_UI_COLOR_NOTEBOOK_H

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <memory>

#include "colors/color-set.h"

class SPDesktop;
class SPDocument;

namespace Linea::UI {

class IconComboBox;
class ColorEntry;
class ColorPage;

class ColorNotebook : public QWidget {
    Q_OBJECT

public:
    explicit ColorNotebook(SPDesktop* desktop, std::shared_ptr<Inkscape::Colors::ColorSet> color, QWidget* parent = nullptr);
    ~ColorNotebook() override;

    void setLabel(const QString& label);
    void setCurrentColor(std::shared_ptr<Inkscape::Colors::ColorSet>& colors);
    // show/hide color space selector
    void setSwitcherVisible(bool visible);
    // show/hide "too much ink" warning icon
    void setInkWarningVisible(bool visible);
    // show/hide "color managed" icon
    void setColorManagedVisible(bool visible);
    // show/hide dropper (pick from canvas) button
    void setDropperVisible(bool visible);

private:
    void initUI();
    void switchToSpace(std::shared_ptr<Inkscape::Colors::Space::AnySpace>& space);
    void setDocument(SPDocument* document);

    std::shared_ptr<Inkscape::Colors::ColorSet> _colors;
    std::shared_ptr<Inkscape::Colors::Space::AnySpace> _current_space;
    ColorPage* _current_page = nullptr;
    IconComboBox* _combo = nullptr;
    QWidget* _buttonbox = nullptr;
    QLabel* _label = nullptr;
    ColorEntry* _rgba_entry = nullptr;
    QLabel* _colormanaged = nullptr;
    QLabel* _outofgamut = nullptr;
    QLabel* _toomuchink = nullptr;
    QPushButton* _btn_picker = nullptr;
    QGridLayout* _main_layout = nullptr;
    std::vector<std::shared_ptr<Inkscape::Colors::Space::AnySpace>> _pickers;
    SPDocument* _document = nullptr;
    sigc::scoped_connection _doc_replaced_connection;
    sigc::scoped_connection _onetimepick;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_NOTEBOOK_H
