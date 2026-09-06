// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPickerPanel — hosts a ColorPage and an optional color wheel/plate,
 * plus a bottom bar with dropper, hex-RGB edit and color-space selector.
 */

#ifndef LINEA_UI_COLOR_PICKER_PANEL_H
#define LINEA_UI_COLOR_PICKER_PANEL_H

#include <memory>
#include <QWidget>
#include <sigc++/signal.h>

QT_BEGIN_NAMESPACE
class QMenu;
namespace Ui {
class ColorPickerPanel;
}
QT_END_NAMESPACE

#include "color-entry.h"
#include "color-preview.h"

#include "colors/color-set.h"
#include "colors/color.h"
#include "colors/spaces/enum.h"

class SPDesktop;

namespace Linea::UI {

class ColorPage;
class ColorWheel;

/**
 * A widget hosting a ColorPage with an optional color plate/wheel on top and
 * a bottom bar containing a dropper button, RGB hex entry and color-space selector.
 */
class ColorPickerPanel : public QWidget {
    Q_OBJECT
public:
    // color plate type — rectangular, color wheel, no plate (only sliders)
    enum PlateType { Rect, Circle, None };

    static std::unique_ptr<ColorPickerPanel> create(
        Inkscape::Colors::Space::Type space,
        PlateType type,
        std::shared_ptr<Inkscape::Colors::ColorSet> color,
        QWidget* parent = nullptr);

    ~ColorPickerPanel() override;

    void setDesktop(SPDesktop* desktop);
    void setColor(const Inkscape::Colors::Color& color);
    // request color type/space change
    void setPickerType(Inkscape::Colors::Space::Type type);
    // request type of color wheel/plate
    void setPlateType(PlateType plate);
    PlateType getPlateType() const;

    sigc::signal<void(Inkscape::Colors::Space::Type)>& colorSpaceChanged();

Q_SIGNALS:
    void colorSpaceChangedQt(int spaceType);

private:
    explicit ColorPickerPanel(
        Inkscape::Colors::Space::Type space,
        PlateType type,
        std::shared_ptr<Inkscape::Colors::ColorSet> color,
        QWidget* parent);

    void buildBottomBar();
    void createColorPage(Inkscape::Colors::Space::Type type, PlateType plateType);
    void removeWidgets();
    void switchPage(Inkscape::Colors::Space::Type space, PlateType plateType);
    void updateColor();
    void pickColor(); // TODO: port dropper tool integration

    // UI from .ui file
    std::unique_ptr<Ui::ColorPickerPanel> _ui;

    Inkscape::Colors::Space::Type _spaceType = Inkscape::Colors::Space::Type::NONE;
    PlateType _plateType;
    std::shared_ptr<Inkscape::Colors::ColorSet> _colorSet;
    SPDesktop* _desktop = nullptr;

    ColorWheel* _plate = nullptr;       // interface (non-owning alias into _plateWidget)
    QWidget*    _plateWidget = nullptr; // the actual widget owned by the layout
    ColorPage*  _page = nullptr;
    // custom widgets created in code (not uic-constructible), owned by colorEntryFrame
    ColorPreview* _swatch = nullptr;
    ColorEntry* _hexEdit = nullptr;
    std::unique_ptr<QMenu> _spacesMenu;

    sigc::scoped_connection _colorChanged;
    sigc::signal<void(Inkscape::Colors::Space::Type)> _colorSpaceChanged;
};

// get a plate type from preferences
ColorPickerPanel::PlateType get_plate_type_preference(const char* pref_path_base, ColorPickerPanel::PlateType def_type = ColorPickerPanel::Rect);

// persist a plate type in preferences
void set_plate_type_preference(const char* pref_path_base, ColorPickerPanel::PlateType type);

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PICKER_PANEL_H
