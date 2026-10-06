// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorHolder — observable single-color model for the Qt color widgets.
 */

#include "color-holder.h"

#include "colors/spaces/base.h"

namespace Linea::UI {

using namespace Inkscape::Colors;

ColorHolder::ColorHolder(std::shared_ptr<Space::AnySpace> space, bool alpha)
    : _space(std::move(space))
    , _alpha(alpha) {}

void ColorHolder::set(Color color) {
    if (_space) {
        color.convert(_space);
    }
    color.enableOpacity(_alpha);

    // Near-equal writes are absorbed, keeping the held color (and its space)
    // verbatim — a write->serialize->parse->set cycle must not drift the
    // model or notify observers.
    if (_color && _color->isNear(color)) return;
    _color = std::move(color);
    signal_changed.emit();
}

Color ColorHolder::getOrDefault() const {
    if (_color) return *_color;

    Color black(0x000000ffu);
    if (_space) {
        black.convert(_space);
    }
    return black;
}

void ColorHolder::clear() {
    if (_color) {
        _color.reset();
        signal_cleared.emit();
    }
}

const Space::Components& ColorHolder::getComponents() const {
    if (!_space) {
        throw ColorError("Components are only available on a color space constrained ColorHolder.");
    }
    return _space->getComponents(_alpha);
}

bool ColorHolder::isValid(const Space::Component& component) const {
    return _space && _space->getComponentType() == component.type;
}

bool ColorHolder::setComponent(const Space::Component& component, double value) {
    if (!_color || !isValid(component)) return false;

    auto next = *_color;
    next.set(component.index, value);
    if (_color->isNear(next)) return false;

    *_color = std::move(next);
    signal_changed.emit();
    return true;
}

double ColorHolder::getComponent(const Space::Component& component) const {
    if (!_color || !isValid(component)) return 0.0;

    return _color->get(component.index);
}

} // namespace Linea::UI
