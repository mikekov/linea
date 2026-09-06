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
#include <boost/algorithm/string.hpp>
#include <glibmm/i18n.h>

#include "libnrtype/OpenTypeUtil.h"
#include "libnrtype/font-factory.h"
#include "libnrtype/font-instance.h" // IWYU pragma: keep
#include "number-edit.h"
#include "style-internal.h"

namespace Linea::UI {

namespace {

std::pair<Glib::ustring, Glib::ustring> get_axis_name(const std::string& tag, const Glib::ustring& abbr) {
    // Transformed axis names;
    // mainly from https://fonts.google.com/knowledge/using_type/introducing_parametric_axes
    // CC BY-SA 4.0
    // Standard axes guide for reference: https://variationsguide.typenetwork.com
    // Other references:
    // - https://fonts.google.com/variablefonts#axis-definitions
    // - https://canary.grida.co/docs/reference/open-type-variable-axes

    static std::map<std::string, std::pair<Glib::ustring, Glib::ustring>> map = {
        // TRANSLATORS: “Grade” (GRAD in CSS) is an axis that can be used to alter stroke thicknesses (or other forms)
        // without affecting the type's overall width, inter-letter spacing, or kerning — unlike altering weight.
        {"GRAD",
         std::make_pair(C_("Variable font axis", "Grade"),
                        _("Alter stroke thicknesses (or other forms) without affecting the type’s overall width"))},
        // TRANSLATORS: “Parametric Thick Stroke”, XOPQ, is a reference to its logical name, “X Opaque”,
        // which describes how it alters the opaque stroke forms of glyphs typically in the X dimension
        {"XOPQ", std::make_pair(C_("Variable font axis", "X opaque"),
                                _("Alter the opaque stroke forms of glyphs in the X dimension"))},
        // TRANSLATORS: “Parametric Thin Stroke”, YOPQ, is a reference to its logical name, “Y Opaque”,
        // which describes how it alters the opaque stroke forms of glyphs typically in the Y dimension
        {"YOPQ", std::make_pair(C_("Variable font axis", "Y opaque"),
                                _("Alter the opaque stroke forms of glyphs in the Y dimension"))},
        // TRANSLATORS: “Parametric Counter Width”, XTRA, is a reference to its logical name, “X-Transparent,”
        // which describes how it alters a font’s transparent spaces (also known as negative shapes)
        // inside and around all glyphs along the X dimension
        {"XTRA", std::make_pair(C_("Variable font axis", "X transparent"),
                                _("Alter the transparent spaces inside and around all glyphs along the X dimension"))},
        {"YTRA", std::make_pair(C_("Variable font axis", "Y transparent"),
                                _("Alter the transparent spaces inside and around all glyphs along the Y dimension"))},
        // TRANSLATORS: Width/height of Chinese glyphs
        {"XTCH",
         std::make_pair(C_("Variable font axis", "X transparent Chinese"), _("Alter the width of Chinese glyphs"))},
        {"YTCH",
         std::make_pair(C_("Variable font axis", "Y transparent Chinese"), _("Alter the height of Chinese glyphs"))},
        // TRANSLATORS: “Parametric Lowercase Height”
        {"YTLC", std::make_pair(C_("Variable font axis", "Lowercase height"),
                                _("Vary the height of counters and other spaces between the baseline and x-height"))},
        // TRANSLATORS: “Parametric Uppercase Counter Height”
        {"YTUC",
         std::make_pair(C_("Variable font axis", "Uppercase height"), _("Vary the height of uppercase letterforms"))},
        // TRANSLATORS: “Parametric Ascender Height”
        {"YTAS",
         std::make_pair(C_("Variable font axis", "Ascender height"), _("Vary the height of lowercase ascenders"))},
        // TRANSLATORS: “Parametric Descender Depth”
        {"YTDE",
         std::make_pair(C_("Variable font axis", "Descender depth"), _("Vary the depth of lowercase descenders"))},
        // TRANSLATORS: “Parametric Figure Height”
        {"YTFI", std::make_pair(C_("Variable font axis", "Figure height"), _("Vary the height of figures"))},
        // TRANSLATORS: "Serif rise" - found in the wild (https://github.com/googlefonts/amstelvar)
        {"YTSE", std::make_pair(C_("Variable font axis", "Serif rise"), _("Vary the shape of the serifs"))},
        // TRANSLATORS: Flare - flaring of the stems
        {"FLAR", std::make_pair(C_("Variable font axis", "Flare"), _("Controls the flaring of the stems"))},
        // TRANSLATORS: Volume - The volume axis works only in combination with the Flare axis. It transforms the serifs
        // and adds a little more edge to details.
        {"VOLM", std::make_pair(C_("Variable font axis", "Volume"),
                                _("Volume works in combination with flare to transform serifs"))},
        // Softness
        {"SOFT",
         std::make_pair(C_("Variable font axis", "Softness"), _("Softness makes letterforms more soft and rounded"))},
        // Casual
        {"CASL", std::make_pair(C_("Variable font axis", "Casual"),
                                _("Adjust the letterforms from a more serious style to a more casual style"))},
        // Cursive
        {"CRSV", std::make_pair(C_("Variable font axis", "Cursive"), _("Control the substitution of cursive forms"))},
        // Fill
        {"FILL", std::make_pair(C_("Variable font axis", "Fill"), _("Fill can turn transparent forms opaque"))},
        // Monospace
        {"MONO", std::make_pair(C_("Variable font axis", "Monospace"),
                                _("Adjust the glyphs from a proportional width to a fixed width"))},
        // Wonky
        {"WONK", std::make_pair(C_("Variable font axis", "Wonky"),
                                _("Binary switch used to control substitution of “wonky” forms"))},
        // Element shape
        {"ESHP", std::make_pair(C_("Variable font axis", "Element shape"),
                                _("Selection of the base element glyphs are composed of"))},
        // Element shape
        {"ELSH",
         std::make_pair(C_("Variable font axis", "Element shape"), _("Controls element shape characteristics"))},
        // Element grid
        {"ELGR", std::make_pair(C_("Variable font axis", "Element grid"),
                                _("Controls how many elements are used per one grid unit"))},
        // Element grid
        {"EGRD", std::make_pair(C_("Variable font axis", "Element grid"),
                                _("Controls how many elements are used per one grid unit"))},
        // Proposed axis "height"
        {"HGHT", std::make_pair(C_("Variable font axis", "Height"), _("Controls the font file’s height parameter"))},
        // Non-standard Y-axis stem thickness
        {"YAXS",
         std::make_pair(C_("Variable font axis", "Y-Axis"), _("Controls stem thickness in vertical direction"))},
        // Vertical Element Alignment
        {"YELA", std::make_pair(C_("Variable font axis", "Vertical align"), _("Controls vertical element alignment"))},
        // Corner roundness
        {"ROND", std::make_pair(C_("Variable font axis", "Roundness"), _("Controls corner roundness"))},
        // Bleed
        {"BLED", std::make_pair(C_("Variable font axis", "Bleed"), _("Controls ink bleed effect"))},
        // Scanlines
        {"SCAN", std::make_pair(C_("Variable font axis", "Scanlines"), _("Controls scanline effect"))},
        // Morph
        {"MORF", std::make_pair(C_("Variable font axis", "Morph"), _("Controls morphing characteristics"))},
        // Extrusion
        {"EDPT", std::make_pair(C_("Variable font axis", "Extrusion depth"), _("Controls depth of extrusion"))},
        // Edge highlight
        {"EHLT", std::make_pair(C_("Variable font axis", "Edge highlight"), _("Controls edge highlighting"))},
        // Hyper expansion
        {"HEXP",
         std::make_pair(C_("Variable font axis", "Hyper expansion"), _("Controls hyper expansion characteristics"))},
        // Bounce
        {"BNCE", std::make_pair(C_("Variable font axis", "Bounce"), _("Controls bounce/spring effect"))},
        // Informal
        {"INFM", std::make_pair(C_("Variable font axis", "Informality"), _("Controls informality characteristics"))},
        // Spacing
        {"SPAC", std::make_pair(C_("Variable font axis", "Spacing"), _("Controls character spacing"))},
        // Negative space
        {"NEGA", std::make_pair(C_("Variable font axis", "Negative space"), _("Controls negative spacing"))},
        // X-rotation
        {"XROT",
         std::make_pair(C_("Variable font axis", "X rotation"), _("Controls character 3D horizontal rotation"))},
        // Y-rotation
        {"YROT", std::make_pair(C_("Variable font axis", "Y rotation"), _("Controls character 3D vertical rotation"))},
        // Sharpness
        {"SHRP", std::make_pair(C_("Variable font axis", "Sharpness"), _("Controls sharpness characteristics"))},
        // TRANSLATORS: “Optical Size”
        // Optical sizes in a variable font are different versions of a typeface optimized for use at singular specific
        // sizes,
        // such as 14 pt or 144 pt. Small (or body) optical sizes tend to have less stroke contrast, more open and wider
        // spacing,
        // and a taller x-height than those of their large (or display) counterparts.
        {"opsz",
         std::make_pair(C_("Variable font axis", "Optical size"), _("Optimize the typeface for use at specific size"))},
        // TRANSLATORS: Slant controls the font file’s slant parameter for oblique styles.
        {"slnt", std::make_pair(C_("Variable font axis", "Slant"),
                                _("Controls the font file’s slant parameter for oblique styles"))},
        // Italic
        {"ital", std::make_pair(C_("Variable font axis", "Italic"), _("Turns on the font’s italic forms"))},
        // TRANSLATORS: Weight controls the font file’s weight parameter.
        {"wght", std::make_pair(C_("Variable font axis", "Weight"), _("Controls the font file’s weight parameter"))},
        // TRANSLATORS: Width controls the font file’s width parameter.
        {"wdth", std::make_pair(C_("Variable font axis", "Width"), _("Controls the font file’s width parameter"))},
        //
        {"xtab", std::make_pair(C_("Variable font axis", "Tabular width"), _("Controls the tabular width"))},
        {"udln", std::make_pair(C_("Variable font axis", "Underline"), _("Controls the weight of an underline"))},
        {"shdw", std::make_pair(C_("Variable font axis", "Shadow"), _("Controls the depth of a shadow"))},
        {"refl", std::make_pair(C_("Variable font axis", "Reflection"), _("Controls the Y reflection"))},
        {"otln", std::make_pair(C_("Variable font axis", "Outline"), _("Controls the weight of a font’s outline"))},
        {"engr", std::make_pair(C_("Variable font axis", "Engrave"), _("Controls the width of an engraving"))},
        {"embo", std::make_pair(C_("Variable font axis", "Emboss"), _("Controls the depth of an emboss"))},
        {"rxad", std::make_pair(C_("Variable font axis", "Relative X advance"),
                                _("Controls the relative X advance - horizontal motion of the glyph"))},
        {"ryad", std::make_pair(C_("Variable font axis", "Relative Y advance"),
                                _("Controls the relative Y advance - vertical motion of the glyph"))},
        {"rsec", std::make_pair(C_("Variable font axis", "Relative second"),
                                _("Controls the relative second value - as in one second of animation time"))},
        {"vrot",
         std::make_pair(C_("Variable font axis", "Rotation"), _("Controls the rotation of the glyph in degrees"))},
        {"vuid", std::make_pair(C_("Variable font axis", "Unicode variation"), _("Controls the glyph’s unicode ID"))},
        {"votf",
         std::make_pair(C_("Variable font axis", "Feature variation"), _("Controls the glyph’s feature variation"))},
    };

    auto it = map.find(tag);
    if (it == end(map)) {
        // try lowercase variants
        it = map.find(boost::algorithm::to_lower_copy(tag));
    }
    if (it == end(map)) {
        // try uppercase variants
        it = map.find(boost::algorithm::to_upper_copy(tag));
    }
    if (it != end(map)) {
        return it->second;
    } else {
        return std::make_pair(abbr, "");
    }
}

int value_to_slider(double value, double min, double max) {
    if (max <= min) return 0;
    return static_cast<int>(1000.0 * (value - min) / (max - min));
}

double slider_to_value(int slider, double min, double max) {
    return min + (max - min) * slider / 1000.0;
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
    const auto& axes = res ? res->get_opentype_varaxes() : std::map<Glib::ustring, OTVarAxis>();

    if (variations) {
        auto copy = axes;
        for (const auto& [name, value] : variations->axes) {
            auto it =
                std::find_if(copy.begin(), copy.end(), [name](const auto& kv) { return kv.second.tag == name.raw(); });
            if (it != copy.end()) {
                it->second.set_val = std::min(it->second.maximum, std::max(it->second.minimum, (double)value));
            }
        }
        update_axes(copy);
    } else {
        update_axes(axes);
    }
}

void FontVariations::update_axes(const std::map<Glib::ustring, OTVarAxis>& axes) {
    bool rebuild = false;
    if (_open_type_axes.size() != axes.size()) {
        rebuild = true;
    } else {
        bool identical = std::equal(begin(axes), end(axes), begin(_open_type_axes));
        if (identical) return;

        bool same_def = std::equal(begin(axes), end(axes), begin(_open_type_axes), [](const auto& a, const auto& b) {
            return a.first == b.first && a.second.same_definition(b.second);
        });
        if (!same_def) rebuild = true;
    }

    auto scoped = _update.block();

    if (rebuild) {
        build_ui(axes);
    } else {
        auto it = begin(axes);
        for (auto& row : _axes) {
            if (it != end(axes) && row.name == it->first) {
                const auto eps = 0.00001;
                if (std::abs(row.spin->value() - it->second.set_val) > eps) {
                    row.spin->setValue(it->second.set_val);
                }
            }
            ++it;
        }
    }

    _open_type_axes = axes;
}

void FontVariations::build_ui(const std::map<Glib::ustring, OTVarAxis>& ot_axes) {
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
    for (const auto& a : ot_axes) {
        const auto& axis = a.second;
        auto [label_text, tooltip] = get_axis_name(axis.tag, a.first);
        auto tt = QString::fromUtf8(tooltip.c_str());

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

        _axes.push_back({a.first, label, spin, slider, precision, axis.def});
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

namespace {

// Map the display names of well-known axes back to their OpenType tags.
Glib::ustring axis_tag(const Glib::ustring& name) {
    if (name == "Width") return "wdth";
    if (name == "Weight") return "wght";
    if (name == "OpticalSize") return "opsz";
    if (name == "Slant") return "slnt";
    if (name == "Italic") return "ital";
    return name;
}

} // namespace

Glib::ustring FontVariations::get_pango_string(bool include_defaults) const {
    Glib::ustring pango_string;

    if (!_axes.empty()) {
        pango_string += "@";

        for (const auto& row : _axes) {
            if (!include_defaults && row.spin->value() == row.def) continue;

            std::ostringstream str;
            str << std::fixed << std::setprecision(row.precision) << row.spin->value();
            pango_string += axis_tag(row.name) + "=" + str.str() + ",";
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
        settings.axes[axis_tag(row.name)] = static_cast<float>(row.spin->value());
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
    std::map<Glib::ustring, OTVarAxis> axes;
    for (int i = 0; i < axis_count; ++i) {
        auto name = std::to_string(i);
        OTVarAxis axis;
        axis.tag = name;
        axes[name] = axis;
    }
    build_ui(axes);
    auto h = measureHeight();
    build_ui({});
    return h;
}

} // namespace Linea::UI
