// SPDX-License-Identifier: GPL-2.0-or-later
//
// Reusable readers and appliers referenced by the property tables (*.def).
//
// Most readers are: SPItem*                    -> std::optional<T>
// An applier is:    (const EditTarget&, const T&) -> void   (invoked once per target;
//                iteration, undo and echo suppression are owned by Editor)
//
// Three tiers, most properties should need only the first two:
//   1. generic helpers (fn_get, style_get, css_set) parameterized in the table
//   2. shared parameterized helpers for property families (rect_radius_set)
//   3. named free functions for genuinely irregular properties (bottom of file)

#ifndef LINEA_PROPS_ACCESSORS_H
#define LINEA_PROPS_ACCESSORS_H

#include <algorithm>
#include <cstddef>
#include <numbers>
#include <optional>
#include <string>
#include <utility>

#include <glibmm/ustring.h>

#include "colors/color.h"
#include "object/sp-ellipse.h"
#include "object/sp-item.h"
#include "object/sp-rect.h"
#include "props/edit-target.h"
#include "style.h"
#include "util/paint-item-ops.h"  // set_item_style
#include "util/style-utils.h"     // make_css, PaintProp, StrokeWidthProp
#include "util/text-utils.h"      // DecorationThicknessState, is_textual_item

namespace Linea::Props {

// Value + unit pair for properties like font-size and line-height that carry
// both. The unit is an SP_CSS_UNIT_* constant.
struct UnitValue {
    double value = 0.0;
    int unit = 0;
    bool operator == (const UnitValue&) const = default;
};

// Compile-time string, usable as a template parameter (css_set<"opacity">).
template <std::size_t N>
struct FixedString {
    char value[N]{};
    constexpr FixedString(const char (&s)[N]) { std::copy_n(s, N, value); }
    constexpr const char* c_str() const { return value; }
};

// ---------------------------------------------------------------------------
// Tier 1: generic helpers
// ---------------------------------------------------------------------------

// Deduce the argument type of a single-argument member function.
template <typename> struct setter_arg;
template <typename O, typename A> struct setter_arg<void (O::*)(A)> {
    using type = std::remove_cvref_t<A>;
};
template <auto S> using setter_arg_t = typename setter_arg<decltype(S)>::type;

// Read via a const member function of a concrete item type:
//   fn_get<SPRect, &SPRect::getVisibleRx>
template <typename Obj, auto Getter>
auto fn_get(SPItem* item) -> std::optional<decltype((std::declval<const Obj&>().*Getter)())> {
    auto obj = cast<Obj>(item);
    if (!obj) return std::nullopt;
    return (obj->*Getter)();
}

// Write via a member function of a concrete item type:
//   fn_set<SPRect, &SPRect::setVisibleRx>
template <typename Obj, auto Setter>
void fn_set(const EditTarget& target, const setter_arg_t<Setter>& value) {
    if (auto obj = cast<Obj>(target.item())) (obj->*Setter)(value);
}

// Read a value from SPStyle with a projection (any item type that has style):
//   style_get<[](const SPStyle& s) { return static_cast<double>(s.opacity); }>
template <auto Projection>
auto style_get(SPItem* item) -> std::optional<decltype(Projection(std::declval<const SPStyle&>()))> {
    if (!item || !item->style) return std::nullopt;
    return Projection(*item->style);
}

// Default CSS value formatting; irregular values pass a formatter explicitly.
std::string to_css(double v);
std::string to_css(int v);
inline std::string to_css(const std::string& v) { return v; }
inline std::string to_css(bool v) { return v ? "true" : "false"; }

// Write one CSS property through the shared transform-aware path:
//   css_set<"opacity">                       — uses to_css overload
//   css_set<"mix-blend-mode", format_blend_mode>
template <FixedString Name, auto Format = nullptr>
void css_set(const EditTarget& target, const auto& value) {
    auto css = make_css();
    if constexpr (Format != nullptr) {
        sp_repr_css_set_property(css.get(), Name.c_str(), Format(value).c_str());
    } else {
        sp_repr_css_set_property(css.get(), Name.c_str(), to_css(value).c_str());
    }
    target.applyCss(css.get());
}

// ---------------------------------------------------------------------------
// Tier 2: shared helpers for property families
// ---------------------------------------------------------------------------

// Rect corner radius: zero removes the attribute instead of writing "0".
// Shared by rect_rx and rect_ry rows; only the setter/attribute differ.
template <auto Setter, FixedString Attr>
void rect_radius_set(const EditTarget& target, const double& value) {
    auto rect = cast<SPRect>(target.item());
    if (!rect) return;
    if (value > 0) {
        (rect->*Setter)(value);
    } else {
        rect->removeAttribute(Attr.c_str());
    }
}

// Ellipse radius: only positive values are applied (zero would degenerate).
template <auto Setter>
void ellipse_radius_set(const EditTarget& target, const double& value) {
    auto ellipse = cast<SPGenericEllipse>(target.item());
    if (ellipse && value > 0) (ellipse->*Setter)(value);
}

// Ellipse arc angles: exposed in degrees, stored in radians.
// Shared by ellipse_start_angle and ellipse_end_angle rows.
inline constexpr double DEG_PER_RAD = 180.0 / std::numbers::pi;

template <double SPGenericEllipse::* Member>
std::optional<double> ellipse_angle_get(SPItem* item) {
    auto ellipse = cast<SPGenericEllipse>(item);
    if (!ellipse) return std::nullopt;
    return ellipse->*Member * DEG_PER_RAD;
}

template <double SPGenericEllipse::* Member>
void ellipse_angle_set(const EditTarget& target, const double& degrees) {
    auto ellipse = cast<SPGenericEllipse>(target.item());
    if (!ellipse) return;
    ellipse->*Member = degrees / DEG_PER_RAD;
    ellipse->normalize();
    ellipse->updateRepr();
    ellipse->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

// ---------------------------------------------------------------------------
// Tier 3: named accessors for irregular properties (implemented in accessors.cpp;
// bodies move from style-utils.cpp / style-panel.cpp handlers)
// ---------------------------------------------------------------------------

std::string format_blend_mode(SPBlendMode mode);
std::string format_fill_rule(SPWindRule rule);
std::string format_paint_order(const SPIPaintOrder& order);

std::optional<int> read_stroke_linecap(SPItem* item);
void apply_stroke_linecap(const EditTarget& target, const int& cap);

std::optional<int> read_stroke_linejoin(SPItem* item);
void apply_stroke_linejoin(const EditTarget& target, const int& join);

std::optional<StrokeWidthProp> read_stroke_width(SPItem* item);
std::optional<StrokeDashProp> read_stroke_dash(SPItem* item);

std::optional<SPIPaintOrder> read_paint_order(SPItem* item);

std::optional<bool> read_visibility(SPItem* item);
void apply_visibility(const EditTarget& target, const bool& hidden);

// Rect corners are "rounded" when either radius is set or a fillet/chamfer
// path effect is present.
std::optional<bool> read_rect_round_corners(SPItem* item);

// Ellipse arc mode: 0=whole, 1=slice, 2=arc, 3=chord
// (matches the EllipseWidget mode button order).
std::optional<int> read_ellipse_mode(SPItem* item);
void apply_ellipse_mode(const EditTarget& target, const int& mode);

std::optional<PaintProp> read_fill_paint(SPItem* item);    // classify_paint(style->fill, ...)
std::optional<PaintProp> read_stroke_paint(SPItem* item);

// Image properties.
using ImageDimensions = std::pair<int, int>;

std::optional<double> read_image_dpi(SPItem* item);
void apply_image_dpi(const EditTarget& target, const double& dpi);

std::optional<int> read_image_preserve_aspect(SPItem* item);
void apply_image_preserve_aspect(const EditTarget& target, const int& preserve);

std::optional<bool> read_image_embedded(SPItem* item);
std::optional<bool> read_image_linked(SPItem* item);
std::optional<ImageDimensions> read_image_dimensions(SPItem* item);
std::optional<std::string> read_image_color_space(SPItem* item);
std::optional<bool> read_image_missing(SPItem* item);

std::optional<std::string> read_image_url(SPItem* item);
void apply_image_url(const EditTarget& target, const std::string& url);
std::optional<std::size_t> read_image_data_size(SPItem* item);

// Stable identity for the loaded pixbuf (raw pointer). Changes when the
// image's pixbuf is swapped, even if the URL is unchanged — drives preview refresh.
std::optional<const void*> read_image_pixbuf(SPItem* item);

std::string format_image_rendering(int mode);

// Star properties.
std::optional<double> read_star_sides(SPItem* item);
void apply_star_sides(const EditTarget& target, const double& sides);

std::optional<int> read_star_flatsided(SPItem* item);
void apply_star_flatsided(const EditTarget& target, const int& flat);

std::optional<double> read_star_spoke_ratio(SPItem* item);
void apply_star_spoke_ratio(const EditTarget& target, const double& ratio);

std::optional<double> read_star_rounded(SPItem* item);
void apply_star_rounded(const EditTarget& target, const double& rounded);

std::optional<double> read_star_randomized(SPItem* item);
void apply_star_randomized(const EditTarget& target, const double& randomized);

// --- Generic element properties (title, description, locked) -------------
std::optional<std::string> read_id(SPObject* object);

std::optional<std::string> read_title(SPItem* item);
void apply_title(const EditTarget& target, const std::string& title);

std::optional<std::string> read_description(SPItem* item);
void apply_description(const EditTarget& target, const std::string& description);

std::optional<bool> read_locked(SPItem* item);
void apply_locked(const EditTarget& target, const bool& locked);

// ---------------------------------------------------------------------------
// Typography properties (tier 3: named accessors)
//
// Readers return nullopt for non-textual items; merge_item is therefore safe
// to call on any selection. Appliers write CSS through EditTarget::applyCss,
// which routes to sp_te_apply_style for text-range targets (splitting tspans
// to match the selection) and to Util::set_item_style for plain items.
// ---------------------------------------------------------------------------

// Type alias: std::optional<Colors::Color> would break the X-macro comma split.
using DecorationColor = std::optional<Colors::Color>;

std::optional<UnitValue> read_font_size(SPItem* item);
void apply_font_size(const EditTarget& target, const UnitValue& px);

std::optional<UnitValue> read_line_height(SPItem* item);
void apply_line_height(const EditTarget& target, const UnitValue& lh);

std::optional<double> read_letter_spacing(SPItem* item);
void apply_letter_spacing(const EditTarget& target, const double& value);

std::optional<double> read_word_spacing(SPItem* item);
void apply_word_spacing(const EditTarget& target, const double& value);

std::optional<Glib::ustring> read_font_family(SPItem* item);
void apply_font_family(const EditTarget& target, const Glib::ustring& family);

std::optional<Glib::ustring> read_font_style(SPItem* item);
void apply_font_style(const EditTarget& target, const Glib::ustring& style);

std::optional<SPIFontVariationSettings> read_font_variation(SPItem* item);
void apply_font_variation(const EditTarget& target, const SPIFontVariationSettings& variations);

std::optional<int> read_text_align(SPItem* item);
void apply_text_align(const EditTarget& target, const int& index);

std::optional<int> read_writing_mode(SPItem* item);
void apply_writing_mode(const EditTarget& target, const int& mode);

std::optional<int> read_direction(SPItem* item);
void apply_direction(const EditTarget& target, const int& direction);

std::optional<int> read_text_orientation(SPItem* item);
void apply_text_orientation(const EditTarget& target, const int& orientation);

std::optional<bool> read_superscript(SPItem* item);
void apply_superscript(const EditTarget& target, const bool& on);

std::optional<bool> read_subscript(SPItem* item);
void apply_subscript(const EditTarget& target, const bool& on);

std::optional<bool> read_underline(SPItem* item);
void apply_underline(const EditTarget& target, const bool& on);

std::optional<bool> read_overline(SPItem* item);
void apply_overline(const EditTarget& target, const bool& on);

std::optional<bool> read_strikethrough(SPItem* item);
void apply_strikethrough(const EditTarget& target, const bool& on);

std::optional<bool> read_decoration_spelling_error(SPItem* item);
void apply_decoration_spelling_error(const EditTarget& target, const bool& on);

std::optional<int> read_decoration_style(SPItem* item);
void apply_decoration_style(const EditTarget& target, const int& style);

std::optional<DecorationColor> read_decoration_color(SPItem* item);
void apply_decoration_color(const EditTarget& target, const DecorationColor& color);

std::optional<DecorationThicknessState> read_decoration_thickness(SPItem* item);
void apply_decoration_thickness(const EditTarget& target, const DecorationThicknessState& thickness);

} // namespace Linea::Props

#endif // LINEA_PROPS_ACCESSORS_H
