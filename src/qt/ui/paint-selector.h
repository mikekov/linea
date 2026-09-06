// SPDX-License-Identifier: GPL-2.0-or-later

// Simple paint selector widget
// https://gitlab.com/inkscape/ux/-/issues/246

#ifndef LINEA_UI_PAINT_SELECTOR_H
#define LINEA_UI_PAINT_SELECTOR_H

#include <QWidget>
#include <memory>
#include <optional>
#include <string>

#include "colors/color.h"
#include "object/sp-gradient.h"
#include "ui/widget/edit-operation.h"
#include "ui/widget/paint-enums.h"
#include "util/style-utils.h"

class SPDesktop;
class SPDocument;
class SPGradient;
class SPHatch;
class SPIPaint;
class SPPattern;

namespace Geom {
class Affine;
class Point;
class Scale;
}

namespace Inkscape::UI::Widget { enum class PaintMode; enum class PaintDerivedMode; }

namespace Linea::UI {

enum class FillRule {
    NonZero,
    EvenOdd
};

QString get_paint_mode_icon(Inkscape::UI::Widget::PaintMode mode);
QString get_paint_mode_name(Inkscape::UI::Widget::PaintMode mode);

class PaintSelector : public QWidget {
    Q_OBJECT
public:
    PaintSelector();

    // create a new PaintSelector widget; if 'support_no_paint' is true, then add the "no paint" toggle button too
    static std::unique_ptr<PaintSelector> create(bool support_no_paint, bool support_fill_rule, bool compact_mode = false, QWidget* parent = nullptr);

    virtual void setDesktop(SPDesktop* desktop) = 0;
    virtual void setDocument(SPDocument* document) = 0;
    virtual void setMode(Inkscape::UI::Widget::PaintMode mode) = 0;
    virtual void updateFromPaint(const SPIPaint& paint) = 0;
    virtual void updateFromPaintProps(const Linea::PaintProp& paint) = 0;
    virtual void setFillRule(FillRule fill_rule) = 0;
    /**
     * Shows a placeholder for PaintSelector.
     *
     * @param text          The text to display in the placeholder label.
     * @param reset_toggles If true, visually deselects active paint mode button.
     */
    virtual void showPlaceholder(const QString& text, bool reset_toggles) = 0;

    // flat colors
    virtual void setColor(const Inkscape::Colors::Color& color) = 0;

Q_SIGNALS:
    void flatColorChanged(const Inkscape::Colors::Color& color);
    void modeChanged(Inkscape::UI::Widget::PaintMode mode);
    void gradientChanged(SPGradient* gradient, SPGradientType type);
    void swatchChanged(SPGradient* swatch, Inkscape::UI::EditOperation op, SPGradient* replacement,
                       std::optional<Inkscape::Colors::Color> color, QString label);
    void patternChanged(SPPattern* pattern, std::optional<Inkscape::Colors::Color> color,
                        const QString& label, const Geom::Affine& transform,
                        const Geom::Point& offset, bool uniform_scale, const Geom::Scale& gap);
    void hatchChanged(SPHatch* hatch, std::optional<Inkscape::Colors::Color> color,
                      const QString& label, const Geom::Affine& transform,
                      const Geom::Point& offset, double pitch, double rotation, double thickness);
    void meshChanged(SPGradient* mesh);
    void fillRuleChanged(FillRule fill_rule);
    void inheritModeChanged(Inkscape::UI::Widget::PaintDerivedMode mode);
};

} // namespace Linea::UI

#endif // LINEA_UI_PAINT_SELECTOR_H
