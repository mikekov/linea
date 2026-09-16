// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

#include "props/accessors.h"
#include "props/selection-state.h"

#include "display/cairo-utils.h"
#include "live_effects/effect-enum.h"
#include "live_effects/effect.h"
#include "object/sp-ellipse.h"
#include "object/sp-flowtext.h"
#include "enums.h"
#include "object/sp-image.h"
#include "object/sp-item-group.h"
#include "object/sp-root.h"
#include "object/sp-line.h"
#include "object/sp-path.h"
#include "object/sp-page.h"
#include "object/sp-star.h"
#include "object/sp-text.h"
#include "util/font-discovery.h"
#include "util/numeric/converters.h"
#include "util/text-utils.h"
#include "util-string/ustring-format.h"
#include "svg/css-ostringstream.h"
#include "xml/href-attribute-helper.h"

#include <pangomm/fontdescription.h>

namespace Linea::Props {

std::string to_css(double v) {
    return Inkscape::Util::format_number(v, 6);
}

std::string to_css(int v) {
    return std::to_string(v);
}

namespace {

// Find the CSS keyword for an enum value in one of the style-enums.h tables.
template <std::size_t N>
const char* enum_key(const SPStyleEnum (&table)[N], int value) {
    for (const auto& e : table) {
        if (e.value == value) return e.key;
    }
    return nullptr;
}

bool is_data_uri(const char* href) {
    return href && std::strncmp(href, "data:", 5) == 0;
}

void css_set_keyword(const EditTarget& target, const char* property, const char* keyword) {
    if (!keyword) return;
    auto css = make_css();
    sp_repr_css_set_property(css.get(), property, keyword);
    target.applyCss(css.get());
}

} // namespace

std::string format_blend_mode(SPBlendMode mode) {
    auto key = enum_key(enum_blend_mode, mode);
    return key ? key : "normal";
}

std::string format_fill_rule(SPWindRule rule) {
    auto key = enum_key(enum_fill_rule, rule);
    return key ? key : "nonzero";
}

std::string format_paint_order(const SPIPaintOrder& order) {
    auto val = order.get_value();
    return val.empty() ? "normal" : std::string(val.c_str());
}

std::optional<int> read_stroke_linecap(SPItem* item) {
    if (!item || !item->style) return std::nullopt;
    return static_cast<int>(item->style->stroke_linecap.value);
}

void apply_stroke_linecap(const EditTarget& target, const int& cap) {
    css_set_keyword(target, "stroke-linecap", enum_key(enum_stroke_linecap, cap));
}

std::optional<int> read_stroke_linejoin(SPItem* item) {
    if (!item || !item->style) return std::nullopt;
    return static_cast<int>(item->style->stroke_linejoin.value);
}

void apply_stroke_linejoin(const EditTarget& target, const int& join) {
    css_set_keyword(target, "stroke-linejoin", enum_key(enum_stroke_linejoin, join));
}

std::optional<StrokeWidthProp> read_stroke_width(SPItem* item) {
    if (!item || !item->style) return std::nullopt;
    auto style = item->style;
    bool hairline = style->stroke_extensions.hairline;
    // Multiply by the item-to-desktop transform scale, matching the GTK
    // query path in desktop-style.cpp — the UI shows the on-screen width.
    double width = hairline ? 0.0 : style->stroke_width.computed * item->i2dt_affine().descrim();
    return StrokeWidthProp{width, hairline};
}

std::optional<StrokeDashProp> read_stroke_dash(SPItem* item) {
    if (!item || !item->style) return std::nullopt;
    StrokeDashProp sd;
    auto [pattern, offset] = getDashFromStyle(item->style);
    sd.dashes = std::move(pattern);
    sd.offset = offset;
    return sd;
}

std::optional<SPIPaintOrder> read_paint_order(SPItem* item) {
    if (!item || !item->style) return std::nullopt;
    return item->style->paint_order;
}

std::optional<bool> read_visibility(SPItem* item) {
    if (!item) return std::nullopt;
    return item->isExplicitlyHidden();
}

void apply_visibility(const EditTarget& target, const bool& hidden) {
    target.item()->setExplicitlyHidden(hidden);
}

std::optional<int> read_ellipse_mode(SPItem* item) {
    auto ellipse = cast<SPGenericEllipse>(item);
    if (!ellipse) return std::nullopt;

    if (ellipse->is_whole()) return 0;
    switch (ellipse->arc_type) {
        case SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE: return 1;
        case SP_GENERIC_ELLIPSE_ARC_TYPE_ARC:   return 2;
        case SP_GENERIC_ELLIPSE_ARC_TYPE_CHORD: return 3;
    }
    return 0;
}

void apply_ellipse_mode(const EditTarget& target, const int& mode) {
    auto ellipse = cast<SPGenericEllipse>(target.item());
    if (!ellipse) return;

    if (mode == 0) {
        // Whole: reset angles; updateRepr clears the arc attributes.
        ellipse->start = ellipse->end = 0;
        ellipse->normalize();
    } else {
        if (ellipse->is_whole()) {
            // Transitioning from whole to a slice — set initial angles.
            ellipse->start = 30.0 / DEG_PER_RAD;
            ellipse->end = -30.0 / DEG_PER_RAD;
            ellipse->normalize();
        }
        switch (mode) {
            case 1: ellipse->arc_type = SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE; break;
            case 2: ellipse->arc_type = SP_GENERIC_ELLIPSE_ARC_TYPE_ARC;   break;
            case 3: ellipse->arc_type = SP_GENERIC_ELLIPSE_ARC_TYPE_CHORD; break;
        }
    }
    ellipse->updateRepr();
    ellipse->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

std::optional<bool> read_rect_round_corners(SPItem* item) {
    auto rect = cast<SPRect>(item);
    if (!rect) return std::nullopt;

    bool lpe = rect->getFirstPathEffectOfType(Inkscape::LivePathEffect::FILLET_CHAMFER) != nullptr;
    return rect->rx.value > 0 || rect->ry.value > 0 || lpe;
}

std::optional<PaintProp> read_fill_paint(SPItem* item) {
    if (!item || !item->style) return std::nullopt;
    return classify_paint(item->style->fill, item->style->fill_opacity.as_double());
}

std::optional<PaintProp> read_stroke_paint(SPItem* item) {
    if (!item || !item->style) return std::nullopt;
    return classify_paint(item->style->stroke, item->style->stroke_opacity.as_double());
}

// --- Image -------------------------------------------------------------------

std::optional<double> read_image_dpi(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image || !image->getRepr()) return std::nullopt;
    return image->getRepr()->getAttributeDouble("inkscape:svg-dpi", 96);
}

void apply_image_dpi(const EditTarget& target, const double& dpi) {
    auto image = cast<SPImage>(target.item());
    if (!image) return;
    image->setAttribute("inkscape:svg-dpi", Inkscape::ustring::format_classic(dpi));
}

std::optional<int> read_image_preserve_aspect(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image) return std::nullopt;

