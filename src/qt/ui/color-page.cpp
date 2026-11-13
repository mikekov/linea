// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Color Page widget implementation
 */

#include "color-page.h"

#include <QPushButton>
#include <QVBoxLayout>

#include "color-palette-panel.h"
#include "color-slider.h"
#include "color-plate.h"
#include "colors/manager.h"
#include "number-edit.h"
#include "util/signal-blocker.h"

using namespace Inkscape::Colors;

namespace Linea::UI {

ColorPage::ColorPage(std::shared_ptr<Inkscape::Colors::Space::AnySpace> space,
                     std::shared_ptr<ColorHolder> selected_color, QWidget* parent,
                     ChannelLayout channels, bool all_sliders)
    : QWidget(parent)
    , _space(std::move(space))
    , _selected_color(selected_color)
    , _space_color(std::make_shared<ColorHolder>(_space, selected_color->hasAlpha())) {
    _grid = new QGridLayout();
    _grid->setHorizontalSpacing(2);
    _grid->setVerticalSpacing(1);
    _grid->setContentsMargins(0, 0, 0, 0);
    _grid->setColumnStretch(0, 1); // Make column 0 (slider) resizable
    setLayout(_grid);

    // Keep the selected color in-sync with the space-specific color.
    // set() stores the incoming space as written — the picker's space is the
    // color's identity in the model; serialization decides if it survives.
    _space_changed_connection = _space_color->signal_changed.connect([this]() {
        auto scoped = SignalBlocker{_space_changed_connection};
        if (auto color = _space_color->get()) {
            _selected_color->set(*color);
        }
    });

    // Keep the child in-sync with the selected color.
    _selected_changed_connection = _selected_color->signal_changed.connect([this]() {
        auto scoped = SignalBlocker{_selected_changed_connection};
        if (auto color = _selected_color->get()) {
            _space_color->set(*color);
        } else {
            _space_color->clear();
        }
    });

    _selected_cleared_connection = _selected_color->signal_cleared.connect([this]() {
        auto scoped = SignalBlocker(_space_changed_connection);
        _space_color->clear();
    });

    // Control signals when widget isn't visible
    // (handled in showEvent/hideEvent instead)

    auto const configure_edit = [](NumberEdit* edit, const Space::Component& component) {
        edit->setMinimum(0.0);
        edit->setMaximum(component.scale);
        edit->setSingleStep(1.0);

        // Set precision based on component
        edit->setDecimals(0);
        if (component.scale <= 1) {
            // for small values increase precision
            edit->setDecimals(3);
            edit->setSingleStep(0.005);
            edit->setKeptFractionDigits(3);
        }

        // Set suffix based on unit (removed, there's no space)
        if (component.unit == Inkscape::Colors::Space::Unit::Degree) {
            // edit->setSuffix("°");
        } else if (component.unit == Inkscape::Colors::Space::Unit::Percent) {
            // edit->setSuffix("%");
        }

        edit->setToolTip(QString::fromStdString(component.tip));
    };

    if (channels == ChannelLayout::Segments) {
        const auto& components = _space_color->getComponents();
        int row = 0;

        // Optional sliders above the channel edits. By default just the hue
        // and alpha channels; with all_sliders every channel gets one.
        // They bind to the same holder, so segments and sliders stay in sync.
        auto sliders = _sliders = new QVBoxLayout();
        sliders->setContentsMargins(0, 0, 0, 0);
        sliders->setSpacing(6);
        auto add_slider = [this, sliders](const Space::Component& component) {
            auto slider = new ColorSlider(_space_color, component, this);
            slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            sliders->addWidget(slider);
            return slider;
        };
        if (all_sliders) {
            const auto hue = components.hue_index();
            for (unsigned i = 0; i < components.size(); ++i) {
                auto* slider = add_slider(components[i]);
                if (hue && i == *hue) _hue_slider = slider;
            }
        } else {
            if (auto hue = components.hue_index()) {
                _hue_slider = add_slider(components[*hue]);
            }
            for (auto& component : components) {
                if (component.id == "alpha") {
                    add_slider(component);
                    break;
                }
            }
        }
        // always keep the group at row 0, even when empty — createColorPlate
        // may add a proxy hue slider to it once the plate space is known
        _grid->addLayout(sliders, row++, 0, 1, 3);

        // All channel edits in one row, drawn as a single segmented control;
        // each edit carries its channel name in the built-in indicator label.
        auto segments = new QGridLayout();
        segments->setContentsMargins(0, 8, 0, 0);
        segments->setSpacing(0);
        _segments = segments;

        // Name of the color space, styled as the leading segment of the row.
        // A fixed first column keeps the label intact under squeeze — the
        // channel fields in the remaining columns absorb the shrinkage.
        _space_button = new QPushButton(QString::fromStdString(_space->getShortName()), this);
        _space_button->setProperty("class", "NumberEdit");
        _space_button->setProperty("segment", "first");
        _space_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        _space_button->ensurePolished();
        segments->addWidget(_space_button, 0, 0);
        segments->setColumnMinimumWidth(0, _space_button->sizeHint().width());

        const int count = static_cast<int>(components.size());
        int col = 0;
        for (auto& component : components) {
            auto edit = new NumberEdit(this);
            // edit->setLabel(QString::fromStdString(component.name));
            edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            configure_edit(edit, component);
            edit->setProperty("segment", col == count - 1 ? "last" : "middle");
            segments->addWidget(edit, 0, col + 1);

            _channels.emplace_back(std::make_unique<ColorPageChannel>(_space_color, component, *edit));
            col++;
        }

        _grid->addLayout(segments, row, 0, 1, 3);
        return;
    }

    int row = 0;
    for (auto& component : _space_color->getComponents()) {
        auto slider = new ColorSlider(_space_color, component, this);
        auto edit = new NumberEdit(this);

        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // the channel name lives inside the edit, like in segments mode
        edit->setLabel(QString::fromStdString(component.name));
        configure_edit(edit, component);

        _grid->addWidget(slider, row, 0);
        _grid->addWidget(edit, row, 1);

        _channels.emplace_back(std::make_unique<ColorPageChannel>(_space_color, *slider, *edit));
        row++;
    }
}

ColorPage::~ColorPage() = default;

void ColorPage::setSpaceMenu(QMenu* menu) {
    if (!_space_button) return;

    _space_button->setMenu(menu);
    // the dropdown indicator widens the button's hint — re-measure the
    // leading column so the label still fits
    if (_segments) {
        _segments->setColumnMinimumWidth(0, _space_button->sizeHint().width());
    }
}

ColorPlate* ColorPage::createColorPlate(Inkscape::Colors::Space::Type type, ColorPlateType plateType,
                                        const PlateGamut& gamut) {
    if (!ColorPlate::supports(type)) return nullptr;

    auto plate = ColorPlate::create(type, plateType, gamut, this);
    if (!plate) return nullptr;
    // wheel and ring plates already edit hue — drop the redundant slider
    if (_hue_slider &&
        (plateType == ColorPlateType::Wheel || plateType == ColorPlateType::Ring)) {
        _grid->removeWidget(_hue_slider);
        delete _hue_slider;
        _hue_slider = nullptr;
    }
    // they instead hold one channel constant — the lightness in OKHSL. When
    // the page's space is the plate's, give the page a slider for that
    // channel so it drives which lightness the plate displays
    if ((plateType == ColorPlateType::Wheel || plateType == ColorPlateType::Ring) &&
        _sliders && plate->plateSpace() == type) {
        auto slider = new ColorSlider(
            _space_color, _space_color->getComponents()[plate->fixedChannel()], this);
        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        _sliders->insertWidget(0, slider);
    }
    // keep color plate in sync with the color set
    _color_plate = plate;
    _color_plate_changed = _space_color->signal_changed.connect([this]() {
        if (auto color = _space_color->get()) {
            _color_plate->setColor(*color);
        }
    });
    _color_plate_updated = plate->connectColorChanged([this](const Inkscape::Colors::Color& color) {
        auto scoped = SignalBlocker{_color_plate_changed};
        auto opacity = _space_color->get() ? _space_color->get()->getOpacity() : 1.0;
        auto updated = color;
        updated.setOpacity(opacity);
        _space_color->set(updated);
    });

    // A rect plate holds its fixed channel constant. When that channel is
    // the plate space's hue and the page has no hue channel of its own,
    // give the page a hue slider bound to a proxy holder in plate space —
    // it edits the color and drives which slice the plate displays.
    if (!_hue_slider && plateType == ColorPlateType::Rect && _sliders) {
        const auto plate_space = plate->plateSpace();
        const auto& plate_components = Space::Components::get(plate_space);
        const auto plate_anyspace = Manager::get().find(plate_space);
        if (const auto hue = plate_components.hue_index(); hue && plate_anyspace) {
            _plate_color = std::make_shared<ColorHolder>(plate_anyspace, false);

            // page -> proxy: reuse the plate's color — its achromatic-hue
            // preservation has already been applied. Connected after
            // _color_plate_changed, so the plate runs first on this signal.
            _plate_color_sync = _space_color->signal_changed.connect([this]() {
                auto scoped = SignalBlocker{_plate_color_back};
                if (_space_color->get()) {
                    _plate_color->set(_color_plate->plateColor());
                }
            });
            // proxy -> plate: feed the plate-space color straight in so the
            // fixed channel moves even when the color is achromatic and the
            // source-space round-trip would be a no-op
            _plate_color_back = _plate_color->signal_changed.connect([this]() {
                auto scoped = SignalBlocker{_plate_color_sync};
                if (auto color = _plate_color->get()) {
                    _color_plate->setPlateColor(*color);
                }
            });

            _hue_slider = new ColorSlider(_plate_color, plate_components[*hue], this);
            _hue_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            _sliders->insertWidget(0, _hue_slider);
        }
    }
    return plate;
}

void ColorPage::showEvent(QShowEvent* event) {
    {
        // Refreshing the space-specific holder is a display sync, not a user
        // edit — it must not propagate back into _selected_color. A page
        // shown for the first time still has live connections (only hideEvent
        // blocks them), so block explicitly for the sync.
        auto scoped = SignalBlocker{_space_changed_connection};
        _space_color->clear();
        if (auto color = _selected_color->get()) {
            _space_color->set(*color);
        }
    }
    _space_changed_connection.unblock();
    _selected_changed_connection.unblock();
    QWidget::showEvent(event);
}

void ColorPage::hideEvent(QHideEvent* event) {
    _space_color->clear();
    _space_changed_connection.block();
    _selected_changed_connection.block();
    QWidget::hideEvent(event);
}

ColorPageChannel::ColorPageChannel(std::shared_ptr<ColorHolder> color,
                                   ColorSlider& slider, NumberEdit& edit)
    : ColorPageChannel(std::move(color), slider.getComponent(), &slider, edit) {}

ColorPageChannel::ColorPageChannel(std::shared_ptr<ColorHolder> color,
                                   const Inkscape::Colors::Space::Component& component,
                                   NumberEdit& edit)
    : ColorPageChannel(std::move(color), component, nullptr, edit) {}

ColorPageChannel::ColorPageChannel(std::shared_ptr<ColorHolder> color,
                                   const Inkscape::Colors::Space::Component& component,
                                   ColorSlider* slider, NumberEdit& edit)
    : _slider(slider)
    , _edit(edit)
    , _component(component)
    , _color(std::move(color)) {
    _color_changed = _color->signal_changed.connect([this]() {
        if (_color->isValid(_component)) {
            auto scoped = _update.block();
            _edit.setValue(_color->getComponent(_component) * _component.scale);
        }
    });

    _edit_changed = QObject::connect(&_edit, &NumberEdit::valueChanged, [this](double value) {
        if (!_update.pending()) {
            _color->setComponent(_component, value / _component.scale);
        }
    });

    if (_slider) {
        _slider_changed = QObject::connect(_slider, &ColorSlider::valueChanged, [this]() {
            if (!_update.pending()) {
                auto scoped = _update.block();
                _edit.setValue(_slider->getScaled());
            }
        });
    }
}

} // namespace Linea::UI
