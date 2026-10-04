// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * variable-font axes editor.
 */

#include "font-variations.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <glibmm/i18n.h>

#include "libnrtype/OpenTypeUtil.h"
#include "libnrtype/font-factory.h"
#include "libnrtype/font-instance.h" // IWYU pragma: keep
#include "number-edit.h"
#include "style-internal.h"

namespace Linea::UI {

namespace {

Glib::ustring get_axis_tooltip(const std::string& tag) {

    static std::map<std::string, Glib::ustring> map = {

        // ----- Axes defined in font by font designer (upper case ASCII) -----
        // Note: these are NOT standarized and the following tooltips could be incorrect.

        // TRANSLATORS: “Grade” (GRAD in CSS) is an axis that can be used to alter stroke thicknesses (or other forms)
        // without affecting the type's overall width, inter-letter spacing, or kerning — unlike altering weight.
        {"GRAD", _("Alter stroke thicknesses (or other forms) without affecting the type’s overall width")},
        // TRANSLATORS: “Parametric Thick Stroke”, XOPQ, is a reference to its logical name, “X Opaque”,
        // which describes how it alters the opaque stroke forms of glyphs typically in the X dimension
        {"XOPQ", _("Alter the opaque stroke forms of glyphs in the X dimension")},
        // TRANSLATORS: “Parametric Thin Stroke”, YOPQ, is a reference to its logical name, “Y Opaque”,
        // which describes how it alters the opaque stroke forms of glyphs typically in the Y dimension
        {"YOPQ", _("Alter the opaque stroke forms of glyphs in the Y dimension")},
        // TRANSLATORS: “Parametric Counter Width”, XTRA, is a reference to its logical name, “X-Transparent,”
        // which describes how it alters a font’s transparent spaces (also known as negative shapes)
        // inside and around all glyphs along the X dimension
        {"XTRA", _("Alter the transparent spaces inside and around all glyphs along the X dimension")},
        {"YTRA", _("Alter the transparent spaces inside and around all glyphs along the Y dimension")},
        // TRANSLATORS: Width/height of Chinese glyphs
        {"XTCH", _("Alter the width of Chinese glyphs")},
        {"YTCH", _("Alter the height of Chinese glyphs")},
        // TRANSLATORS: “Parametric Lowercase Height”
        {"YTLC", _("Vary the height of counters and other spaces between the baseline and x-height")},
        // TRANSLATORS: “Parametric Uppercase Counter Height”
        {"YTUC", _("Vary the height of uppercase letterforms")},
        // TRANSLATORS: “Parametric Ascender Height”
        {"YTAS", _("Vary the height of lowercase ascenders")},
        // TRANSLATORS: “Parametric Descender Depth”
        {"YTDE", _("Vary the depth of lowercase descenders")},
        // TRANSLATORS: “Parametric Figure Height”
        {"YTFI", _("Vary the height of figures")},
        // TRANSLATORS: "Serif rise" - found in the wild (https://github.com/googlefonts/amstelvar)
        {"YTSE", _("Vary the shape of the serifs")},
        // TRANSLATORS: Flare - flaring of the stems
        {"FLAR", _("Controls the flaring of the stems")},
        // TRANSLATORS: Volume - The volume axis works only in combination with the Flare axis. It transforms the serifs
        // and adds a little more edge to details.
        {"VOLM", _("Volume works in combination with flare to transform serifs")},
        // Softness
        {"SOFT", _("Softness makes letterforms more soft and rounded")},
        // Casual
        {"CASL", _("Adjust the letterforms from a more serious style to a more casual style")},
        // Cursive
        {"CRSV", _("Control the substitution of cursive forms")},
        // Fill
        {"FILL", _("Fill can turn transparent forms opaque")},
        // Monospace
        {"MONO", _("Adjust the glyphs from a proportional width to a fixed width")},
        // Wonky
        {"WONK", _("Binary switch used to control substitution of “wonky” forms")},
        // Element shape
        {"ESHP", _("Selection of the base element glyphs are composed of")},
        // Element shape
        {"ELSH", _("Controls element shape characteristics")},
        // Element grid
        {"ELGR", _("Controls how many elements are used per one grid unit")},
        // Element grid
        {"EGRD", _("Controls how many elements are used per one grid unit")},
        // Proposed axis "height"
        {"HGHT", _("Controls the font file’s height parameter")},
        // Non-standard Y-axis stem thickness
        {"YAXS", _("Controls stem thickness in vertical direction")},
        // Vertical Element Alignment
        {"YELA", _("Controls vertical element alignment")},
        // Corner roundness
        {"ROND", _("Controls corner roundness")},
        // Bleed
        {"BLED", _("Controls ink bleed effect")},
        // Scanlines
        {"SCAN", _("Controls scanline effect")},
        // Morph
        {"MORF", _("Controls morphing characteristics")},
        // Extrusion
        {"EDPT", _("Controls depth of extrusion")},
        // Edge highlight
        {"EHLT", _("Controls edge highlighting")},
        // Hyper expansion
        {"HEXP", _("Controls hyper expansion characteristics")},
        // Bounce
        {"BNCE", _("Controls bounce/spring effect")},
        // Informal
        {"INFM", _("Controls informality characteristics")},
        // Spacing
        {"SPAC", _("Controls character spacing")},
        // Negative space
        {"NEGA", _("Controls negative spacing")},
        // X-rotation
        {"XROT", _("Controls character 3D horizontal rotation")},
        // Y-rotation
        {"YROT", _("Controls character 3D vertical rotation")},
        // Sharpness
        {"SHRP", _("Controls sharpness characteristics")},


        // ----- Axes defined in OpenType specification (lower case ASCII) -----

        // TRANSLATORS: “Optical Size”
        // Optical sizes in a variable font are different versions of a typeface optimized for use at singular specific sizes,
        // such as 14 pt or 144 pt. Small (or body) optical sizes tend to have less stroke contrast, more open and wider spacing,
        // and a taller x-height than those of their large (or display) counterparts.
        {"opsz", _("Optimize the typeface for use at specific size")},
        // TRANSLATORS: Slant controls the font file’s slant parameter for oblique styles.
        {"slnt", _("Controls the font file’s slant parameter for oblique styles")},
        // Italic
        {"ital", _("Turns on the font’s italic forms")},
        // TRANSLATORS: Weight controls the font file’s weight parameter.
        {"wght", _("Controls the font file’s weight parameter")},
        // TRANSLATORS: Width controls the font file’s width parameter.
        {"wdth", _("Controls the font file’s width parameter")},


        // ----- Experimental values, see https://variationsguide.typenetwork.com -----
        // Note: These are NOT part of the OpenType specification despite being lower case ASCII.

        {"xtab", _("Controls the tabular width")},
        {"udln", _("Controls the weight of an underline")},
        {"shdw", _("Controls the depth of a shadow")},
        {"refl", _("Controls the Y reflection")},
        {"otln", _("Controls the weight of a font’s outline")},
        {"engr", _("Controls the width of an engraving")},
        {"embo", _("Controls the depth of an emboss")},
        {"rxad", _("Controls the relative X advance - horizontal motion of the glyph")},
        {"ryad", _("Controls the relative Y advance - vertical motion of the glyph")},
        {"rsec", _("Controls the relative second value - as in one second of animation time")},
        {"vrot", _("Controls the rotation of the glyph in degrees")},
        {"vuid", _("Controls the glyph’s unicode ID")},
        {"votf", _("Controls the glyph’s feature variation")},
    };

    auto it = map.find(tag);
    if (it != end(map)) {
        return it->second;
    }
    else {
        return Glib::ustring(tag);
    }
}

int value_to_slider(double value, double min, double max) {
    if (max <= min) return 0;
    return static_cast<int>(1000.0 * (value - min) / (max - min));
}

double slider_to_value(int slider, double min, double max) {
    return min + (max - min) * slider / 1000.0;
}

// Fold the resolved style's weight/stretch/slant into axis values.
// Named instances ("Thin", "Bold", ...) encode their axis values in the
// Pango font description rather than in font-variation-settings.
void apply_description_axes(std::vector<OTVarAxis>& axes, PangoFontDescription* descr) {
    if (!descr) return;

    auto set_axis = [&axes](const char* tag, double value) {
        auto it = std::find_if(axes.begin(), axes.end(),
                               [tag](const auto& axis) { return axis.tag == tag; });
        if (it != axes.end()) {
            it->set_val = std::clamp(value, it->minimum, it->maximum);
        }
    };

    // Pango weight values already match the wght scale (100..900).
    set_axis("wght", pango_font_description_get_weight(descr));

    // PangoStretch maps onto the wdth percentage scale.
    static const double stretch_pct[] = {50, 62.5, 75, 87.5, 100, 112.5, 125, 150, 200};
    int stretch = pango_font_description_get_stretch(descr);
    if (stretch >= PANGO_STRETCH_ULTRA_CONDENSED && stretch <= PANGO_STRETCH_ULTRA_EXPANDED) {
        set_axis("wdth", stretch_pct[stretch]);
    }

    // Italic/oblique styles map onto the ital/slnt axes.
    auto style = pango_font_description_get_style(descr);
    if (style == PANGO_STYLE_ITALIC) {
        set_axis("ital", 1);
    } else if (style == PANGO_STYLE_OBLIQUE) {
        set_axis("slnt", -14); // CSS oblique default angle
    }
}

} // namespace

FontVariations::FontVariations(QWidget* parent)
    : QWidget(parent) {
    // Outer layout holds the container; container holds the grid of axes.
    // Deleting the container in build_ui tears down all axis widgets in one
    // call (QWidget destructor deletes all children).
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
}

FontVariations::~FontVariations() = default;

void FontVariations::update(const Glib::ustring& font_spec, const SPIFontVariationSettings* variations) {
    auto res = ::FontFactory::get().FaceFromFontSpecification(font_spec.c_str());
    auto axes = res ? res->get_opentype_varaxes() : std::vector<OTVarAxis>();

    if (res && !axes.empty()) {
        apply_description_axes(axes, res->get_descr());
    }
    if (variations) {
        for (const auto& [name, value] : variations->axes) {
            auto it =
                std::find_if(axes.begin(), axes.end(), [name](const auto& axis) { return axis.tag == name.raw(); });
            if (it != axes.end()) {
                it->set_val = std::clamp(static_cast<double>(value), it->minimum, it->maximum);
            }
        }
    }
    update_axes(axes);
}

void FontVariations::update_axes(const std::vector<OTVarAxis>& axes) {
    bool rebuild = false;
    if (_ot_axes.size() != axes.size()) {
        rebuild = true;
    } else {
        bool identical = std::equal(begin(axes), end(axes), begin(_ot_axes));
        if (identical) return;

        bool same_def = std::equal(begin(axes), end(axes), begin(_ot_axes), [](const auto& a, const auto& b) {
            return a.same_definition(b);
        });
        if (!same_def) rebuild = true;
    }

    auto scoped = _update.block();

    if (rebuild) {
        build_ui(axes);
    } else {
        for (size_t i = 0; i < _axes.size() && i < axes.size(); ++i) {
            if (_axes[i].name == axes[i].name) {
                const auto eps = 0.00001;
                if (std::abs(_axes[i].spin->value() - axes[i].set_val) > eps) {
                    _axes[i].spin->setValue(axes[i].set_val);
                }
            }
        }
    }

    _ot_axes = axes;
}

void FontVariations::build_ui(const std::vector<OTVarAxis>& ot_axes) {
    // Delete the old container — its destructor cleans up all axis widgets
    // and the grid layout in one call.
    delete _container;
    _axes.clear();

    // Fresh container + grid each time. QGridLayout never shrinks its internal
    // row/column count (rr/cc only grow via expand()), so reusing a grid after
    // going from N axes to fewer would leave stale empty rows.
    _container = new QWidget(this);
    static_cast<QVBoxLayout*>(layout())->addWidget(_container);

    auto grid = new QGridLayout(_container);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 0);