    bool none = false;
    if (image->aspect_set) {
        none = image->aspect_align == SP_ASPECT_NONE;
    }
    return none ? 0 : 1;
}

void apply_image_preserve_aspect(const EditTarget& target, const int& preserve) {
    auto image = cast<SPImage>(target.item());
    if (!image) return;
    image->setAttribute("preserveAspectRatio", preserve ? "xMidYMid" : "none");
}

std::optional<bool> read_image_embedded(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image || !image->getRepr()) return std::nullopt;
    auto href = Inkscape::getHrefAttribute(*image->getRepr()).second;
    return is_data_uri(href);
}

std::optional<bool> read_image_linked(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image || !image->getRepr()) return std::nullopt;
    auto href = Inkscape::getHrefAttribute(*image->getRepr()).second;
    return href && *href && !is_data_uri(href);
}

std::optional<ImageDimensions> read_image_dimensions(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image || !image->pixbuf) return std::nullopt;
    Inkscape::Pixbuf const* pixbuf = image->pixbuf.get();
    return ImageDimensions{pixbuf->width(), pixbuf->height()};
}

std::optional<std::string> read_image_color_space(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image) return std::nullopt;
    return std::string(image->color_profile ? image->color_profile : "");
}

std::optional<bool> read_image_missing(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image) return std::nullopt;
    return image->missing;
}

std::optional<std::string> read_image_url(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image || !image->getRepr()) return std::nullopt;

    auto href = Inkscape::getHrefAttribute(*image->getRepr()).second;
    if (href && !is_data_uri(href)) return std::string(href);
    return std::string();
}

