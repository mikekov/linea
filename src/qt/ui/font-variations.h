// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * variable-font axes editor.
 */

#ifndef LINEA_UI_FONT_VARIATIONS_H
#define LINEA_UI_FONT_VARIATIONS_H

#include <vector>

#include <QWidget>

#include <glibmm/ustring.h>

#include "ui/operation-blocker.h"

class QLabel;
class QSlider;
class QWidget;
class SPIFontVariationSettings;
struct OTVarAxis;

namespace Linea::UI {

class NumberEdit;

/**
 * Widget for editing OpenType variable font axis values.
 */
class FontVariations : public QWidget {
    Q_OBJECT

public:
    explicit FontVariations(QWidget* parent = nullptr);
    ~FontVariations() override;

    /// Rebuild or update the axes UI from a Pango font spec.
    void update(const Glib::ustring& font_spec, const SPIFontVariationSettings* variations = nullptr);

    /// Return Pango-style font variations string (e.g. "wght=400,wdth=100").
    Glib::ustring get_pango_string(bool include_defaults = false) const;

    /// Return the current axis values (all axes, defaults included) as a
    /// style-internal value, keyed by OpenType tag.
    SPIFontVariationSettings get_variations() const;

    /// True if the font has any variation axes.
    bool variationsPresent() const;
    int axisCount() const { return static_cast<int>(_axes.size()); }

    /// Show/hide the sliders (leaving only the spin boxes).
    void set_scales_visible(bool visible);

    /// Measure the height of the current UI or a simulated UI with N axes.
    int measureHeight();
    int measureHeight(int axis_count);

Q_SIGNALS:
    /// Emitted when any axis value changes.
    void changed();

private:
    void build_ui(const std::map<Glib::ustring, OTVarAxis>& axes);
    void update_axes(const std::map<Glib::ustring, OTVarAxis>& axes);

    /// One row in the axes grid: the per-axis widgets and metadata.
    struct AxisRow {
        Glib::ustring name;
        QLabel* label = nullptr;
        NumberEdit* spin = nullptr;
        QSlider* slider = nullptr;
        int precision = 0;
        double def = 0;
    };

    std::vector<AxisRow> _axes;
    QWidget* _container = nullptr;
    std::map<Glib::ustring, OTVarAxis> _open_type_axes;
    OperationBlocker _update;
    bool _scales_visible = true;
};

} // namespace Linea::UI

#endif // LINEA_UI_FONT_VARIATIONS_H