    int row = 0;
    for (const auto& axis : ot_axes) {
        // The font's own name table provides the label (localized by the
        // designer); the tag fallback covers fonts missing English names.
        Glib::ustring label_text = axis.name.empty() ? Glib::ustring(axis.tag) : axis.name;
        auto tt = QString::fromUtf8(get_axis_tooltip(axis.tag).c_str());

        auto label = new QLabel(QString::fromUtf8(label_text.c_str()), _container);
        label->setToolTip(tt);
        label->setProperty("class", "panel-label");
        grid->addWidget(label, row++, 0, 1, 2);

        int precision = 2 - static_cast<int>(std::log10(axis.maximum - axis.minimum));
        if (precision < 0) precision = 0;

        auto slider = new QSlider(Qt::Horizontal, _container);
        slider->setRange(0, 1000);
        slider->setValue(value_to_slider(axis.set_val, axis.minimum, axis.maximum));
        slider->setVisible(_scales_visible);
        grid->addWidget(slider, row, 0);

        auto spin = new NumberEdit(_container);
        spin->setToolTip(tt);
        spin->setRange(axis.minimum, axis.maximum);
        spin->setDecimals(precision);
        spin->setValue(axis.set_val);
        spin->setSingleStep(std::pow(10.0, -precision));
        grid->addWidget(spin, row, 1);

        // Keep slider and spin in sync without loops.
        connect(spin, &NumberEdit::valueChanged, slider, [slider, axis](double value) {
            QSignalBlocker b(slider);
            slider->setValue(value_to_slider(value, axis.minimum, axis.maximum));
        });
        connect(slider, &QSlider::valueChanged, spin, [spin, axis](int value) {
            QSignalBlocker b(spin);
            spin->setValue(slider_to_value(value, axis.minimum, axis.maximum));
        });

        // Forward any user edit to the changed signal.
        connect(spin, &NumberEdit::valueChanged, this, [this] {
            if (!_update.pending()) Q_EMIT changed();
        });
        connect(slider, &QSlider::valueChanged, this, [this] {
            if (!_update.pending()) Q_EMIT changed();
        });

        _axes.push_back({axis.name, axis.tag, label, spin, slider, precision, axis.def});
        ++row;
    }