void apply_image_url(const EditTarget& target, const std::string& url) {
    auto image = cast<SPImage>(target.item());
    if (!image || !image->getRepr() || url.empty()) return;
    Inkscape::setHrefAttribute(*image->getRepr(), url.c_str());
}

std::optional<std::size_t> read_image_data_size(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image || !image->getRepr()) return std::nullopt;
    auto href = Inkscape::getHrefAttribute(*image->getRepr()).second;
    if (!href || !is_data_uri(href)) return std::nullopt;
    return std::strlen(href);
}

std::optional<const void*> read_image_pixbuf(SPItem* item) {
    auto image = cast<SPImage>(item);
    if (!image || !image->pixbuf) return std::nullopt;
    return static_cast<const void*>(image->pixbuf.get());
}

std::string format_image_rendering(int mode) {
    auto key = enum_key(enum_image_rendering, mode);
    return key ? key : "auto";
}

// --- Star properties -------------------------------------------------------

std::optional<double> read_star_sides(SPItem* item) {
    auto star = cast<SPStar>(item);
    if (!star) return std::nullopt;
    return static_cast<double>(star->sides);
}

void apply_star_sides(const EditTarget& target, const double& sides) {
    auto star = cast<SPStar>(target.item());
    if (!star) return;

    int s = std::clamp(static_cast<int>(std::lround(sides)),
                       star->flatsided ? 3 : 2, 1024);
    star->sides = s;
    star->arg[1] = star->arg[0] + std::numbers::pi / s;
    star->updateRepr();
    star->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

std::optional<int> read_star_flatsided(SPItem* item) {
    auto star = cast<SPStar>(item);
    if (!star) return std::nullopt;
    return star->flatsided ? 1 : 0;
}

void apply_star_flatsided(const EditTarget& target, const int& flat) {
    auto star = cast<SPStar>(target.item());
    if (!star) return;

    star->flatsided = (flat != 0);
    if (star->flatsided && star->sides < 3) {
        star->sides = 3;
        star->arg[1] = star->arg[0] + std::numbers::pi / 3;
    }
    star->updateRepr();
    star->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

std::optional<double> read_star_spoke_ratio(SPItem* item) {
    auto star = cast<SPStar>(item);
    if (!star) return std::nullopt;

    double r1 = star->r[0];
    double r2 = star->r[1];
    double ratio = 0.5;
    if (r2 < r1) {
        ratio = r1 > 0.0 ? r2 / r1 : 0.5;
    } else {
        ratio = r2 > 0.0 ? r1 / r2 : 0.5;
    }
    return ratio * 100.0;
}

void apply_star_spoke_ratio(const EditTarget& target, const double& percent) {
    auto star = cast<SPStar>(target.item());
    if (!star) return;

    double ratio = percent / 100.0;
    if (star->r[1] < star->r[0]) {
        if (star->r[0] > 0.0) {
            star->r[1] = star->r[0] * ratio;
        }
    } else {
        if (star->r[1] > 0.0) {
            star->r[0] = star->r[1] * ratio;
        }
    }
    star->updateRepr();
    star->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

std::optional<double> read_star_rounded(SPItem* item) {
    auto star = cast<SPStar>(item);
    if (!star) return std::nullopt;
    return star->rounded;
}

void apply_star_rounded(const EditTarget& target, const double& rounded) {
    auto star = cast<SPStar>(target.item());
    if (!star) return;
    star->rounded = rounded;
    star->updateRepr();
    star->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

std::optional<double> read_star_randomized(SPItem* item) {
    auto star = cast<SPStar>(item);
    if (!star) return std::nullopt;
    return star->randomized;
}

void apply_star_randomized(const EditTarget& target, const double& randomized) {
    auto star = cast<SPStar>(target.item());
    if (!star) return;
    star->randomized = randomized;
    star->updateRepr();
    star->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}


// --- Selection composition (moves from element-properties.cpp pattern) -------

void merge_counts(Counts& counts, SPObject* item) {
    if (!item) return;

    counts.items++;

    if (is<SPRect>(item)) {
        counts.rectangles++;
    } else if (is<SPGenericEllipse>(item)) {
        counts.ellipses++;
    } else if (auto star = cast<SPStar>(item)) {
        if (star->flatsided) {
            counts.polygons++;
        } else {
            counts.stars++;
        }
    } else if (is<SPImage>(item)) {
        counts.images++;
    } else if (is<SPPath>(item)) {
        counts.paths++;
    } else if (is<SPLine>(item)) {
        counts.lines++;
    } else if (is<SPPage>(item)) {
        counts.pages++;
    } else if (is<SPRoot>(item)) {
        counts.svgs++;
    } else if (auto group = cast<SPGroup>(item)) {
        if (group->isLayer()) {
            counts.layers++;
        } else {
            counts.groups++;
        }
    } else {
        if (is_textual_item(item)) {
            counts.textual++;
        }
        if (is<SPFlowtext>(item)) {
            counts.flowtext++;
        }
    }
}

// --- Generic element properties (title, description, locked) -------------

std::optional<std::string> read_id(SPObject* object) {
    if (!object) return std::nullopt;
    auto id = object->getId();
    return id ? std::optional<std::string>(id) : std::nullopt;
}

std::optional<std::string> read_title(SPItem* item) {
    if (!item) return std::nullopt;
    auto t = item->title();
    std::string result = t ? std::string(t) : std::string();
    if (t) g_free(t);
    return result;
}

void apply_title(const EditTarget& target, const std::string& title) {
    target.item()->setTitle(title.empty() ? nullptr : title.c_str());
}

std::optional<std::string> read_description(SPItem* item) {
    if (!item) return std::nullopt;
    auto d = item->desc();
    std::string result = d ? std::string(d) : std::string();
    if (d) g_free(d);
    return result;
}

void apply_description(const EditTarget& target, const std::string& description) {
    target.item()->setDesc(description.empty() ? nullptr : description.c_str());
}

std::optional<bool> read_locked(SPItem* item) {
    if (!item) return std::nullopt;
    return !item->isSensitive();
}

void apply_locked(const EditTarget& target, const bool& locked) {
    target.item()->setLocked(locked);
}

// ===========================================================================
// Typography readers and appliers
// ===========================================================================

namespace {

// Shared precondition: item must be a textual element with a style.
// Returns the style pointer, or nullptr if the item is not textual.
SPStyle* text_style(SPItem* item) {
    if (!item || !Linea::is_textual_item(item)) return nullptr;
    return item->style;
}

// CSS and Pango swap italic/oblique enum values:
// CSS: NORMAL=0, ITALIC=1, OBLIQUE=2; Pango: NORMAL=0, OBLIQUE=1, ITALIC=2
const Pango::Style css_to_pango_style[] = {
    Pango::Style::NORMAL, Pango::Style::ITALIC, Pango::Style::OBLIQUE
};

Glib::ustring style_font_family(SPStyle* style) {
    return style->font_family.value()
        ? Glib::ustring(style->font_family.value())
        : Glib::ustring();
}

// The item's current font identity (family, weight, style, stretch) as a
// Pango description; used to read the face style and to match faces when
// the family changes.
Pango::FontDescription description_from_style(SPStyle* style) {
    auto family = style_font_family(style);
    Pango::FontDescription desc;
    if (family.size()) desc.set_family(family.raw());
    auto fs = style->font_style.computed;
    desc.set_style(fs < 3 ? css_to_pango_style[fs] : Pango::Style::NORMAL);
    desc.set_weight(static_cast<Pango::Weight>(style->font_weight.computed));
    desc.set_stretch(static_cast<Pango::Stretch>(style->font_stretch.computed));
    return desc;
}

// Write the full font-identity CSS blob (family, weight, style, stretch,
// variant, variation settings, fontspec) resolved from a concrete font face.
// `variations` is a Pango-style axis string with the fontspec "@" prefix
// (e.g. "@wght=700,wdth=100"), or empty for default axes.
void set_font_css(const EditTarget& target, const Inkscape::FontInfo& font, const Glib::ustring& variations = {}) {
    auto css = make_css();
    auto desc = Inkscape::get_font_description(font.ff, font.face);
    if (!variations.empty()) desc.set_variations(variations);
    auto fontspec = Inkscape::get_inkscape_fontspec(font.ff, font.face, variations);
    Linea::fill_css_from_font_description(css.get(), font.ff->get_name(), desc, fontspec);
    target.applyCss(css.get());
}

// Build the space-separated text-decoration-line CSS value from individual flags.
std::string decoration_line_css(bool underline, bool overline, bool strikethrough,
                                bool spelling_error) {
    if (!underline && !overline && !strikethrough && !spelling_error) return "none";
    std::string decor;
    if (spelling_error)   decor += "spelling-error ";
    if (underline)        decor += "underline ";
    if (overline)         decor += "overline ";
    if (strikethrough)    decor += "line-through ";
    decor.pop_back(); // trailing space
    return decor;
}

} // namespace

// --- font size & spacing --------------------------------------------------

std::optional<UnitValue> read_font_size(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    // Font-size is always stored as px in SVG; the computed value is in px.
    // Multiply by the item-to-desktop transform scale to show the on-screen size,
    // matching the GTK query path in objects_query_fontnumbers.
    double scale = item->i2dt_affine().descrim();
    return UnitValue{style->font_size.computed * scale, SP_CSS_UNIT_PX};
}

void apply_font_size(const EditTarget& target, const UnitValue& fs) {
    if (!text_style(target.item())) return;
    auto css = make_css();
    Inkscape::CSSOStringStream os;
    os << fs.value << sp_style_get_css_unit_string(fs.unit);
    sp_repr_css_set_property(css.get(), "font-size", os.str().c_str());
    target.applyCss(css.get());
}

std::optional<UnitValue> read_line_height(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    auto& lh = style->line_height;
    if (lh.normal) {
        return UnitValue{-1, SP_CSS_UNIT_NONE};  // sentinel for "normal"
    }
    // Relative units (none, %, em, ex) are transform-independent; absolute
    // units are scaled by the item-to-desktop transform, matching
    // objects_query_fontnumbers in desktop-style.cpp.
    bool relative = (lh.unit == SP_CSS_UNIT_NONE || lh.unit == SP_CSS_UNIT_PERCENT ||
                     lh.unit == SP_CSS_UNIT_EM || lh.unit == SP_CSS_UNIT_EX);
    double scale = relative ? 1.0 : item->i2dt_affine().descrim();
    return UnitValue{lh.value * scale, lh.unit};
}

void apply_line_height(const EditTarget& target, const UnitValue& lh) {
    if (!text_style(target.item())) return;
    auto css = make_css();
    if (lh.value < 0) {
        // sentinel for "normal"
        sp_repr_css_set_property(css.get(), "line-height", "normal");
    } else {
        // SPILength stores percent as a fraction (1.5 = 150%); multiply
        // by 100 for CSS output. Relative units (em, ex, %) use the raw
        // value + suffix; absolute units are converted to px for SVG
        // (matching the text toolbar's behavior).
        double value = lh.value;
        Inkscape::CSSOStringStream os;
        if (lh.unit == SP_CSS_UNIT_PERCENT) {
            value *= 100.0;
            os << value << "%";
        } else if (lh.unit == SP_CSS_UNIT_NONE) {
            os << value;
        } else if (lh.unit == SP_CSS_UNIT_EM || lh.unit == SP_CSS_UNIT_EX) {
            os << value << sp_style_get_css_unit_string(lh.unit);
        } else {
            // Absolute unit — convert to px for SVG storage
            os << Inkscape::Util::Quantity::convert(value, sp_style_get_css_unit_string(lh.unit), "px") << "px";
        }
        sp_repr_css_set_property(css.get(), "line-height", os.str().c_str());
    }
    target.applyCss(css.get());
}

std::optional<double> read_letter_spacing(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    if (style->letter_spacing.normal) return 0.0;
    // Scale by item-to-desktop transform, matching objects_query_fontnumbers.
    double scale = item->i2dt_affine().descrim();
    return style->letter_spacing.computed * scale;
}

void apply_letter_spacing(const EditTarget& target, const double& value) {
    if (!text_style(target.item())) return;
    auto css = make_css();
    sp_repr_css_set_property_double(css.get(), "letter-spacing", value);
    target.applyCss(css.get());
}

std::optional<double> read_word_spacing(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    if (style->word_spacing.normal) return 0.0;
    // Scale by item-to-desktop transform, matching objects_query_fontnumbers.
    double scale = item->i2dt_affine().descrim();
    return style->word_spacing.computed * scale;
}

void apply_word_spacing(const EditTarget& target, const double& value) {
    if (!text_style(target.item())) return;
    auto css = make_css();
    sp_repr_css_set_property_double(css.get(), "word-spacing", value);
    target.applyCss(css.get());
}

// --- font identity --------------------------------------------------------

std::optional<Glib::ustring> read_font_family(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    return style->font_family.value()
        ? Glib::ustring(style->font_family.value())
        : Glib::ustring();
}

void apply_font_family(const EditTarget& target, const Glib::ustring& family) {
    auto style = text_style(target.item());
    if (!style) return;

    // Resolve the family against the font database and preserve this item's
    // current weight/style/stretch by picking the closest face in the new
    // family, so family and style can be changed independently.
    auto fonts = Inkscape::FontDiscovery::get().fonts();
    if (auto fam = fonts ? Inkscape::find_font_family(*fonts, family) : nullptr) {
        if (auto font = Inkscape::find_closest_face(*fam, description_from_style(style))) {
            set_font_css(target, *font);
            return;
        }
    }
    // Unknown family (generic name or a font that is not installed):
    // write it as-is with default traits.
    auto css = make_css();
    auto desc = Pango::FontDescription(family);
    Linea::fill_css_from_font_description(css.get(), family, desc, {});
    target.applyCss(css.get());
}

std::optional<Glib::ustring> read_font_style(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    return get_face_style(description_from_style(style));
}

void apply_font_style(const EditTarget& target, const Glib::ustring& style_name) {
    auto style = text_style(target.item());
    if (!style) return;

    // Resolve (this item's family, style_name) against the font database and
    // write the full CSS blob. Each item in a mixed-family selection resolves
    // the face against its own family.
    auto family = style_font_family(style);
    auto fonts = Inkscape::FontDiscovery::get().fonts();
    if (auto fam = fonts ? Inkscape::find_font_family(*fonts, family) : nullptr) {
        if (auto font = Inkscape::find_font_face(*fam, style_name)) {
            set_font_css(target, *font);
            return;
        }
    }
    // Family or face unknown: fall back to a bold/italic heuristic and drop
    // the properties that cannot be resolved without a concrete face.
    auto css = make_css();
    bool bold = style_name.find("Bold") != Glib::ustring::npos;
    bool italic = style_name.find("Italic") != Glib::ustring::npos;
    sp_repr_css_set_property(css.get(), "font-weight", bold ? "bold" : "normal");
    sp_repr_css_set_property(css.get(), "font-style", italic ? "italic" : "normal");
    sp_repr_css_unset_property(css.get(), "-inkscape-font-specification");
    sp_repr_css_unset_property(css.get(), "font-variation-settings");
    sp_repr_css_unset_property(css.get(), "font-stretch");
    sp_repr_css_unset_property(css.get(), "font-variant");
    target.applyCss(css.get());
}

std::optional<SPIFontVariationSettings> read_font_variation(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    return style->font_variation_settings;
}

void apply_font_variation(const EditTarget& target, const SPIFontVariationSettings& variations) {
    auto style = text_style(target.item());
    if (!style) return;

    // Re-resolve the item's current font so the fontspec carries the new
    // axis values alongside the family/face (matching the format written by
    // the family/style appliers). toString() yields the Pango axis string;
    // the fontspec convention prefixes it with '@'.
    auto vars = variations.axes.empty() ? Glib::ustring() : "@" + variations.toString();
    auto fonts = Inkscape::FontDiscovery::get().fonts();
    if (auto fam = fonts ? Inkscape::find_font_family(*fonts, style_font_family(style)) : nullptr) {
        if (auto font = Inkscape::find_closest_face(*fam, description_from_style(style))) {
            set_font_css(target, *font, vars);
            return;
        }
    }
    // Unknown family: write the variation settings alone.
    auto css = make_css();
    sp_repr_css_set_property(css.get(), "font-variation-settings", variations.get_value().c_str());
    target.applyCss(css.get());
}

// --- text layout (enums) --------------------------------------------------

std::optional<int> read_text_align(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    bool rtl = (style->direction.computed == SP_CSS_DIRECTION_RTL);
    return Linea::get_text_align_button_index(
        rtl, static_cast<SPCSSTextAlign>(style->text_align.computed));
}

void apply_text_align(const EditTarget& target, const int& index) {
    // Text alignment needs anchor-position adjustment to preserve the
    // visual bounding box; delegate to the existing helper for SPText.
    if (auto text = cast<SPText>(target.item())) {
        Linea::apply_text_alignment(text, index);
    } else if (text_style(target.item())) {
        // For non-SPText textual items (tspans, flowparas), write CSS directly.
        static const char* align_css[] = {"left", "center", "right", "justify"};
        if (index < 0 || index > 3) return;
        auto css = make_css();
        sp_repr_css_set_property(css.get(), "text-align", align_css[index]);
        target.applyCss(css.get());
    }
}

std::optional<int> read_writing_mode(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    // Map the CSS enum to button indices (LR_TB and RL_TB both → 0).
    switch (style->writing_mode.computed) {
        case SP_CSS_WRITING_MODE_LR_TB:
        case SP_CSS_WRITING_MODE_RL_TB: return 0;
        case SP_CSS_WRITING_MODE_TB_RL: return 1;
        case SP_CSS_WRITING_MODE_TB_LR: return 2;
    }
    return std::nullopt;
}

void apply_writing_mode(const EditTarget& target, const int& mode) {
    if (!text_style(target.item())) return;
    // Use SVG 1.1 forms (lr-tb, tb-rl) for compatibility with existing files
    // and the text toolbar. The style parser accepts both old and CSS3 forms.
    static const char* mode_css[] = {"lr-tb", "tb-rl", "vertical-lr"};
    if (mode < 0 || mode > 2) return;
    auto css = make_css();
    sp_repr_css_set_property(css.get(), "writing-mode", mode_css[mode]);
    target.applyCss(css.get());
}

std::optional<int> read_direction(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    return style->direction.computed;
}

void apply_direction(const EditTarget& target, const int& dir) {
    if (!text_style(target.item())) return;
    static const char* dir_css[] = {"ltr", "rtl"};
    if (dir < 0 || dir > 1) return;
    auto css = make_css();
    sp_repr_css_set_property(css.get(), "direction", dir_css[dir]);
    target.applyCss(css.get());
}

std::optional<int> read_text_orientation(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    return style->text_orientation.computed;
}

void apply_text_orientation(const EditTarget& target, const int& orientation) {
    if (!text_style(target.item())) return;
    // The text toolbar writes "auto" for mode 0, but the style enum table
    // only recognizes "mixed" (which is the CSS3 default). "auto" falls
    // through to the default, so "mixed" is the correct value to write.
    static const char* orient_css[] = {"mixed", "upright", "sideways"};
    if (orientation < 0 || orientation > 2) return;
    auto css = make_css();
    sp_repr_css_set_property(css.get(), "text-orientation", orient_css[orientation]);
    target.applyCss(css.get());
}

// --- baseline shift (superscript / subscript) -----------------------------

std::optional<bool> read_superscript(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    return style->baseline_shift.set &&
           style->baseline_shift.type == SP_BASELINE_SHIFT_LITERAL &&
           style->baseline_shift.literal == SP_CSS_BASELINE_SHIFT_SUPER;
}

std::optional<bool> read_subscript(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    return style->baseline_shift.set &&
           style->baseline_shift.type == SP_BASELINE_SHIFT_LITERAL &&
           style->baseline_shift.literal == SP_CSS_BASELINE_SHIFT_SUB;
}

void apply_superscript(const EditTarget& target, const bool& on) {
    if (!text_style(target.item())) return;
    auto css = Linea::apply_text_script(on, false);
    target.applyCss(css.get());
}

void apply_subscript(const EditTarget& target, const bool& on) {
    if (!text_style(target.item())) return;
    auto css = Linea::apply_text_script(false, on);
    target.applyCss(css.get());
}

// --- text decoration lines ------------------------------------------------
// Each decoration toggle writes the combined text-decoration-line value,
// preserving the other flags from the item's current style.

std::optional<bool> read_underline(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    bool v = style->text_decoration_line.underline;
    return v;
}

std::optional<bool> read_overline(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    bool v = style->text_decoration_line.overline;
    return v;
}

std::optional<bool> read_strikethrough(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    bool v = style->text_decoration_line.line_through;
    return v;
}

std::optional<bool> read_decoration_spelling_error(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    bool v = style->text_decoration_line.spelling_error;
    return v;
}

static void apply_decoration_line(const EditTarget& target, bool underline, bool overline,
                                  bool strikethrough, bool spelling_error) {
    if (!text_style(target.item())) return;
    auto css = make_css();
    sp_repr_css_set_property(css.get(), "text-decoration-line",
        decoration_line_css(underline, overline, strikethrough, spelling_error).c_str());
    target.applyCss(css.get());
}

void apply_underline(const EditTarget& target, const bool& on) {
    auto style = text_style(target.item());
    if (!style) return;
    // Clear spelling_error: it's mutually exclusive with line decorations.
    // The style parser would clear underline anyway if spelling_error remained,
    // so the toggle would appear to do nothing.
    apply_decoration_line(target, on, style->text_decoration_line.overline,
                          style->text_decoration_line.line_through, false);
}

void apply_overline(const EditTarget& target, const bool& on) {
    auto style = text_style(target.item());
    if (!style) return;
    apply_decoration_line(target, style->text_decoration_line.underline, on,
                          style->text_decoration_line.line_through, false);
}

void apply_strikethrough(const EditTarget& target, const bool& on) {
    auto style = text_style(target.item());
    if (!style) return;
    apply_decoration_line(target, style->text_decoration_line.underline,
                          style->text_decoration_line.overline, on, false);
}

void apply_decoration_spelling_error(const EditTarget& target, const bool& on) {
    auto style = text_style(target.item());
    if (!style) return;
    // Spelling-error is mutually exclusive with line decorations: clear
    // underline/overline/strikethrough when turning it on.
    apply_decoration_line(target, false, false, false, on);
}

// --- decoration style / color / thickness ---------------------------------

std::optional<int> read_decoration_style(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    int ds = 0; // solid by default
    if (style->text_decoration_style.isdouble) ds = 1;
    else if (style->text_decoration_style.dotted) ds = 2;
    else if (style->text_decoration_style.dashed) ds = 3;
    else if (style->text_decoration_style.wavy) ds = 4;
    return ds;
}

void apply_decoration_style(const EditTarget& target, const int& style_val) {
    if (!text_style(target.item())) return;
    static const char* style_css[] = {"solid", "double", "dotted", "dashed", "wavy"};
    if (style_val < 0 || style_val > 4) return;
    auto css = make_css();
    sp_repr_css_set_property(css.get(), "text-decoration-style", style_css[style_val]);
    target.applyCss(css.get());
}

std::optional<DecorationColor> read_decoration_color(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    auto dc_ptr = &style->text_decoration_color;
    if (!dc_ptr->set && item->parent && item->parent->style) {
        dc_ptr = &item->parent->style->text_decoration_color;
    }
    bool dc_is_set = dc_ptr->set && !dc_ptr->inherit;
    if (!dc_is_set) return std::nullopt;
    return std::optional<Colors::Color>(dc_ptr->getColor());
}

void apply_decoration_color(const EditTarget& target, const DecorationColor& color) {
    if (!text_style(target.item())) return;
    auto css = make_css();
    if (color) {
        auto str = color->toString();
        sp_repr_css_set_property(css.get(), "text-decoration-color", str.c_str());
    } else {
        sp_repr_css_unset_property(css.get(), "text-decoration-color");
    }
    target.applyCss(css.get());
}

std::optional<DecorationThicknessState> read_decoration_thickness(SPItem* item) {
    auto style = text_style(item);
    if (!style) return std::nullopt;
    auto tdt_ptr = &style->text_decoration_thickness;
    if (!tdt_ptr->set && item->parent && item->parent->style) {
        tdt_ptr = &item->parent->style->text_decoration_thickness;
    }
    const auto& tdt = *tdt_ptr;
    if (!tdt.set) return std::nullopt;
    return DecorationThicknessState{tdt.computed, tdt.auto_val, tdt.from_font};
}

void apply_decoration_thickness(const EditTarget& target, const DecorationThicknessState& thickness) {
    if (!text_style(target.item())) return;
    auto css = make_css();
    if (thickness.auto_val) {
        sp_repr_css_set_property(css.get(), "text-decoration-thickness", "auto");
    } else if (thickness.from_font) {
        sp_repr_css_set_property(css.get(), "text-decoration-thickness", "from-font");
    } else {
        sp_repr_css_set_property_double(css.get(), "text-decoration-thickness",
                                        thickness.value);
    }
    target.applyCss(css.get());
}

} // namespace Linea::Props
