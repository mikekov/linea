// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Qt Actions for working with objects without GUI.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "actions-object.h"

#include <array>
#include <giomm.h> // Not <gtkmm.h>! To eventually allow a headless version!

#include "action-registry.h"
#include "actions-helper.h"
#include "actions/action-meta.h"
#include "document-undo.h"
#include "i18n/action-strings.h"
#include "linea-application.h"
#include "live_effects/effect.h"
#include "live_effects/lpe-powerclip.h"
#include "live_effects/lpe-powermask.h"
#include "object/sp-lpe-item.h"
#include "object/sp-star.h"
#include "preferences.h"
#include "selection.h"
#include "trace/potrace/inkscape-potrace.h"
#include "trace/trace.h"
#include "ui/icon-names.h"
#include "util/cast.h"

namespace {

double stod_finite(const std::string& str) {
    const double result = std::stod(str);
    if (!std::isfinite(result)) {
        throw std::out_of_range{"stod: Inf or NaN"};
    }
    return result;
}

void object_trace(const QString& value, Inkscape::Selection* selection) {
    if (!selection || selection->isEmpty()) {
        show_output("action:object_trace: selection empty!", true);
        return;
    }

    const Glib::ustring str = value.toStdString();
    const std::vector<Glib::ustring> settings = Glib::Regex::split_simple(",", str);
    if (settings.size() != 7) {
        show_output("action:object_trace: expected argument format: "
                    "{scans},{smooth[false|true]},{stack[false|true]},{remove_background[false|true],{speckles},{"
                    "smooth_corners},{optimize}}",
                    true);
        return;
    }

    int scans;
    bool smooth;
    bool stack;
    bool remove_background;
    int speckles;
    double smooth_corners;
    double optimize;
    try {
        scans = std::stoi(settings[0]);
        smooth = settings[1] == "true";
        stack = settings[2] == "true";
        remove_background = settings[3] == "true";
        speckles = std::stoi(settings[4]);
        smooth_corners = stod_finite(settings[5]);
        optimize = stod_finite(settings[6]);
    } catch (const std::logic_error& e) {
        show_output(std::string{"action:object_trace: parsing arguments failed: "} + e.what(), true);
        return;
    }

    auto tracer = std::make_unique<Inkscape::Trace::Potrace::PotraceTracingEngine>(
        Inkscape::Trace::Potrace::TraceType::QUANT_COLOR, false, 64, 0.45, 0.0, 0.65, scans, stack, smooth,
        remove_background);
    tracer->setOptiCurve(true);
    tracer->setTurdSize(speckles);
    tracer->setAlphaMax(smooth_corners);
    tracer->setOptTolerance(optimize);

    auto mainloop = Glib::MainLoop::create();

    auto future = Inkscape::Trace::trace(
        std::move(tracer), false,
        [](double progress) { std::cout << "Tracing... " << std::round(100 * progress) << '%' << std::endl; },
        [&] {
            show_output("Tracing done.");
            mainloop->quit();
        });

    if (!future) {
        show_output("Tracing failed.", true);
        return;
    }

    mainloop->run();
}

void object_get_attribute(const QString& value, Inkscape::Selection* selection) {
    if (!selection) return;
    const Glib::ustring attribute = value.toStdString();

    for (auto obj : selection->objects()) {
        Inkscape::XML::Node* repr = obj->getRepr();
        auto val = repr->attribute(attribute.c_str());
        show_output(val ? Glib::strescape(val) : "", false);
    }
}

void object_get_property(const QString& value, Inkscape::Selection* selection) {
    if (!selection) return;
    const Glib::ustring attribute = value.toStdString();

    for (auto obj : selection->objects()) {
        Inkscape::XML::Node* repr = obj->getRepr();
        SPCSSAttr* css = sp_repr_css_attr(repr, "style");
        auto val = sp_repr_css_property(css, attribute.c_str(), "");
        show_output(val ? Glib::strescape(val) : "", false);
        sp_repr_css_attr_unref(css);
    }
}

void object_remove_attribute(const QString& value, Inkscape::Selection* selection) {
    if (!selection || selection->isEmpty()) {
        show_output("action:object_remove_attribute: selection empty!");
        return;
    }
    const Glib::ustring attribute = value.toStdString();

    for (auto obj : selection->objects()) {
        Inkscape::XML::Node* repr = obj->getRepr();
        repr->removeAttribute(attribute);
    }
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Action remove attribute from objects"), "");
}

void object_remove_property(const QString& value, Inkscape::Selection* selection) {
    if (!selection || selection->isEmpty()) {
        show_output("action:object_remove_property: selection empty!");
        return;
    }
    const Glib::ustring property = value.toStdString();

    for (auto obj : selection->objects()) {
        Inkscape::XML::Node* repr = obj->getRepr();
        SPCSSAttr* css = sp_repr_css_attr(repr, "style");
        sp_repr_css_set_property(css, property.c_str(), nullptr);
        sp_repr_css_set(repr, css, "style");
        sp_repr_css_attr_unref(css);
    }
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Action remove property from objects"), "");
}

// No sanity checking is done... should probably add.
void object_set_attribute(const QString& value, Inkscape::Selection* selection) {
    const Glib::ustring argument = value.toStdString();
    const auto comma_position = argument.find_first_of(',');
    if (comma_position == 0 || comma_position == Glib::ustring::npos) {
        show_output("action:object_set_attribute: requires 'attribute name, attribute value'");
        return;
    }
    const auto attribute = argument.substr(0, comma_position);
    const auto new_value = argument.substr(comma_position + 1);

    if (!selection || selection->isEmpty()) {
        show_output("action:object_set_attribute: selection empty!");
        return;
    }

    // Should this be a selection member function?
    for (auto obj : selection->objects()) {
        Inkscape::XML::Node* repr = obj->getRepr();
        repr->setAttribute(attribute, new_value);
    }

    // TODO: Needed to update repr (is this the best way?).
    Inkscape::DocumentUndo::done(selection->document(),
                                 Inkscape::Util::Internal::ContextString("ActionObjectSetAttribute"), "");
}

// No sanity checking is done... should probably add.
void object_set_property(const QString& value, Inkscape::Selection* selection) {
    const Glib::ustring s = value.toStdString();

    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple(",", s);
    if (tokens.size() != 2) {
        show_output("action:object_set_property: requires 'property name, property value'");
        return;
    }

    if (!selection || selection->isEmpty()) {
        show_output("action:object_set_property: selection empty!");
        return;
    }

    // Should this be a selection member function?
    for (auto obj : selection->objects()) {
        Inkscape::XML::Node* repr = obj->getRepr();
        SPCSSAttr* css = sp_repr_css_attr(repr, "style");
        sp_repr_css_set_property(css, tokens[0].c_str(), tokens[1].c_str());
        sp_repr_css_set(repr, css, "style");
        sp_repr_css_attr_unref(css);
    }

    // Needed to update repr (is this the best way?).
    Inkscape::DocumentUndo::done(selection->document(),
                                 Inkscape::Util::Internal::ContextString("ActionObjectSetProperty"), "");
}

void object_unlink_clones(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    selection->unlink();
}

bool should_remove_original() {
    return Inkscape::Preferences::get()->getBool("/options/maskobject/remove", true);
}

void object_clip_set(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Object Clip Set
    selection->setMask(true, false, should_remove_original());
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Set clipping path"), "");
}

void object_clip_set_inverse(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Object Clip Set Inverse
    selection->setMask(true, false, should_remove_original());
    Inkscape::LivePathEffect::sp_inverse_powerclip(selection);
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Set Inverse Clip(LPE)"), "");
}

void object_clip_release(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Object Clip Release
    Inkscape::LivePathEffect::sp_remove_powerclip(selection);
    selection->unsetMask(true, true, should_remove_original());
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Release clipping path"), "");
}

void object_clip_set_group(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    selection->setClipGroup();
    // Undo added in setClipGroup().
}

void object_mask_set(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Object Mask Set
    selection->setMask(false, false, should_remove_original());
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Set mask"), "");
}

void object_mask_set_inverse(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Object Mask Set Inverse
    selection->setMask(false, false, should_remove_original());
    Inkscape::LivePathEffect::sp_inverse_powermask(selection);
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Set Inverse Mask (LPE)"), "");
}

void object_mask_release(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Object Mask Release
    Inkscape::LivePathEffect::sp_remove_powermask(selection);
    selection->unsetMask(false, true, should_remove_original());
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Release mask"), "");
}

void object_rotate_90_cw(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Object Rotate 90
    auto doc = selection->document();
    selection->rotateAnchored((!doc || doc->yaxisdown()) ? 90 : -90);
}

void object_rotate_90_ccw(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Object Rotate 90 CCW
    auto doc = selection->document();
    selection->rotateAnchored((!doc || doc->yaxisdown()) ? -90 : 90);
}

void object_flip_horizontal(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    Geom::OptRect bbox = selection->visualBounds();
    if (!bbox) {
        return;
    }

    // Get center
    Geom::Point center;
    if (selection->center()) {
        center = *selection->center();
    } else {
        center = bbox->midpoint();
    }

    // Object Flip Horizontal
    selection->scaleRelative(center, Geom::Scale(-1.0, 1.0));
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Flip horizontally"),
                                 INKSCAPE_ICON("object-flip-horizontal"));
}