    // Force re-polish so qss rules (min-height:22px on NumberEdit, font-size
    // on .panel-label) are applied. unpolish+polish forces re-evaluation of
    // stylesheet rules against the widget's current properties.
    for (auto& a : _axes) {
        a.label->style()->unpolish(a.label);
        a.label->style()->polish(a.label);
        a.spin->style()->unpolish(a.spin);
        a.spin->style()->polish(a.spin);
        a.slider->style()->unpolish(a.slider);
        a.slider->style()->polish(a.slider);
    }
    // polish() posts FontChange/StyleChange events asynchronously; QLabel
    // caches its sizeHint and only invalidates the cache on FontChange. Flush
    // posted events so sizeHint() sees the updated font metrics.
    QCoreApplication::sendPostedEvents();
    // Invalidate the cached sizeHint so the next sizeHint() call recomputes
    // from the children's (now correctly polished) sizeHints.
    _container->layout()->invalidate();
}

Glib::ustring FontVariations::get_pango_string(bool include_defaults) const {
    Glib::ustring pango_string;

    if (!_axes.empty()) {
        pango_string += "@";

        for (const auto& row : _axes) {
            if (!include_defaults && row.spin->value() == row.def) continue;

            std::ostringstream str;
            str << std::fixed << std::setprecision(row.precision) << row.spin->value();
            pango_string += row.tag + "=" + str.str() + ",";
        }

        if (pango_string.size() > 1) {
            pango_string.erase(pango_string.size() - 1); // erase trailing ','
        } else {
            pango_string.clear(); // only '@'
        }
    }

    return pango_string;
}

SPIFontVariationSettings FontVariations::get_variations() const {
    SPIFontVariationSettings settings;
    for (const auto& row : _axes) {
        settings.axes[row.tag] = static_cast<float>(row.spin->value());
    }
    settings.normal = settings.axes.empty();
    settings.set = !settings.axes.empty();
    return settings;
}

bool FontVariations::variationsPresent() const {
    return !_axes.empty();
}

void FontVariations::set_scales_visible(bool visible) {
    if (_scales_visible == visible) return;
    _scales_visible = visible;
    for (auto& row : _axes) {
        row.slider->setVisible(visible);
    }
}

int FontVariations::measureHeight() {
    if (_axes.empty() || !_container) return 0;

    return sizeHint().height();
}

int FontVariations::measureHeight(int axis_count) {
    std::vector<OTVarAxis> axes(axis_count);
    for (int i = 0; i < axis_count; ++i) {
        axes[i].tag = std::to_string(i);
        axes[i].name = axes[i].tag;
    }
    build_ui(axes);
    auto h = measureHeight();
    build_ui({});
    return h;
}

} // namespace Linea::UI
