// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Gradient editor widget
 *
 * Author:
 *   Michael Kowalski
 *
 * Copyright (C) 2020-2026 Michael Kowalski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_GRADIENT_EDITOR_H
#define LINEA_UI_GRADIENT_EDITOR_H

#include <QWidget>
#include <QMenu>
#include <memory>
#include <optional>
#include <sigc++/scoped_connection.h>

#include "ui/operation-blocker.h"

#include "colors/color-set.h"
#include "object/sp-gradient.h"
#include "qt/ui/color-picker-panel.h"
#include "ui/widget/gradient-selector-interface.h"

class SPDocument;
class SPStop;
class SPGradient;

namespace Inkscape::Colors {
class Color;
}

QT_BEGIN_NAMESPACE
namespace Ui {
class GradientEditor;
}
QT_END_NAMESPACE

namespace Linea::UI {

class ColorPickerPanel;
class GradientSelector;
class GradientWithStops;
class NumberEdit;

/**
 * Gradient editor widget that allows editing gradient stops,
 * changing gradient type (linear/radial), and adjusting gradient properties.
 */
class GradientEditor : public QWidget, public GradientSelectorInterface {
    Q_OBJECT

public:
    GradientEditor(
        const char* prefs,
        Inkscape::Colors::Space::Type space,
        bool showTypeSelector,
        bool showColorwheelExpander,
        QWidget* parent = nullptr);
    ~GradientEditor() override;

    // GradientSelectorInterface implementation
    void setGradient(SPGradient* gradient) override;
    SPGradient* getVector() override;
    void setVector(SPDocument* doc, SPGradient* vector) override;
    void setMode(SelectorMode mode) override;
    void setUnits(SPGradientUnits units) override;
    SPGradientUnits getUnits() override;
    void setSpread(SPGradientSpread spread) override;
    SPGradientSpread getSpread() override;
    void selectStop(SPStop* selected) override;

    // Additional public methods
    void setColorPickerPlate(ColorPickerPanel::PlateType type);
    ColorPickerPanel::PlateType getColorPickerPlate() const;
    SPGradientType getType() const;
    ColorPickerPanel& getPicker() { return *_colorPicker; }
    QWidget& getColorBox();

Q_SIGNALS:
    void signalChanged(SPGradient* gradient);
    void signalGrabbed();
    void signalDragged();
    void signalReleased();

private Q_SLOTS:
    void onOffsetChanged(double value);
    void onAngleChanged(double value);
    void onReverseClicked();
    void onTurnClicked();
    void onLinearToggled(bool checked);
    void onRadialToggled(bool checked);
    void onStopSelected(size_t index);
    void onStopOffsetChanged(size_t index, double offset);
    void onAddStopAt(double offset);
    void onDeleteStop(size_t index);
    void onRepeatModeTriggered();

private:
    void setupCustomWidgets(Inkscape::Colors::Space::Type space, bool showTypeSelector, bool showColorwheelExpander);
    void connectSignals();
    void setGradientInternal(SPGradient* gradient);
    void stopSelectedInternal();
    void insertStopAt(double offset);
    void addStop(int index);
    void deleteStopInternal(int index);
    void setRepeatMode(SPGradientSpread mode);
    void setRepeatIcon(SPGradientSpread mode);
    void reverseGradient();
    void turnGradient(double angle, bool relative);
    void setStopColor(const Inkscape::Colors::Color& color);
    SPStop* currentStop();
    std::optional<int> currentStopIndex();
    std::optional<int> getStopIndex(SPStop* stop);
    SPStop* getNthStop(size_t index);
    bool selectStop(int index);
    void setStopOffset(size_t index, double offset);
    SPGradient* getGradientVector();
    void fireStopSelected(SPStop* stop);
    void fireChangeType(bool linear);

    // UI from .ui file
    std::unique_ptr<Ui::GradientEditor> _ui;

    // Custom widgets (not in UI file)
    std::unique_ptr<GradientWithStops> _gradientImage;
    std::unique_ptr<QMenu> _repeatMenu;
    std::unique_ptr<QMenu> _libraryMenu;
    std::unique_ptr<GradientSelector> _selector;
    ColorPickerPanel* _colorPicker = nullptr;
    std::shared_ptr<Inkscape::Colors::ColorSet> _colors;

    // State
    int _currentStopIndex = 0;
    SPGradient* _gradient = nullptr;
    SPDocument* _document = nullptr;
    std::string _prefs;
    OperationBlocker _update;
    OperationBlocker _notification;

    // Connections
    sigc::scoped_connection _colorChanged;
};

// SPGradientSpread modes, names and icons
const std::array<std::tuple<SPGradientSpread, const char*, const char*>, 3>& spGetSpreadRepeats();

} // namespace Linea::UI

#endif // LINEA_UI_GRADIENT_EDITOR_H
