// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Built-in console commands.
 *
 * Each entry in kCommands declares a command's name, help text, handler,
 * an optional declarative argument spec (see command-args.h), and an
 * optional argument completer. Handlers emit output through the
 * interpreter (printOut/printErr) and resolve the active document,
 * desktop, and selection through the application pointer.
 */

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <giomm/file.h>
#include <format>

#include <2geom/svg-path-parser.h>
#include <2geom/transforms.h>

#include "actions/action-registry.h"
#include "desktop.h"
#include "desktop-style.h"
#include "document-undo.h"
#include "document-undo.h"
#include "interpreter.h"
#include "layer-manager.h"
#include "linea-application.h"
#include "selection-chemistry.h"
#include "selection.h"
#include "document.h"
#include "script/script-engine.h"
#include "script/script-registry.h"
#include "object/sp-item.h"
#include "object/sp-item-group.h"
#include "object/sp-object.h"
#include "object/sp-star.h"
#include "util/cast.h"
#include "xml/document.h"
#include "xml/node.h"
#include "xml/repr.h"

namespace Linea {
namespace {
using namespace args;

void cmdHelp(Interpreter& interp, const ParsedArgs& args, LineaApplication* /*app*/) {
    if (!args.tokens.empty()) {
        const std::string help = interp.commandHelp(args.tokens.front());
        interp.printOut(help.empty()
                            ? "unknown command: " + args.tokens.front() + "\n"
                            : help + "\n");
        return;
    }
    for (const std::string& cmd : interp.commandNames()) {
        interp.printOut(interp.commandHelp(cmd) + "\n");
    }
}

// decode C-style escapes for `echo -e`: \n \t \\ \e \xNN \0NNN
std::string decodeEscapes(std::string_view in) {
    const bp::symbols<char> named{
        {"n", '\n'}, {"t", '\t'}, {"e", '\x1b'}, {"E", '\x1b'}, {"\\", '\\'}};

    std::string out;
    const auto push = [](auto& ctx) {
        bp::_globals(ctx) += static_cast<char>(bp::_attr(ctx));
    };
    const auto hex2 = bp::parser_interface{bp::uint_parser<unsigned, 16, 1, 2>{}};
    const auto oct3 = bp::parser_interface{bp::uint_parser<unsigned, 8, 1, 3>{}};
    const auto esc =
        bp::lit('\\') >> (bp::lit('x') >> hex2[push] |
                          bp::lit('0') >> (oct3[push] | bp::attr('0')[push]) |
                          named[push] |
                          // unrecognized escape: emit both chars literally
                          bp::char_[([](auto& ctx) {
                              bp::_globals(ctx) += '\\';
                              bp::_globals(ctx) += bp::_attr(ctx);
                          })] |
                          // a lone '\' at end of input
                          bp::attr('\\')[push]);
    bp::parse(in, bp::with_globals(*(esc | bp::char_[push]), out));
    return out;
}

void cmdEcho(Interpreter& interp, const ParsedArgs& args, LineaApplication* /*app*/) {
    std::vector<std::string> parts = args.tokens;
    const bool decode = !parts.empty() && parts.front() == "-e";
    if (decode) {
        parts.erase(parts.begin());
    }
    std::string text;
    for (const std::string& p : parts) {
        if (!text.empty()) {
            text += ' ';
        }
        text += p;
    }
    text += '\n';
    interp.printOut(decode ? decodeEscapes(text) : text);
}

void cmdNew(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    if (!app->createNewDocument(static_cast<int>(args.num("index")))) {
        interp.printErr("new: failed to create document\n");
    }
}

void cmdOpen(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    std::string path = args.str("path");
    if (path.starts_with('~')) {
        path = QDir::homePath().toStdString() + path.substr(1);
    }
    app->openDocument(Gio::File::create_for_path(path));
}

void cmdClose(Interpreter& interp, const ParsedArgs& /*args*/, LineaApplication* app) {
    if (!app->get_active_desktop()) {
        interp.printErr("close: no active desktop\n");
        return;
    }
    app->desktopCloseActive();
}

void cmdRevert(Interpreter& interp, const ParsedArgs& /*args*/, LineaApplication* app) {
    SPDocument* doc = app->get_active_document();
    if (!doc) {
        interp.printErr("revert: no active document\n");
        return;
    }
    app->document_revert(doc);
}

void cmdSelect(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    SPDesktop* desktop = app->get_active_desktop();
    if (!desktop) {
        interp.printErr("select: no active desktop\n");
        return;
    }
    if (args.has("id")) {
        SPDocument* doc = desktop->getDocument();
        SPObject* object = doc ? doc->getObjectById(args.str("id")) : nullptr;
        if (!object) {
            interp.printErr("select: no object with id \"" + args.str("id") + "\"\n");
            return;
        }
        desktop->getSelection()->set(object->getRepr());
        return;
    }
    const std::string& what = args.str("which");
    if (what == "all") {
        Inkscape::SelectionHelper::selectAll(desktop);
    } else if (what == "invert") {
        Inkscape::SelectionHelper::invert(desktop);
    } else {
        Inkscape::SelectionHelper::selectNone(desktop);
    }
}

// ---- shape and transform commands ----

// Default x/y for shape commands, set by `location`.
Geom::Point origin(const Interpreter& interp, const ParsedArgs& args) {
    const Geom::Point loc = interp.location();
    return {args.num("x", loc.x()), args.num("y", loc.y())};
}

// Create an SVG element in the current layer — same pattern as the shape
// tools (see RectTool::drag): apply the tool's remembered style, append to
// the current layer, compensate the layer's transform so coordinates are in
// document space, close an undo step. `post` runs on the appended item
// between transform compensation and updateRepr — for parametric shapes
// (e.g. sodipodi:star) that configure themselves post-append. Reports
// errors; returns nullptr on failure.
SPObject* createElement(Interpreter& interp, LineaApplication* app, const char* cmd,
                        const char* tag, const char* stylePath, const ParsedArgs& args,
                        std::initializer_list<std::pair<const char*, std::string>> attrs,
                        const char* undoDesc,
                        const std::function<void(SPItem*)>& post = {}) {
    SPDesktop* desktop = app->get_active_desktop();
    SPDocument* doc = desktop ? desktop->doc() : nullptr;
    SPGroup* layer = desktop ? desktop->layerManager().currentLayer() : nullptr;
    if (!doc || !layer) {
        interp.printErr(std::format("{}: no editable document\n", cmd));
        return nullptr;
    }
    auto repr = doc->getReprDoc()->createElement(tag);
    desktop->applyCurrentOrToolStyle(repr, stylePath, false);
    if (args.has("id")) {
        repr->setAttribute("id",
                           generate_similar_unique_id(doc, args.str("id")).c_str());
    }
    for (const auto& [name, value] : attrs) {
        repr->setAttribute(name, value.c_str());
    }
    SPObject* object = layer->appendChildRepr(repr);
    Inkscape::GC::release(repr);
    if (auto item = cast<SPItem>(object)) {
        item->transform = layer->i2doc_affine().inverse();
        if (post) {
            post(item);
        }
        item->updateRepr();
        desktop->getSelection()->set(item);
    }
    DocumentUndo::done(doc, Inkscape::Util::Internal::ContextString(undoDesc), "");
    return object;
}

// The active selection, or nullptr with an error printed.
Inkscape::Selection* needSelection(Interpreter& interp, const char* cmd,
                                   LineaApplication* app) {
    SPDesktop* desktop = app->get_active_desktop();
    auto sel = desktop ? desktop->getSelection() : nullptr;
    if (!sel || sel->isEmpty()) {
        interp.printErr(std::format("{}: nothing selected\n", cmd));
        return nullptr;
    }
    return sel;
}

void cmdRect(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    const Geom::Point p = origin(interp, args);
    const double w = args.num("w");
    createElement(interp, app, "rect", "svg:rect", "/tools/shapes/rect", args,
                  {{"x", std::format("{}", p.x())},
                   {"y", std::format("{}", p.y())},
                   {"width", std::format("{}", w)},
                   {"height", std::format("{}", args.num("h", w))}},
                  "Create rectangle");
}

void cmdOval(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    const Geom::Point c = origin(interp, args);
    const double r1 = args.num("r1");
    createElement(interp, app, "oval", "svg:ellipse", "/tools/shapes/arc", args,
                  {{"cx", std::format("{}", c.x())},
                   {"cy", std::format("{}", c.y())},
                   {"rx", std::format("{}", r1)},
                   {"ry", std::format("{}", args.num("r2", r1))}},
                  "Create ellipse");
}

void cmdCircle(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    const Geom::Point c = origin(interp, args);
    createElement(interp, app, "circle", "svg:circle", "/tools/shapes/arc", args,
                  {{"cx", std::format("{}", c.x())},
                   {"cy", std::format("{}", c.y())},
                   {"r", std::format("{}", args.num("r"))}},
                  "Create circle");
}

void cmdLine(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    std::vector<double> xs{args.num("x")};
    std::vector<double> ys{args.num("y")};
    for (const double d : args.nums("xn")) {
        xs.push_back(d);
    }
    for (const double d : args.nums("yn")) {
        ys.push_back(d);
    }
    if (xs.size() == 2) {
        createElement(interp, app, "line", "svg:line", "/tools/freehand/pen", args,
                      {{"x1", std::format("{}", xs[0])},
                       {"y1", std::format("{}", ys[0])},
                       {"x2", std::format("{}", xs[1])},
                       {"y2", std::format("{}", ys[1])}},
                      "Create line");
        return;
    }
    std::string points;
    for (size_t i = 0; i < xs.size(); ++i) {
        points += std::format("{},{} ", xs[i], ys[i]);
    }
    points.pop_back(); // trailing space
    createElement(interp, app, "line", "svg:polyline", "/tools/freehand/pen", args,
                  {{"points", std::move(points)}}, "Create polyline");
}

void cmdPolygon(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    // SPStar clamps to 1024 sides (sp_star_position_set)
    const int sides = static_cast<int>(args.num("sides"));
    if (sides < 3 || sides > 1024) {
        interp.printErr("polygon: sides must be between 3 and 1024\n");
        return;
    }
    const Geom::Point c = origin(interp, args);
    createElement(interp, app, "polygon", "svg:path", "/tools/shapes/star", args,
                  {{"sodipodi:type", "star"}}, "Create polygon",
                  [c, sides, size = args.num("size")](SPItem* item) {
                      if (auto star = cast<SPStar>(item)) {
                          const Geom::Point center = c * item->transform;
                          sp_star_set_regular_polygon(star, sides, center, size);
                      }
                  });
}

void cmdPath(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    const std::string& d = args.str("d");
    if (d.empty()) {
        interp.printErr("path: missing path data\n");
        return;
    }
    try {
        Geom::parse_svg_path(d.c_str());
    } catch (const Geom::SVGPathParseError& e) {
        interp.printErr(std::format("path: invalid path data: {}\n", e.what()));
        return;
    }
    createElement(interp, app, "path", "svg:path", "/tools/freehand/pen", args,
                  {{"d", d}}, "Create path");
}

void cmdLocation(Interpreter& interp, const ParsedArgs& args, LineaApplication* /*app*/) {
    if (args.has("x")) {
        interp.setLocation({args.num("x"), args.num("y")});
        return;
    }
    const Geom::Point p = interp.location();
    interp.printOut(std::format("{} {}\n", p.x(), p.y()));
}

void cmdRun(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    std::optional<Script::Source> source;
    if (args.has("name")) {
        source = app->scriptRegistry().find(args.str("name"));
        if (!source) {
            interp.printErr(std::format("run: unknown script: {}\n", args.str("name")));
            return;
        }
    } else {
        source = app->scriptRegistry().current();
        if (!source) {
            interp.printErr("run: no current script\n");
            return;
        }
    }

    auto const error = app->scriptEngine().evaluate(
        *source,
        [&interp](std::string_view text) { interp.printOut(text); });
    if (!error.message.empty()) {
        if (error.line > 0) {
            interp.printErr(std::format("run: {}:{}: {}\n", source->name, error.line, error.message));
        } else {
            interp.printErr(std::format("run: {}\n", error.message));
        }
        return;
    }

    if (app->scriptEngine().modified() && app->get_active_document()) {
        Inkscape::DocumentUndo::done(app->get_active_document(), RC_("Undo", "Run script"), "");
    }
}

void cmdTranslate(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    auto sel = needSelection(interp, "translate", app);
    if (!sel) {
        return;
    }
    sel->moveRelative(args.num("dx"), args.num("dy"));
    DocumentUndo::done(app->get_active_document(), RC_("Undo", "Translate"), "");
}

void cmdScale(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    auto sel = needSelection(interp, "scale", app);
    if (!sel) {
        return;
    }
    const Geom::OptRect bbox = sel->visualBounds();
    if (!bbox) {
        interp.printErr("scale: selection has no bounds\n");
        return;
    }
    const double s = args.num("s");
    sel->scaleRelative(bbox->midpoint(), Geom::Scale(s, args.num("sy", s)));
    DocumentUndo::done(app->get_active_document(), RC_("Undo", "Scale"), "");
}

void cmdRotate(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    auto sel = needSelection(interp, "rotate", app);
    if (!sel) {
        return;
    }
    const Geom::OptRect bbox = sel->visualBounds();
    if (!bbox) {
        interp.printErr("rotate: selection has no bounds\n");
        return;
    }
    sel->rotateRelative(bbox->midpoint(), args.num("angle"));
    DocumentUndo::done(app->get_active_document(), RC_("Undo", "Rotate"), "");
}

void cmdSkew(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    auto sel = needSelection(interp, "skew", app);
    if (!sel) {
        return;
    }
    const Geom::OptRect bbox = sel->visualBounds();
    if (!bbox) {
        interp.printErr("skew: selection has no bounds\n");
        return;
    }
    sel->skewRelative(bbox->midpoint(), args.num("sx"), args.num("sy", 0));
    DocumentUndo::done(app->get_active_document(), RC_("Undo", "Skew"), "");
}

void cmdFlip(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    auto sel = needSelection(interp, "flip", app);
    if (!sel) {
        return;
    }
    const Geom::OptRect bbox = sel->visualBounds();
    if (!bbox) {
        interp.printErr("flip: selection has no bounds\n");
        return;
    }
    const std::string& dir = args.str("dir");
    sel->scaleRelative(bbox->midpoint(),
                       Geom::Scale(dir.find('x') != std::string::npos ? -1 : 1,
                                   dir.find('y') != std::string::npos ? -1 : 1));
    DocumentUndo::done(app->get_active_document(), RC_("Undo", "Flip"), "");
}

// Maps a validated paint value onto a CSS property: NN% sets `<prop>-opacity`,
// anything else sets `<prop>` verbatim.
void setPaintCss(SPCSSAttr* css, const char* prop, const std::string& v) {
    if (v.ends_with('%')) {
        sp_repr_css_set_property_double(css, (std::string(prop) + "-opacity").c_str(),
                                        std::strtod(v.c_str(), nullptr) / 100.0);
    } else {
        sp_repr_css_set_property(css, prop, v.c_str());
    }
}

void cmdFill(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    auto sel = needSelection(interp, "fill", app);
    if (!sel) {
        return;
    }
    SPDesktop* desktop = app->get_active_desktop();
    SPCSSAttr* css = sp_repr_css_attr_new();
    setPaintCss(css, "fill", args.str("value"));
    sp_desktop_set_style(sel, desktop, css);
    sp_repr_css_attr_unref(css);
    DocumentUndo::done(desktop->doc(), Inkscape::Util::Internal::ContextString("Fill"), "");
}

void cmdOpacity(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    auto sel = needSelection(interp, "opacity", app);
    if (!sel) {
        return;
    }
    SPDesktop* desktop = app->get_active_desktop();
    SPCSSAttr* css = sp_repr_css_attr_new();
    sp_repr_css_set_property_double(css, "opacity", args.num("value"));
    sp_desktop_set_style(sel, desktop, css);
    sp_repr_css_attr_unref(css);
    DocumentUndo::done(desktop->doc(), Inkscape::Util::Internal::ContextString("Opacity"), "");
}

void cmdStroke(Interpreter& interp, const ParsedArgs& args, LineaApplication* app) {
    auto sel = needSelection(interp, "stroke", app);
    if (!sel) {
        return;
    }
    SPDesktop* desktop = app->get_active_desktop();
    SPCSSAttr* css = sp_repr_css_attr_new();
    if (args.has("value")) {
        setPaintCss(css, "stroke", args.str("value"));
    }
    if (args.has("w")) {
        // Hairline (stroke 0 / hairline): same trio set_stroke_width writes —
        // -inkscape-stroke for Inkscape, vector-effect+1px as fallback.
        if (args.str("w") == "hairline" || args.num("w") == 0.0) {
            sp_repr_css_set_property_double(css, "stroke-width", 1.0);
            sp_repr_css_set_property(css, "vector-effect", "non-scaling-stroke");
            sp_repr_css_set_property(css, "-inkscape-stroke", "hairline");
        } else {
            sp_repr_css_set_property_double(css, "stroke-width", args.num("w"));
            sp_repr_css_unset_property(css, "vector-effect");
            sp_repr_css_unset_property(css, "-inkscape-stroke");
        }
    }
    sp_desktop_set_style(sel, desktop, css);
    sp_repr_css_attr_unref(css);
    DocumentUndo::done(desktop->doc(), Inkscape::Util::Internal::ContextString("Stroke"), "");
}

void cmdDuplicate(Interpreter& interp, const ParsedArgs& /*args*/, LineaApplication* app) {
    auto sel = needSelection(interp, "duplicate", app);
    if (sel) {
        sel->duplicate();
    }
}

void cmdActions(Interpreter& interp, const ParsedArgs& args, LineaApplication* /*app*/) {
    auto& registry = ActionRegistry::get();
    const std::string_view filter = args.tokens.empty() ? std::string_view() : args.tokens.front();
    for (const std::string& id : registry.actionIds()) {
        if (!filter.empty() && id.find(filter) == std::string::npos) {
            continue;
        }
        interp.printOut(std::format("\x1b[1m{}\x1b[0m — {}\n", id,
                                    registry.action(id)->toolTip().toStdString()));
    }
}

void cmdQuit(Interpreter& /*interp*/, const ParsedArgs& /*args*/, LineaApplication* app) {
    // destroy_all() runs the unsaved-changes checks; a false return means
    // the user cancelled a save dialog, so don't quit in that case
    if (app->destroy_all()) {
        QCoreApplication::quit();
    }
}

CompletionResult completeSelectArgs(std::string_view input, LineaApplication* /*app*/) {
    const size_t start = Interpreter::tokenStart(input);
    const std::string_view prefix = input.substr(start);

    CompletionResult result;
    result.replaceFrom = static_cast<int>(start);
    result.replaceTo = static_cast<int>(input.size());
    for (const std::string_view option : {"all", "invert", "none"}) {
        if (option.starts_with(prefix)) {
            result.items.push_back({std::string(option), {}});
        }
    }
    return result;
}

const Interpreter::CommandDef kCommands[] = {
    {"actions", R"(@b{actions} — list registered actions, optionally filtered)", &cmdActions},
    {"help", R"(@b{help} [command] — list commands or describe one)", &cmdHelp},
    {"run", R"(@b{run} [name] — run the current or named script)", &cmdRun,
     spec(-str("name"))},
    {"echo", R"(@b{echo} — print arguments; -e decodes escapes (\e \xNN \n \t))", &cmdEcho},
    {"new", R"(@b{new} — create a new document [template-index])", &cmdNew, spec(-integer("index"))},
    {"open", R"(@b{open} — <file>)", &cmdOpen, spec(filearg("path")),
     &Interpreter::completeFilePath},
    {"close", R"(@b{close} — close the active desktop)", &cmdClose},
    {"revert", R"(@b{revert} — revert the active document to its saved version)", &cmdRevert},
    {"select", R"(@b{select} — select all|invert|none|"id")", &cmdSelect,
     spec(kw("which", {"all", "invert", "none"}) | id()), &completeSelectArgs},
    {"rect", R"(@b{rect} ["id"] w [h [x y]] — create a rectangle)", &cmdRect,
     spec(-id() >> pos("w") >> -(pos("h") >> -(num("x") >> num("y"))))},
    {"oval", R"(@b{oval} ["id"] r1 [r2 [x y]] — create an ellipse (x y = center))", &cmdOval,
     spec(-id() >> pos("r1") >> -(pos("r2") >> -(num("x") >> num("y"))))},
    {"circle", R"(@b{circle} ["id"] r [x y] — create a circle (x y = center))", &cmdCircle,
     spec(-id() >> pos("r") >> -(num("x") >> num("y")))},
    {"line", R"(@b{line} ["id"] x y [xn yn]+ — create a line or polyline)", &cmdLine,
     spec(-id() >> num("x") >> num("y") >> +(num("xn") >> num("yn")))},
    {"polygon", R"(@b{polygon} ["id"] sides size [x y] — create a regular polygon (3..1024 sides, size = side length))",
     &cmdPolygon,
     spec(-id() >> integer("sides") >> pos("size") >> -(num("x") >> num("y")))},
    {"path", R"(@b{path} ["id"] <svg-path-data> — create a path)", &cmdPath,
     spec(-id() >> rest("d"))},
    {"location", R"(@b{location} [x y] — get/set the default point for new shapes)", &cmdLocation,
     spec(-(num("x") >> num("y")))},
    {"move", R"(@b{move} dx [dy] — move the selection)", &cmdTranslate, spec(num("dx") >> -num("dy"))},
    // {"translate", R"(@b{translate} dx [dy] — translate the selection)", &cmdTranslate, spec(num("dx") >> -num("dy"))},
    {"scale", R"(@b{scale} scale [scale-y] — scale the selection about its center)", &cmdScale,
     spec(pos("s") >> -pos("sy"))},
    {"rotate", R"(@b{rotate} angle — rotate the selection about its center (degrees))", &cmdRotate,
     spec(num("angle"))},
    {"skew", R"(@b{skew} sx [sy] — skew the selection about its center)", &cmdSkew,
     spec(num("sx") >> -num("sy"))},
    {"flip", R"(@b{flip} x|y|xy — mirror the selection about its center)", &cmdFlip,
     spec(kw("dir", {"x", "y", "xy"}))},
    {"fill", R"(@b{fill} none|inherit|NN%|css-color — set fill or fill-opacity)", &cmdFill,
     spec(paint("value"))},
    {"opacity", R"(@b{opacity} 0..1|0%..100% — set element opacity)", &cmdOpacity,
     spec(opacity("value"))},
    {"stroke", R"(@b{stroke} [none|inherit|NN%|css-color] [width|hairline] — set stroke, opacity, width)",
     &cmdStroke, spec(strokeWidth("w") | (paint("value") >> -strokeWidth("w")))},
    {"duplicate", R"(@b{duplicate} — clone the selection)", &cmdDuplicate},
    {"quit", R"(@b{quit} — close all documents and exit)", &cmdQuit},
};

} // namespace

std::span<const Interpreter::CommandDef> builtinCommands() {
    return kCommands;
}

} // namespace Linea
