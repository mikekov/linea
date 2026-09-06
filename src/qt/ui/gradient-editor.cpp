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

#include "gradient-editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QAction>
#include <QIcon>
#include <QSignalBlocker>
#include <QWidgetAction>

#include "ui_gradient-editor.h"
#include "gradient-with-stops.h"
#include "color-picker-panel.h"
#include "number-edit.h"
#include "gradient-selector.h"
#include "qt/ui/gradient-selector.h"
#include "document-undo.h"
#include "gradient-chemistry.h"
#include "object/sp-linear-gradient.h"
#include "object/sp-stop.h"
#include "ui/icon-names.h"
#include "ui/util.h"
#include "util-string/context-string.h"
#include <cmath>
#include <2geom/affine.h>

namespace Linea::UI {

using namespace Inkscape::IO;

const std::array<std::tuple<SPGradientSpread, const char*, const char*>, 3>& spGetSpreadRepeats() {
    static auto const repeats = std::to_array({std::tuple
        {SP_GRADIENT_SPREAD_PAD,     QT_TR_NOOP("None"),      "gradient-spread-pad"},
        {SP_GRADIENT_SPREAD_REPEAT,  QT_TR_NOOP("Direct"),    "gradient-spread-repeat"},
        {SP_GRADIENT_SPREAD_REFLECT, QT_TR_NOOP("Reflected"), "gradient-spread-reflect"},
    });
    return repeats;
}

GradientEditor::GradientEditor(
    const char* prefs,
    Inkscape::Colors::Space::Type space,
    bool showTypeSelector,
    bool showColorwheelExpander,
    QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::GradientEditor>())
    , _prefs(prefs)
    , _colors(std::make_shared<Inkscape::Colors::ColorSet>())
{
    _ui->setupUi(this);
    setupCustomWidgets(space, showTypeSelector, showColorwheelExpander);
    connectSignals();
}

GradientEditor::~GradientEditor() = default;

void GradientEditor::setupCustomWidgets(Inkscape::Colors::Space::Type space, bool showTypeSelector, bool showColorwheelExpander) {
    // Create gradient image widget and add to placeholder
    _gradientImage = std::make_unique<GradientWithStops>();
    if (_ui->gradientWidgetPlaceholder) {
        auto layout = new QVBoxLayout(_ui->gradientWidgetPlaceholder);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(_gradientImage.get());
    }

    // Create repeat mode menu using resource icons
    _repeatMenu = std::make_unique<QMenu>(this);
    for (auto [mode, text, icon] : spGetSpreadRepeats()) {
        auto action = _repeatMenu->addAction(QIcon(QString(":/icons/%1").arg(icon)), tr(text));
        connect(action, &QAction::triggered, [this, mode]() { setRepeatMode(mode); });
    }
    _ui->repeatModeButton->setMenu(_repeatMenu.get());

    // Create library menu and selector
    _libraryMenu = std::make_unique<QMenu>(this);
    _ui->libraryButton->setMenu(_libraryMenu.get());
    _selector = std::make_unique<GradientSelector>(this);

    // Add GradientSelector to library menu using QWidgetAction
    auto selectorAction = new QWidgetAction(_libraryMenu.get());
    selectorAction->setDefaultWidget(_selector.get());
    _libraryMenu->addAction(selectorAction);

    // Hide type buttons if not needed
    if (!showTypeSelector) {
        _ui->linearButton->setVisible(false);
        _ui->radialButton->setVisible(false);
    }

    // Create color picker and add to placeholder
    _colorPicker = ColorPickerPanel::create(
        space,
    get_plate_type_preference(_prefs.c_str(), ColorPickerPanel::None),
        _colors,
        this
    ).release();

    if (_ui->colorPickerPlaceholder) {
        auto* layout = new QVBoxLayout(_ui->colorPickerPlaceholder);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(_colorPicker);
    }

    // Set up initial repeat icon
    setRepeatIcon(SP_GRADIENT_SPREAD_PAD);
}

void GradientEditor::connectSignals() {
    // Type buttons
    connect(_ui->linearButton, &QPushButton::toggled, this, &GradientEditor::onLinearToggled);
    connect(_ui->radialButton, &QPushButton::toggled, this, &GradientEditor::onRadialToggled);

    // Reverse and turn
    connect(_ui->reverseButton, &QPushButton::clicked, this, &GradientEditor::onReverseClicked);
    connect(_ui->turnButton, &QPushButton::clicked, this, &GradientEditor::onTurnClicked);

    // Spin boxes
    connect(_ui->offsetSpin, &NumberEdit::valueChanged, this, &GradientEditor::onOffsetChanged);
    connect(_ui->angleSpin, &NumberEdit::valueChanged, this, &GradientEditor::onAngleChanged);

    // Gradient image widget signals
    connect(_gradientImage.get(), &GradientWithStops::stopSelected, this, &GradientEditor::onStopSelected);
    connect(_gradientImage.get(), &GradientWithStops::stopOffsetChanged, this, &GradientEditor::onStopOffsetChanged);
    connect(_gradientImage.get(), &GradientWithStops::addStopAt, this, &GradientEditor::onAddStopAt);
    connect(_gradientImage.get(), &GradientWithStops::deleteStop, this, &GradientEditor::onDeleteStop);

    // Color picker
    _colorChanged = _colors->signal_changed.connect([this]() {
        setStopColor(_colors->getAverage());
    });
}

void GradientEditor::onLinearToggled(bool checked) {
    if (checked) {
        _ui->radialButton->setChecked(false);
        fireChangeType(true);
    }
}

void GradientEditor::onRadialToggled(bool checked) {
    if (checked) {
        _ui->linearButton->setChecked(false);
        fireChangeType(false);
    }
}

void GradientEditor::onReverseClicked() {
    reverseGradient();
}

void GradientEditor::onTurnClicked() {
    turnGradient(90, true);
}

void GradientEditor::onOffsetChanged(double value) {
    if (_update.pending()) return;

    if (auto index = currentStopIndex()) {
        setStopOffset(index.value(), value / 100.0);
    }
}

void GradientEditor::onAngleChanged(double value) {
    if (_update.pending()) return;

    turnGradient(value, false);
}

void GradientEditor::onStopSelected(size_t index) {
    selectStop(static_cast<int>(index));
    fireStopSelected(currentStop());
}

void GradientEditor::onStopOffsetChanged(size_t index, double offset) {
    setStopOffset(index, offset);
}

void GradientEditor::onAddStopAt(double offset) {
    insertStopAt(offset);
}

void GradientEditor::onDeleteStop(size_t index) {
    deleteStopInternal(static_cast<int>(index));
}

void GradientEditor::setGradient(SPGradient* gradient) {
    auto scoped = _update.block();

    _gradient = gradient;
    _document = gradient ? gradient->document : nullptr;

    setGradientInternal(gradient);
}

void GradientEditor::setGradientInternal(SPGradient* gradient) {
    SPGradient* vector = gradient ? gradient->getVector() : nullptr;

    if (vector) {
        vector->ensureVector();
    }

    _gradientImage->setGradient(vector);

    if (!vector || !vector->hasStops()) return;

    auto mode = gradient->isSpreadSet() ? gradient->getSpread() : SP_GRADIENT_SPREAD_PAD;
    setRepeatIcon(mode);

    auto canRotate = false;
    // only linear gradient can be rotated currently
    if (auto* linear = dynamic_cast<SPLinearGradient*>(gradient)) {
        canRotate = true;
        auto line = Geom::Line(
            Geom::Point(linear->x1.computed, linear->y1.computed),
            Geom::Point(linear->x2.computed, linear->y2.computed)
        );
        auto angle = std::atan2(line.finalPoint().y() - line.initialPoint().y(),
                                line.finalPoint().x() - line.initialPoint().x()) * 180 / M_PI;
        {
            const bool wasBlocked = _ui->angleSpin->blockSignals(true);
            _ui->angleSpin->setValue(angle);
            _ui->angleSpin->blockSignals(wasBlocked);
        }
        _ui->linearButton->setChecked(true);
        _ui->radialButton->setChecked(false);
    } else {
        _ui->radialButton->setChecked(true);
        _ui->linearButton->setChecked(false);
    }

    _ui->turnButton->setEnabled(canRotate);
    _ui->angleSpin->setEnabled(canRotate);

    selectStop(_currentStopIndex);
}

SPGradient* GradientEditor::getVector() {
    return _gradient;
}

void GradientEditor::setVector(SPDocument* doc, SPGradient* vector) {
    auto scoped = _update.block();
    _selector->setVector(doc, vector);
}

void GradientEditor::setMode(SelectorMode mode) {
    _selector->setMode(static_cast<GradientSelector::SelectorMode>(mode));
}

void GradientEditor::setUnits(SPGradientUnits units) {
    _selector->setUnits(units);
}

SPGradientUnits GradientEditor::getUnits() {
    return _selector->getUnits();
}

void GradientEditor::setSpread(SPGradientSpread spread) {
    _selector->setSpread(spread);
}

SPGradientSpread GradientEditor::getSpread() {
    return _selector->getSpread();
}

void GradientEditor::selectStop(SPStop* selected) {
    if (_notification.pending()) return;

    auto scoped = _notification.block();
    selectStop(getStopIndex(selected).value_or(-1));
}

void GradientEditor::setStopOffset(size_t index, double offset) {
    if (_update.pending()) return;

    if (SPGradient* vector = getGradientVector()) {
        if (SPStop* stop = sp_get_nth_stop(vector, index)) {
            auto scoped = _update.block();

            stop->offset = offset;
            if (auto repr = stop->getRepr()) {
                repr->setAttributeCssDouble("offset", stop->offset);
            }

            DocumentUndo::done(_document, RC_("Undo", "Change gradient stop offset"), INKSCAPE_ICON("color-gradient"));
        }
    }
}

void GradientEditor::setColorPickerPlate(ColorPickerPanel::PlateType type) {
    _colorPicker->setPlateType(type);
    set_plate_type_preference(_prefs.c_str(), type);
}

ColorPickerPanel::PlateType GradientEditor::getColorPickerPlate() const {
    return _colorPicker->getPlateType();
}

SPGradientType GradientEditor::getType() const {
    return _ui->linearButton->isChecked() ? SP_GRADIENT_TYPE_LINEAR : SP_GRADIENT_TYPE_RADIAL;
}

QWidget& GradientEditor::getColorBox() {
    return *_colorPicker;
}

void GradientEditor::setStopColor(const Inkscape::Colors::Color& color) {
    if (_update.pending()) return;

    SPGradient* vector = getGradientVector();
    if (!vector) return;

    if (auto* stop = currentStop()) {
        if (_document) {
            auto scoped = _update.block();
            sp_set_gradient_stop_color(_document, stop, color);
        }
    }
}

SPStop* GradientEditor::currentStop() {
    SPGradient* vector = _gradient ? _gradient->getVector() : nullptr;

    if (!vector || !vector->hasStops()) return nullptr;

    vector->ensureVector();
    int index = 0;
    for (auto& child : vector->children) {
        if (auto* stop = dynamic_cast<SPStop*>(&child)) {
            if (index == _currentStopIndex) return stop;
            ++index;
        }
    }

    return nullptr;
}

std::optional<int> GradientEditor::currentStopIndex() {
    if (currentStop()) {
        return _currentStopIndex;
    }
    return std::nullopt;
}

std::optional<int> GradientEditor::getStopIndex(SPStop* stop) {
    SPGradient* vector = _gradient ? _gradient->getVector() : nullptr;
    if (!vector || !stop) return std::nullopt;

    return sp_number_of_stops_before_stop(vector, stop);
}

SPStop* GradientEditor::getNthStop(size_t index) {
    if (SPGradient* vector = getGradientVector()) {
        return sp_get_nth_stop(vector, index);
    }
    return nullptr;
}

void GradientEditor::stopSelectedInternal() {
    auto scoped = _update.block();
    _colors->clear();

    if (auto* stop = currentStop()) {
        _colors->set(stop->getId(), stop->getColor());

        auto [before, after] = sp_get_before_after_stops(stop);
        _ui->offsetSpin->setRange(before ? before->offset * 100 : 0, after ? after->offset * 100 : 100);
        _ui->offsetSpin->setEnabled(true);
        {
            const bool wasBlocked = _ui->offsetSpin->blockSignals(true);
            _ui->offsetSpin->setValue(stop->offset * 100);
            _ui->offsetSpin->blockSignals(wasBlocked);
        }

        _gradientImage->setFocusedStop(currentStopIndex().value_or(-1));
    } else {
        // no selection
        _ui->offsetSpin->setRange(0, 0);
        _ui->offsetSpin->setValue(0);
        _ui->offsetSpin->setEnabled(false);
    }
}

void GradientEditor::insertStopAt(double offset) {
    if (SPGradient* vector = getGradientVector()) {
        // only insert a new stop if there are some stops present
        if (vector->hasStops()) {
            SPStop* stop = sp_gradient_add_stop_at(vector, offset);
            // select the next stop
            auto pos = sp_number_of_stops_before_stop(vector, stop);
            auto selected = selectStop(pos);
            fireStopSelected(stop);
            if (!selected) {
                selectStop(pos);
            }
        }
    }
}

void GradientEditor::addStop(int index) {
    if (SPGradient* vector = getGradientVector()) {
        if (SPStop* current = sp_get_nth_stop(vector, index)) {
            SPStop* stop = sp_gradient_add_stop(vector, current);
            // select the next stop
            selectStop(sp_number_of_stops_before_stop(vector, stop));
            fireStopSelected(stop);
        }
    }
}

void GradientEditor::deleteStopInternal(int index) {
    if (SPGradient* vector = getGradientVector()) {
        if (SPStop* stop = sp_get_nth_stop(vector, index)) {
            // try deleting a stop if it can be
            sp_gradient_delete_stop(vector, stop);
        }
    }
}

double lineAngle(const Geom::Line& line) {
    auto d = line.finalPoint() - line.initialPoint();
    return std::atan2(d.y(), d.x());
}

void GradientEditor::turnGradient(double angle, bool relative) {
    if (_update.pending() || !_document || !_gradient) return;

    if (auto linear = dynamic_cast<SPLinearGradient*>(_gradient)) {
        auto scoped = _update.block();

        auto line = Geom::Line(
            Geom::Point(linear->x1.computed, linear->y1.computed),
            Geom::Point(linear->x2.computed, linear->y2.computed)
        );
        auto center = line.pointAt(0.5);
        auto radians = angle / 180 * M_PI;
        if (!relative) {
            radians -= lineAngle(line);
        }
        auto rotate = Geom::Affine(Geom::Translate(-center)) * Geom::Affine(Geom::Rotate(radians)) * Geom::Affine(Geom::Translate(center));
        auto rotated = line.transformed(rotate);

        linear->x1 = rotated.initialPoint().x();
        linear->y1 = rotated.initialPoint().y();
        linear->x2 = rotated.finalPoint().x();
        linear->y2 = rotated.finalPoint().y();

        _gradient->updateRepr();

        DocumentUndo::done(_document, RC_("Undo", "Rotate gradient"), INKSCAPE_ICON("color-gradient"));
    }
}

void GradientEditor::reverseGradient() {
    if (_document && _gradient) {
        // reverse works on a gradient definition, the one with stops:
        if (SPGradient* vector = getGradientVector()) {
            sp_gradient_reverse_vector(vector);
            DocumentUndo::done(_document, RC_("Undo", "Reverse gradient"), INKSCAPE_ICON("color-gradient"));
        }
    }
}

void GradientEditor::setRepeatMode(SPGradientSpread mode) {
    if (_update.pending()) return;

    if (_document && _gradient) {
        auto scoped = _update.block();

        // spread is set on a gradient reference, which is _gradient object
        _gradient->setSpread(mode);
        _gradient->updateRepr();

        DocumentUndo::done(_document, RC_("Undo", "Set gradient repeat"), INKSCAPE_ICON("color-gradient"));

        setRepeatIcon(mode);
    }
}

void GradientEditor::setRepeatIcon(SPGradientSpread mode) {
    for (auto [repeatMode, label, iconName] : spGetSpreadRepeats()) {
        if (mode == repeatMode) {
            _ui->repeatModeButton->setIcon(QIcon(QString(":/icons/%1").arg(iconName)));
            break;
        }
    }
}

SPGradient* GradientEditor::getGradientVector() {
    if (!_gradient) return nullptr;
    return sp_gradient_get_forked_vector_if_necessary(_gradient, false);
}

bool GradientEditor::selectStop(int index) {
    if (getNthStop(index)) {
        _currentStopIndex = index;
        // update related widgets
        stopSelectedInternal();
        return true;
    }
    return false;
}

void GradientEditor::fireStopSelected(SPStop* stop) {
    if (!_notification.pending()) {
        auto scoped = _notification.block();
        emit_stop_selected(stop);
    }
}

void GradientEditor::fireChangeType(bool linear) {
    if (_notification.pending()) return;

    auto scoped = _notification.block();
    Q_EMIT signalChanged(_gradient);
}

void GradientEditor::onRepeatModeTriggered() {
    // Handled by menu actions
}

} // namespace Linea::UI
