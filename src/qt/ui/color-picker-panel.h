// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPickerPanel — hosts a ColorPage and an optional color wheel/plate,
 * plus a bottom bar with dropper, hex-RGB edit and color-space selector.
 */

#ifndef LINEA_UI_COLOR_PICKER_PANEL_H
#define LINEA_UI_COLOR_PICKER_PANEL_H

#include <QWidget>
#include <memory>
#include <string>
#include <sigc++/signal.h>

#include "color-holder.h"
#include "color-plate.h"
#include "colors/color.h"
#include "colors/spaces/enum.h"
#include "flat-color.h"

QT_BEGIN_NAMESPACE
class QMenu;
class QSpacerItem;
namespace Ui {
class ColorPickerPanel;
}
QT_END_NAMESPACE

class SPDesktop;
namespace Inkscape::UI::Tools { class DropperTool; }
namespace Linea { class RecentColors; }

namespace Linea::UI {

class ColorPage;

/**
 * A widget hosting a ColorPage with an optional color plate/wheel on top and
 * a bottom bar containing a dropper button, RGB hex entry and color-space selector.
 */
class ColorPickerPanel : public QWidget {
    Q_OBJECT
public:
    // color plate type — rectangular, color wheel, no plate (only sliders)
    // enum PlateType { Rect, Circle, None };

    static std::unique_ptr<ColorPickerPanel> create(Inkscape::Colors::Space::Type space, ColorPlateType type,
                                                    std::shared_ptr<ColorHolder> color, QWidget* parent = nullptr);

    ~ColorPickerPanel() override;

    void setDesktop(SPDesktop* desktop);
    void setColor(const Inkscape::Colors::Color& color);
    // request color type/space change
    void setPickerType(Inkscape::Colors::Space::Type type);
    // request type of color wheel/plate
    void setPlateType(ColorPlateType plate);
    ColorPlateType getPlateType() const;
    Inkscape::Colors::Space::Type spaceType() const { return _spaceType; }
    // enable/disable recent colors widget
    void showRecentColors(bool show);

    // emitted when a recent-colors tile is clicked; Color entries are also
    // pushed into the holder, a Swatch is the host's to apply as a var() reference
    sigc::signal<void(const FlatColor&)>& recentColorClicked();

    sigc::signal<void(Inkscape::Colors::Space::Type)>& colorSpaceChanged();

Q_SIGNALS:
    void colorSpaceChangedQt(int spaceType);

private:
    explicit ColorPickerPanel(Inkscape::Colors::Space::Type space, ColorPlateType type, std::shared_ptr<ColorHolder> color,
                              QWidget* parent);

    void buildBottomBar();
    void createColorPage(Inkscape::Colors::Space::Type type, ColorPlateType plateType);
    void removeWidgets();
    void switchPage(Inkscape::Colors::Space::Type space, ColorPlateType plateType);
    void updateColor();
    void bindRecentColors();
    void updateRecentColors();
    void setRecentVisible(bool show);
    void pickColor();
    void cancelPick();
    void endPick();

    // UI from .ui file
    std::unique_ptr<Ui::ColorPickerPanel> _ui;

    Inkscape::Colors::Space::Type _spaceType = Inkscape::Colors::Space::Type::NONE;
    ColorPlateType _plateType;
    std::shared_ptr<ColorHolder> _colorSet;
    SPDesktop* _desktop = nullptr;

    ColorPlate* _plate = nullptr;    // owned by the layout
    QSpacerItem* _topGap = nullptr;  // spacing above the plate, when present
    ColorPage* _page = nullptr;
    // custom widgets created in code (not uic-constructible), owned by colorEntryFrame
    std::unique_ptr<QMenu> _spacesMenu;
    QMenu* _swatchMenu = nullptr;

    sigc::scoped_connection _colorChanged;
    sigc::scoped_connection _colorPicking;
    sigc::scoped_connection _colorPickingCancel;
    // recent colors strip state; _recentColors is owned by the bound document
    Linea::RecentColors* _recentColors = nullptr;
    sigc::scoped_connection _recentChanged;
    sigc::scoped_connection _docReplaced;
    bool _showRecent = false;
    Inkscape::UI::Tools::DropperTool* _pickTool = nullptr; // valid only while _colorPicking is connected
    sigc::signal<void(const FlatColor&)> _recentColorClicked;
    sigc::signal<void(Inkscape::Colors::Space::Type)> _colorSpaceChanged;
};

// persisted "show color plate" toggle for a picker under 'pref_path_base';
// the plate's shape comes from the space's spec, not from preferences
bool get_show_plate_preference(const std::string& pref_path_base, bool def);

// persist the toggle
void set_show_plate_preference(const std::string& pref_path_base, bool show);

// gamut drawing preferences for plates in unbounded spaces
PlateGamut get_plate_gamut_preference();

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PICKER_PANEL_H
