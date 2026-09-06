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

#include "colors/color-set.h"
#include "colors/color.h"
#include "colors/spaces/base.h"
#include "colors/spaces/enum.h"
#include "ui/operation-blocker.h"

namespace Linea::UI {

class ColorSlider;
class NumberEdit;
class ColorPageChannel;
class ColorWheel;

class ColorPage : public QWidget {
    Q_OBJECT

public:
    ColorPage(std::shared_ptr<Inkscape::Colors::Space::AnySpace> space,
              std::shared_ptr<Inkscape::Colors::ColorSet> colors,
              QWidget* parent = nullptr);
    ~ColorPage() override;

    // Create a color wheel/plate widget (returns nullptr if not yet ported)
    ColorWheel* createColorWheel(Inkscape::Colors::Space::Type type, bool disc);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    std::shared_ptr<Inkscape::Colors::Space::AnySpace> _space;
    std::shared_ptr<Inkscape::Colors::ColorSet> _selected_colors;
    std::shared_ptr<Inkscape::Colors::ColorSet> _specific_colors;

    QGridLayout* _grid;
    std::vector<std::unique_ptr<ColorPageChannel>> _channels;

    sigc::scoped_connection _specific_changed_connection;
    sigc::scoped_connection _selected_changed_connection;
    sigc::scoped_connection _color_wheel_changed;
    ColorWheel* _color_wheel = nullptr;
};

class ColorPageChannel {
public:
    ColorPageChannel(std::shared_ptr<Inkscape::Colors::ColorSet> color,
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
    std::shared_ptr<Inkscape::Colors::ColorSet> _color;
    sigc::scoped_connection _color_changed;
    QMetaObject::Connection _slider_changed;
    QMetaObject::Connection _edit_changed;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PAGE_H
