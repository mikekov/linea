// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorHolder — observable single-color model for the Qt color widgets.
 *
 * Replaces Inkscape::Colors::ColorSet for panel widgets: a ColorSet is an id→color map for multi-color editing;
 * the Qt panels only ever edit one color, so this holds a single Color with the same space/alpha constraint semantics.
 */

#ifndef LINEA_UI_COLOR_HOLDER_H
#define LINEA_UI_COLOR_HOLDER_H

#include <memory>
#include <optional>
#include <sigc++/scoped_connection.h>
#include <sigc++/signal.h>

#include "colors/color.h"
#include "colors/spaces/components.h"

namespace Linea::UI {

class ColorHolder {
public:
    explicit ColorHolder(std::shared_ptr<Inkscape::Colors::Space::AnySpace> space = nullptr, bool alpha = true);

    // whole-color access; set() converts to the constraint space when one is
    // configured, applies the alpha constraint, and absorbs writes that are
    // isNear() the held color (keeping the held value verbatim)
    void set(Inkscape::Colors::Color color);
    const std::optional<Inkscape::Colors::Color>& get() const { return _color; }
    // stored color, or black converted into the constraint space when empty
    Inkscape::Colors::Color getOrDefault() const;
    void clear();
    bool isEmpty() const { return !_color.has_value(); }

    // constraint accessors
    bool hasAlpha() const { return _alpha; }
    const Inkscape::Colors::Space::Components& getComponents() const;
    bool isValid(const Inkscape::Colors::Space::Component& component) const;

    // per-channel access for sliders/spinboxes; component must belong to the
    // constraint space (isValid)
    bool setComponent(const Inkscape::Colors::Space::Component& component, double value);
    double getComponent(const Inkscape::Colors::Space::Component& component) const;

    sigc::signal<void()> signal_changed;
    sigc::signal<void()> signal_cleared;

private:
    std::optional<Inkscape::Colors::Color> _color;
    std::shared_ptr<Inkscape::Colors::Space::AnySpace> _space;
    bool _alpha;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_HOLDER_H
