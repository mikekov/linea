// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Color Page widget
 */

#ifndef LINEA_UI_COLOR_PAGE_H
#define LINEA_UI_COLOR_PAGE_H

#include <QGridLayout>
#include <QMetaObject>
#include <QShowEvent>
#include <QHideEvent>
#include <QWidget>
#include <memory>
#include <vector>

#include "color-holder.h"
#include "color-plate.h"
#include "colors/color.h"
#include "colors/spaces/base.h"
#include "colors/spaces/components.h"
#include "colors/spaces/enum.h"
#include "ui/operation-blocker.h"

namespace Linea::UI {

class ColorSlider;
class NumberEdit;
class ColorPageChannel;

class ColorPage : public QWidget {
    Q_OBJECT

public:
    enum class ChannelLayout {
        Sliders,  // one row per channel: slider | number edit
        Segments, // single row of side-by-side number edits, no sliders
    };

    ColorPage(std::shared_ptr<Inkscape::Colors::Space::AnySpace> space,
              std::shared_ptr<ColorHolder> selected_color,
              QWidget* parent = nullptr,
              ChannelLayout channels = ChannelLayout::Sliders);
    ~ColorPage() override;

    // Create a color plate widget (returns nullptr if the space has no plate)
    ColorPlate* createColorPlate(Inkscape::Colors::Space::Type type, ColorPlateType plateType,
                                 const PlateGamut& gamut);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    std::shared_ptr<Inkscape::Colors::Space::AnySpace> _space;
    std::shared_ptr<ColorHolder> _selected_color;
    std::shared_ptr<ColorHolder> _space_color;

    QGridLayout* _grid;
    std::vector<std::unique_ptr<ColorPageChannel>> _channels;

    sigc::scoped_connection _space_changed_connection;
    sigc::scoped_connection _selected_changed_connection;
    sigc::scoped_connection _selected_cleared_connection;
    sigc::scoped_connection _color_plate_changed;
    sigc::scoped_connection _color_plate_updated;
    ColorPlate* _color_plate = nullptr;
    ColorSlider* _hue_slider = nullptr;
};

class ColorPageChannel {
public:
    // Slider layout: binds edit <-> slider <-> holder.
    ColorPageChannel(std::shared_ptr<ColorHolder> color,
                     ColorSlider& slider,
                     NumberEdit& edit);
    // Segment layout: binds edit <-> holder directly (no slider).
    ColorPageChannel(std::shared_ptr<ColorHolder> color,
                     const Inkscape::Colors::Space::Component& component,
                     NumberEdit& edit);
    ColorPageChannel(const ColorPageChannel&) = delete;

    NumberEdit& get_edit() { return _edit; }

private:
    ColorPageChannel(std::shared_ptr<ColorHolder> color,
                     const Inkscape::Colors::Space::Component& component,
                     ColorSlider* slider,
                     NumberEdit& edit);

    ColorSlider* _slider = nullptr;
    NumberEdit& _edit;
    Inkscape::Colors::Space::Component _component;
    std::shared_ptr<ColorHolder> _color;
    sigc::scoped_connection _color_changed;
    QMetaObject::Connection _slider_changed;
    QMetaObject::Connection _edit_changed;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PAGE_H