void object_flip_vertical(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    Geom::OptRect bbox = selection->visualBounds();
    if (!bbox) {
        return;
    }

    // Get center
    Geom::Point center;
    if (selection->center()) {
        center = *selection->center();
    } else {
        center = bbox->midpoint();
    }

    // Object Flip Vertical
    selection->scaleRelative(center, Geom::Scale(1.0, -1.0));
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Flip vertically"),
                                 INKSCAPE_ICON("object-flip-vertical"));
}

void object_star_turn_upright(Inkscape::Selection* selection) {
    if (!selection || selection->isEmpty()) {
        show_output("action:object_star_turn_upright: selection empty!");
        return;
    }

    bool has_stars = false;
    for (auto obj : selection->objects()) {
        if (auto star = cast<SPStar>(obj)) {
            has_stars = true;
            star->turn_upright();
        }
    }

    if (!has_stars) {
        show_output("action:objects_star_turn_upright: no SPStar in selection!");
        return;
    }

    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Turn stars upright"),
                                 INKSCAPE_ICON("object-level"));
}

void object_to_path(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    selection->toCurves(false, Inkscape::Preferences::get()->getBool("/options/clonestocurvesjustunlink/value", true));
}

void object_add_corners_lpe(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // We should not have to do this!
    auto document = selection->document();
    if (!document) {
        return;
    }

    auto items = selection->items_vector();
    selection->clear();
    for (auto i : items) {
        if (auto lpeitem = cast<SPLPEItem>(i)) {
            if (auto lpe = lpeitem->getFirstPathEffectOfType(Inkscape::LivePathEffect::FILLET_CHAMFER)) {
                lpeitem->removePathEffect(lpe, false);
                Inkscape::DocumentUndo::done(document, RC_("Undo", "Remove Live Path Effect"),
                                             INKSCAPE_ICON("dialog-path-effects"));
            } else {
                Inkscape::LivePathEffect::Effect::createAndApply("fillet_chamfer", document, lpeitem);
                Inkscape::DocumentUndo::done(document, RC_("Undo", "Create and apply path effect"),
                                             INKSCAPE_ICON("dialog-path-effects"));
            }
            if (auto lpe = lpeitem->getCurrentLPE()) {
                lpe->refresh_widgets = true;
            }
        }
        selection->add(i);
    }
}

