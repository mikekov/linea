// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Color Page widget implementation
 */

#include "color-page.h"

#include "color-slider.h"
#include "color-wheel.h"
#include "color-wheel-factory.h"
#include "number-edit.h"
#include "util/signal-blocker.h"

using namespace Inkscape::Colors;

namespace Linea::UI {

ColorPage::ColorPage(std::shared_ptr<Inkscape::Colors::Space::AnySpace> space,
                     std::shared_ptr<Inkscape::Colors::ColorSet> colors, QWidget* parent)
    : QWidget(parent)
    , _space(std::move(space))
    , _selected_colors(colors)
    , _specific_colors(
          std::make_shared<Inkscape::Colors::ColorSet>(_space, colors->getAlphaConstraint().value_or(true))) {
    _grid = new QGridLayout();
    _grid->setHorizontalSpacing(2);
    _grid->setVerticalSpacing(1);
    _grid->setContentsMargins(0, 0, 0, 0);
    _grid->setColumnStretch(1, 1); // Make column 1 (slider) resizable
    setLayout(_grid);

    // Keep the selected colorset in-sync with the space specific colorset.
    _specific_changed_connection = _specific_colors->signal_changed.connect([this]() {
        auto scoped = SignalBlocker{_specific_changed_connection};
        for (auto& [id, color] : *_specific_colors) {
            _selected_colors->set(id, color);
        }
    });

    // Keep the child in-sync with the selected colorset.
    _selected_changed_connection = _selected_colors->signal_changed.connect([this]() {
        auto scoped = SignalBlocker{_selected_changed_connection};
        _specific_colors->clear();
        for (auto& [id, color] : *_selected_colors) {
            _specific_colors->set(id, color);
        }
    });

    // Control signals when widget isn't visible
    // (handled in showEvent/hideEvent instead)

    int row = 0;
    for (auto& component : _specific_colors->getComponents()) {
        auto label = new QLabel(this);
        auto slider = new ColorSlider(_specific_colors, component, this);
        auto edit = new NumberEdit(this);

        // Set label properties
        label->setText(QString::fromStdString(component.name));
        label->setToolTip(QString::fromStdString(component.tip));
        label->setAlignment(Qt::AlignCenter);
        label->setBuddy(edit);
        label->setProperty("class", "panel-label");

        // Set slider properties
        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        // Set edit properties
        edit->setMinimum(0.0);
        edit->setMaximum(component.scale);
        edit->setSingleStep(1.0);

        // Set precision based on component
        edit->setDecimals(0);
        // if (component.id == "alpha") {
        //     edit->setDecimals(0);
        // } else if (component.unit == Space::Unit::Degree || component.unit == Space::Unit::Percent) {
        //     edit->setDecimals(0);
        if (component.scale < 100) {
            // for small values increase precision
            edit->setDecimals(2);
            edit->setSingleStep(0.1);
        }

        // Set suffix based on unit
        if (component.unit == Inkscape::Colors::Space::Unit::Degree) {
            edit->setSuffix("°");
        } else if (component.unit == Inkscape::Colors::Space::Unit::Percent) {
            edit->setSuffix("%");
        }
        //  else if (component.unit == Inkscape::Colors::Space::Unit::Chroma40) {
            // (very) limited chroma range; increase precision
            // edit->setDecimals(2);
        // }

        _grid->addWidget(label, row, 0);
        _grid->addWidget(slider, row, 1);
        _grid->addWidget(edit, row, 2);

        _channels.emplace_back(std::make_unique<ColorPageChannel>(_specific_colors, *label, *slider, *edit));
        row++;
    }
}

ColorPage::~ColorPage() = default;

ColorWheel* ColorPage::createColorWheel(Inkscape::Colors::Space::Type type, bool disc) {
    if (!canCreateColorWheel(type)) return nullptr;

    auto wheel = Linea::UI::createColorWheel(type, disc, this);
    if (!wheel) return nullptr;
    // keep color wheel in sync with the color set
    _color_wheel = wheel;
    _color_wheel_changed = _specific_colors->signal_changed.connect([this]() {
        if (!_specific_colors->isEmpty()) {
            _color_wheel->setColor(_specific_colors->getAverage());
        }
    });
    _color_wheel_changed = wheel->connectColorChanged([this](const Inkscape::Colors::Color& color) {
        auto scoped = SignalBlocker{_color_wheel_changed};
        auto opacity = _specific_colors->isEmpty() ? 1.0 : _specific_colors->getAverage().getOpacity();
        auto updated = color;
        updated.setOpacity(opacity);
        _specific_colors->setAll(updated);
    });
    return wheel;
}

void ColorPage::showEvent(QShowEvent* event) {
    _specific_colors->setAll(*_selected_colors);
    _specific_changed_connection.unblock();
    _selected_changed_connection.unblock();
    QWidget::showEvent(event);
}

void ColorPage::hideEvent(QHideEvent* event) {
    _specific_colors->clear();
    _specific_changed_connection.block();
    _selected_changed_connection.block();
    QWidget::hideEvent(event);
}

ColorPageChannel::ColorPageChannel(std::shared_ptr<Inkscape::Colors::ColorSet> color, QLabel& label,
                                   ColorSlider& slider, NumberEdit& edit)
    : _label(label)
    , _slider(slider)
    , _edit(edit)
    , _color(std::move(color)) {
    auto& component = _slider.getComponent();

    _color_changed = _color->signal_changed.connect([this, &component]() {
        if (_color->isValid(component)) {
            auto scoped = _update.block();
            _edit.setValue(_slider.getScaled());
        }
    });

    _edit_changed = QObject::connect(&_edit, &NumberEdit::valueChanged, [this](double value) {
        if (!_update.pending()) {
            _slider.setScaled(value);
        }
    });

    _slider_changed = QObject::connect(&_slider, &ColorSlider::valueChanged, [this]() {
        if (!_update.pending()) {
            auto scoped = _update.block();
            _edit.setValue(_slider.getScaled());
        }
    });
}

} // namespace Linea::UI
