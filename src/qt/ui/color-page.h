// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Color Page widget
 */

#ifndef LINEA_UI_COLOR_PAGE_H
#define LINEA_UI_COLOR_PAGE_H

#include <QGridLayout>
#include <QLabel>
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
#include "colors/spaces/enum.h"
#include "ui/operation-blocker.h"

namespace Linea::UI {

class ColorSlider;
class NumberEdit;
class ColorPageChannel;

class ColorPage : public QWidget {
    Q_OBJECT

public:
    ColorPage(std::shared_ptr<Inkscape::Colors::Space::AnySpace> space,
              std::shared_ptr<ColorHolder> colors,
              QWidget* parent = nullptr);
    ~ColorPage() override;

    // Create a color plate widget (returns nullptr if the space has no plate)
    ColorPlate* createColorPlate(Inkscape::Colors::Space::Type type, ColorPlateType plateType,
                                 const PlateGamut& gamut);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    std::shared_ptr<Inkscape::Colors::Space::AnySpace> _space;
    std::shared_ptr<ColorHolder> _selected_colors;
    std::shared_ptr<ColorHolder> _specific_colors;

    QGridLayout* _grid;
    std::vector<std::unique_ptr<ColorPageChannel>> _channels;

    sigc::scoped_connection _specific_changed_connection;
    sigc::scoped_connection _selected_changed_connection;
    sigc::scoped_connection _selected_cleared_connection;
    sigc::scoped_connection _color_plate_changed;
    sigc::scoped_connection _color_plate_updated;
    ColorPlate* _color_plate = nullptr;
};

class ColorPageChannel {
public:
    ColorPageChannel(std::shared_ptr<ColorHolder> color,
                     QLabel& label,
                     ColorSlider& slider,
                     NumberEdit& edit);
    ColorPageChannel(const ColorPageChannel&) = delete;

    QLabel& get_label() { return _label; }
    NumberEdit& get_edit() { return _edit; }

private:
    QLabel& _label;
    ColorSlider& _slider;
    NumberEdit& _edit;
    std::shared_ptr<ColorHolder> _color;
    sigc::scoped_connection _color_changed;
    QMetaObject::Connection _slider_changed;
    QMetaObject::Connection _edit_changed;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PAGE_H