void object_stroke_to_path(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    selection->strokesToPaths();
}

const Glib::ustring SECTION = NC_("Action Section", "Object");

static auto object_action_defs = std::to_array<ActionSpec<Inkscape::Selection>>({
    // clang-format off
    {"object-set-attribute",       N_("Set Attribute"),           SECTION, N_("Set or update an attribute of selected objects"), nullptr,
        [](auto selection){ object_set_attribute(QString(), selection); }},
    {"object-set-property",        N_("Set Property"),            SECTION, N_("Set or update a property on selected objects"), nullptr,
        [](auto selection){ object_set_property(QString(), selection); }},
    {"object-get-attribute",       N_("Get Attribute"),           SECTION, N_("Get the value of an attribute of selected objects"), nullptr,
        [](auto selection){ object_get_attribute(QString(), selection); }},
    {"object-get-property",        N_("Get Property"),            SECTION, N_("Get the value of a property on selected objects"), nullptr,
        [](auto selection){ object_get_property(QString(), selection); }},
    {"object-remove-attribute",    N_("Remove Attribute"),        SECTION, N_("Remove an attribute on selected objects"), nullptr,
        [](auto selection){ object_remove_attribute(QString(), selection); }},
    {"object-remove-property",     N_("Remove Property"),         SECTION, N_("Remove a property on selected objects"), nullptr,
        [](auto selection){ object_remove_property(QString(), selection); }},
    {"object-trace",               N_("Trace Bitmap"),            SECTION, N_("Trace selected bitmap"), nullptr,
        [](auto selection){ object_trace(QString(), selection); }},

    {"object-unlink-clones",       N_("Unlink Clones"),           SECTION, N_("Unlink clones and symbols"),
        nullptr, object_unlink_clones},
    {"object-to-path",             N_("Object to Path"),          SECTION, N_("Convert shapes to paths"),
        "object-to-path", object_to_path},
    {"object-add-corners-lpe",     N_("Add Corners LPE"),         SECTION, N_("Add Corners Live Path Effect to path"),
        nullptr, object_add_corners_lpe},
    {"object-stroke-to-path",      N_("Stroke to Path"),          SECTION, N_("Convert strokes to paths"),
        "stroke-to-path", object_stroke_to_path},

    {"object-set-clip",            N_("Object Clip Set"),         SECTION, N_("Apply clipping path to selection (using the topmost object as clipping path)"),
        nullptr, object_clip_set},
    {"object-set-inverse-clip",    N_("Object Clip Set Inverse"), SECTION, N_("Apply inverse clipping path to selection (Power Clip LPE)"),
        nullptr, object_clip_set_inverse},
    {"object-release-clip",        N_("Object Clip Release"),     SECTION, N_("Remove clipping path from selection"),
        nullptr, object_clip_release},
    {"object-set-clip-group",      N_("Object Clip Set Group"),   SECTION, N_("Create a self-clipping group to which objects (not contributing to the clip-path) can be added"),
        nullptr, object_clip_set_group},
    {"object-set-mask",            N_("Object Mask Set"),         SECTION, N_("Apply mask to selection (using the topmost object as mask)"),
        nullptr, object_mask_set},
    {"object-set-inverse-mask",    N_("Object Mask Set Inverse"), SECTION, N_("Apply inverse mask to selection (Power Mask LPE)"),          nullptr, object_mask_set_inverse},
    {"object-release-mask",        N_("Object Mask Release"),     SECTION, N_("Remove mask from selection"),                                nullptr, object_mask_release},

    // Deprecated, see app.transform-rotate(90)
    {"object-rotate-90-cw",        N_("Object Rotate 90°"),       SECTION, N_("Rotate selection 90° clockwise"),                            nullptr, object_rotate_90_cw},
    {"object-rotate-90-ccw",       N_("Object Rotate -90°"),      SECTION, N_("Rotate selection 90° counter-clockwise"),                    nullptr, object_rotate_90_ccw},
    {"object-flip-horizontal",     N_("Object Flip Horizontal"),  SECTION, N_("Flip selected objects horizontally"),                        nullptr, object_flip_horizontal},
    {"object-flip-vertical",       N_("Object Flip Vertical"),    SECTION, N_("Flip selected objects vertically"),                          nullptr, object_flip_vertical},
    {"object-star-turn-upright",   N_("Turn Stars/Polygons Upright"), SECTION, N_("Turn stars and polygons upright"), nullptr, object_star_turn_upright}
    // clang-format on
});

} // namespace

void add_actions_object(LineaApplication* app) {
    ActionRegistry::get().registerActions(app, object_action_defs);
}
