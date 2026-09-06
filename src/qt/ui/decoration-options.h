// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DecorationOptions - text decoration line style, thickness and color editor.
 *
 * Extracted from the inline "decoration popover" in text-panel.ui so the
 * popover content is a self-contained widget with its own grid layout,
 * signal wiring and state push. The TextPanel only toggles its visibility.
 */

#ifndef LINEA_UI_DECORATION_OPTIONS_H
#define LINEA_UI_DECORATION_OPTIONS_H

#include <memory>
#include <QWidget>

#include "colors/color.h"
#include "util/text-utils.h"  // DecorationThicknessState

QT_BEGIN_NAMESPACE
namespace Ui {
class DecorationOptions;
}
QT_END_NAMESPACE

namespace Linea::Props { class Binder; }

namespace Linea::UI {

/**
 * Widget for editing text decoration line style, thickness and color.
 *
 * Exposes the three sub-controls (line style buttons, thickness radio +
 * spin box, color radio + picker) and emits high-level change signals so
 * the host panel can apply CSS and record undo steps.
 */
class DecorationOptions : public QWidget {
    Q_OBJECT

public:
    explicit DecorationOptions(QWidget* parent = nullptr);
    ~DecorationOptions() override;

    // --- line style (0=solid, 1=double, 2=dotted, 3=dashed, 4=wavy) ---
    void setLineStyle(int style);
    void setLineStyleMixed(bool mixed);

    // --- thickness ---
    void setThickness(const DecorationThicknessState& state, bool mixed = false);

    // --- color ---
    void setDecorationColor(const std::optional<Inkscape::Colors::Color>& color, bool mixed = false);

    /// Declarative binding through a Binder (replaces manual push + signals
    /// when the host panel uses the property system).
    void bind(Props::Binder& binder);

Q_SIGNALS:
    void lineStyleChanged(int style);
    void thicknessChanged(const DecorationThicknessState& state);
    void decorationColorChanged(const std::optional<Inkscape::Colors::Color>& color);

private:
    void setupWidgets();
    void setupConnections();

    std::unique_ptr<Ui::DecorationOptions> _ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_DECORATION_OPTIONS_H
