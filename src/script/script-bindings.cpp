// SPDX-License-Identifier: GPL-2.0-or-later
#include "script-bindings.h"

#include <cmath>
#include <functional>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <lua.hpp>

#include "actions/action-registry.h"
#include "desktop.h"
#include "id-clash.h"
#include "layer-manager.h"
#include "linea-application.h"
#include "object/object-set.h"
#include "object/sp-flowtext.h"
#include "object/sp-item.h"
#include "object/sp-object.h"
#include "object/sp-path.h"
#include "object/sp-star.h"
#include "object/sp-text.h"
#include "script-engine.h"
#include "selection.h"
#include "text-editing.h"
#include "svg/css-ostringstream.h"
#include "util/cast.h"
#include "xml/repr.h"

namespace Linea::Script {
namespace {

constexpr char document_metatable[] = "Linea.Script.Document";
constexpr char object_metatable[] = "Linea.Script.Object";
constexpr char style_metatable[] = "Linea.Script.Style";
constexpr char selection_metatable[] = "Linea.Script.Selection";

Engine* engineFor(lua_State* state) {
    return *static_cast<Engine**>(lua_getextraspace(state));
}

struct DocumentHandle {
    LineaApplication* app;
    SPDocument* document;
};

struct ObjectHandle {
    LineaApplication* app;
    SPDocument* document;
    std::string id;
};

struct StyleHandle {
    LineaApplication* app;
    SPDocument* document;
    std::string id;
};

struct SelectionHandle {
    LineaApplication* app;
    SPDesktop* desktop;
};

bool live(const DocumentHandle& handle) {
    if (!handle.app || !handle.document) {
        return false;
    }
    auto documents = handle.app->get_documents();
    return std::find(documents.begin(), documents.end(), handle.document) != documents.end();
}

SPDocument* document(lua_State* state, int index) {
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, index, document_metatable));
    if (!live(*handle)) {
        luaL_error(state, "document is no longer available");
    }
    return handle->document;
}

SPObject* object(lua_State* state, int index) {
    auto handle = static_cast<ObjectHandle*>(luaL_checkudata(state, index, object_metatable));
    DocumentHandle document_handle{handle->app, handle->document};
    if (!live(document_handle)) {
        luaL_error(state, "object document is no longer available");
    }

    auto result = handle->document->getObjectById(handle->id);
    if (!result) {
        luaL_error(state, "object no longer exists");
    }
    return result;
}

Inkscape::Selection* selection(lua_State* state, int index) {
    auto handle = static_cast<SelectionHandle*>(luaL_checkudata(state, index, selection_metatable));
    if (!handle->app || handle->app->get_active_desktop() != handle->desktop) {
        luaL_error(state, "selection is no longer active");
    }
    return handle->desktop->getSelection();
}

void push_selection(lua_State* state, LineaApplication* app, SPDesktop* desktop) {
    auto handle = static_cast<SelectionHandle*>(lua_newuserdatauv(state, sizeof(SelectionHandle), 0));
    std::construct_at(handle, SelectionHandle{app, desktop});
    luaL_setmetatable(state, selection_metatable);
}

void push_object(lua_State* state, LineaApplication* app, SPDocument* document, SPObject* object) {
    auto handle = static_cast<ObjectHandle*>(lua_newuserdatauv(state, sizeof(ObjectHandle), 0));
    std::construct_at(handle, ObjectHandle{app, document, object->getId() ? object->getId() : ""});
    luaL_setmetatable(state, object_metatable);
}

SPObject* style_object(lua_State* state, int index) {
    auto handle = static_cast<StyleHandle*>(luaL_checkudata(state, index, style_metatable));
    DocumentHandle document_handle{handle->app, handle->document};
    if (!live(document_handle)) {
        luaL_error(state, "style object document is no longer available");
    }
    auto result = handle->document->getObjectById(handle->id);
    if (!result) {
        luaL_error(state, "style object no longer exists");
    }
    return result;
}

void push_style(lua_State* state, LineaApplication* app, SPDocument* document, SPObject* object) {
    auto handle = static_cast<StyleHandle*>(lua_newuserdatauv(state, sizeof(StyleHandle), 0));
    std::construct_at(handle, StyleHandle{app, document, object->getId() ? object->getId() : ""});
    luaL_setmetatable(state, style_metatable);
}

void set_style_property(SPObject* object, const char* property, lua_State* state, int value_index) {
    auto css = sp_repr_css_attr_new();
    if (auto style = object->getAttribute("style")) {
        sp_repr_css_attr_add_from_string(css, style);
    }
    switch (lua_type(state, value_index)) {
        case LUA_TSTRING:
            sp_repr_css_set_property(css, property, lua_tostring(state, value_index));
            break;
        case LUA_TNUMBER:
            sp_repr_css_set_property_double(css, property, lua_tonumber(state, value_index));
            break;
        default:
            sp_repr_css_attr_unref(css);
            luaL_error(state, "style values must be strings or numbers");
    }
    sp_repr_css_set(object->getRepr(), css, "style");
    sp_repr_css_attr_unref(css);
    object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

std::string css_style_name(const char* property) {
    std::string name = property;
    std::replace(name.begin(), name.end(), '_', '-');
    return name;
}

int style_index(lua_State* state) {
    auto item = style_object(state, 1);
    const auto property = css_style_name(luaL_checkstring(state, 2));
    auto css = sp_repr_css_attr(item->getRepr(), "style");
    if (!css) {
        lua_pushnil(state);
        return 1;
    }
    auto value = sp_repr_css_property(css, property.c_str(), nullptr);
    if (value) {
        lua_pushstring(state, value);
    } else {
        lua_pushnil(state);
    }
    sp_repr_css_attr_unref(css);
    return 1;
}

int style_newindex(lua_State* state) {
    auto item = style_object(state, 1);
    const auto property = css_style_name(luaL_checkstring(state, 2));
    set_style_property(item, property.c_str(), state, 3);
    engineFor(state)->markModified();
    return 0;
}

int style_gc(lua_State* state) {
    auto handle = static_cast<StyleHandle*>(luaL_checkudata(state, 1, style_metatable));
    std::destroy_at(handle);
    return 0;
}

int object_id_get(lua_State* state, SPObject* item) {
    auto value = item->getId();
    if (value) {
        lua_pushstring(state, value);
    } else {
        lua_pushnil(state);
    }
    return 1;
}

int object_label_get(lua_State* state, SPObject* item) {
    auto value = item->getAttribute("inkscape:label");
    if (value) {
        lua_pushstring(state, value);
    } else {
        lua_pushnil(state);
    }
    return 1;
}

int object_label_set(lua_State* state, SPObject* item, int value_index) {
    item->setLabel(luaL_checkstring(state, value_index));
    engineFor(state)->markModified();
    return 0;
}

bool is_text_item(SPObject* item) {
    return is<SPText>(item) || is<SPFlowtext>(item);
}

int object_text_get(lua_State* state, SPObject* item) {
    if (!is_text_item(item)) {
        return luaL_error(state, "object is not a text object");
    }
    const auto value = sp_te_get_string_multiline(static_cast<SPItem*>(item));
    lua_pushstring(state, value.c_str());
    return 1;
}

int object_text_set(lua_State* state, SPObject* item, int value_index) {
    if (!is_text_item(item)) {
        return luaL_error(state, "object is not a text object");
    }
    sp_te_set_repr_text_multiline(static_cast<SPItem*>(item), luaL_checkstring(state, value_index));
    item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    engineFor(state)->markModified();
    return 0;
}

int object_parent_get(lua_State* state, SPObject* item) {
    if (!item->parent) {
        lua_pushnil(state);
        return 1;
    }

    auto handle = static_cast<ObjectHandle*>(luaL_checkudata(state, 1, object_metatable));
    push_object(state, handle->app, handle->document, item->parent);
    return 1;
}

int object_children_get(lua_State* state, SPObject* item) {
    auto handle = static_cast<ObjectHandle*>(luaL_checkudata(state, 1, object_metatable));
    lua_newtable(state);
    int index = 1;
    for (auto child : item->childList(false)) {
        push_object(state, handle->app, handle->document, child);
        lua_rawseti(state, -2, index++);
    }
    return 1;
}

int object_style_get(lua_State* state, SPObject* item) {
    auto handle = static_cast<ObjectHandle*>(luaL_checkudata(state, 1, object_metatable));
    push_style(state, handle->app, handle->document, item);
    return 1;
}

struct ObjectField {
    const char* name;
    int (*get)(lua_State*, SPObject*);
    int (*set)(lua_State*, SPObject*, int);
};

const ObjectField object_fields[] = {
    // clang-format off
    {"id", object_id_get, nullptr},
    {"label", object_label_get, object_label_set},
    {"text", object_text_get, object_text_set},
    {"parent", object_parent_get, nullptr},
    {"children", object_children_get, nullptr},
    {"style", object_style_get, nullptr},
    // clang-format on
};

const ObjectField* find_object_field(std::string_view name) {
    for (const auto& field : object_fields) {
        if (field.name == name) {
            return &field;
        }
    }
    return nullptr;
}

int object_index(lua_State* state) {
    auto item = object(state, 1);
    const char* property = luaL_checkstring(state, 2);
    if (auto field = find_object_field(property); field && field->get) {
        return field->get(state, item);
    }

    luaL_getmetatable(state, object_metatable);
    lua_getfield(state, -1, property);
    return 1;
}

int object_newindex(lua_State* state) {
    auto item = object(state, 1);
    const char* property = luaL_checkstring(state, 2);
    auto field = find_object_field(property);
    if (!field) {
        return luaL_error(state, "unknown object field: %s", property);
    }
    if (!field->set) {
        return luaL_error(state, "object field is read-only: %s", property);
    }

    return field->set(state, item, 3);
}

int run_action(lua_State* state) {
    const char* id = luaL_checkstring(state, 1);
    auto& registry = ActionRegistry::get();
    if (!registry.hasAction(id)) {
        return luaL_error(state, "unknown action: %s", id);
    }

    auto action = registry.action(id);
    if (!action->isEnabled()) {
        return luaL_error(state, "action is disabled: %s", id);
    }
    action->trigger();
    return 0;
}

int linea_selection(lua_State* state) {
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "selection expects no arguments");
    }
    auto app = static_cast<LineaApplication*>(lua_touserdata(state, lua_upvalueindex(1)));
    auto desktop = app ? app->get_active_desktop() : nullptr;
    if (!desktop) {
        lua_pushnil(state);
        return 1;
    }
    push_selection(state, app, desktop);
    return 1;
}

int selection_count(lua_State* state) {
    lua_pushinteger(state, selection(state, 1)->size());
    return 1;
}

int selection_ids(lua_State* state) {
    lua_newtable(state);
    int index = 1;
    for (auto object : selection(state, 1)->objects()) {
        if (!object->getId()) {
            continue;
        }
        lua_pushstring(state, object->getId());
        lua_rawseti(state, -2, index++);
    }
    return 1;
}

int selection_items(lua_State* state) {
    auto handle = static_cast<SelectionHandle*>(luaL_checkudata(state, 1, selection_metatable));
    auto selected = selection(state, 1);
    lua_newtable(state);
    int index = 1;
    for (auto item : selected->items()) {
        push_object(state, handle->app, handle->desktop->getDocument(), item);
        lua_rawseti(state, -2, index++);
    }
    return 1;
}

SPObject* selection_object(lua_State* state, int index, SelectionHandle* handle) {
    auto item = object(state, index);
    if (item->document != handle->desktop->getDocument()) {
        luaL_error(state, "object belongs to a different document");
    }
    return item;
}

int selection_set(lua_State* state) {
    auto handle = static_cast<SelectionHandle*>(luaL_checkudata(state, 1, selection_metatable));
    auto selected = selection(state, 1);
    selected->set(selection_object(state, 2, handle));
    return 0;
}

int selection_add(lua_State* state) {
    auto handle = static_cast<SelectionHandle*>(luaL_checkudata(state, 1, selection_metatable));
    auto selected = selection(state, 1);
    selected->add(selection_object(state, 2, handle));
    return 0;
}

int selection_remove(lua_State* state) {
    auto handle = static_cast<SelectionHandle*>(luaL_checkudata(state, 1, selection_metatable));
    auto selected = selection(state, 1);
    selected->remove(selection_object(state, 2, handle));
    return 0;
}

int selection_clear(lua_State* state) {
    selection(state, 1)->clear();
    return 0;
}

int selection_gc(lua_State* state) {
    auto handle = static_cast<SelectionHandle*>(luaL_checkudata(state, 1, selection_metatable));
    std::destroy_at(handle);
    return 0;
}

void push_document(lua_State* state, LineaApplication* app, SPDocument* document) {
    auto handle = static_cast<DocumentHandle*>(lua_newuserdatauv(state, sizeof(DocumentHandle), 0));
    *handle = {app, document};
    luaL_setmetatable(state, document_metatable);
}

int linea_document(lua_State* state) {
    auto app = static_cast<LineaApplication*>(lua_touserdata(state, lua_upvalueindex(1)));
    SPDocument* document = nullptr;
    switch (lua_gettop(state)) {
        case 0:
            document = app ? app->get_active_document() : nullptr;
            break;
        case 1: {
            const auto index = luaL_checkinteger(state, 1);
            const auto documents = app ? app->get_documents() : std::vector<SPDocument*>();
            if (index >= 1 && index <= static_cast<lua_Integer>(documents.size())) {
                document = documents[static_cast<size_t>(index - 1)];
            }
            break;
        }
        default:
            return luaL_error(state, "document expects zero arguments or an index");
    }

    if (!document) {
        lua_pushnil(state);
        return 1;
    }
    push_document(state, app, document);
    return 1;
}

int document_object(lua_State* state) {
    auto document_object = document(state, 1);
    auto id = luaL_checkstring(state, 2);
    auto object = document_object->getObjectById(id);
    if (!object) {
        lua_pushnil(state);
        return 1;
    }

    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    push_object(state, handle->app, document_object, object);
    return 1;
}

bool is_xml_name_start(unsigned char character) {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || character == '_';
}

bool is_xml_name_character(unsigned char character) {
    return is_xml_name_start(character) || (character >= '0' && character <= '9') || character == '-' ||
           character == '.';
}

std::string sanitize_xml_local_name(std::string_view requested) {
    if (requested.starts_with("svg:")) {
        requested.remove_prefix(4);
    }

    std::string result;
    result.reserve(requested.size());
    for (size_t index = 0; index < requested.size(); ++index) {
        const auto character = static_cast<unsigned char>(requested[index]);
        result.push_back((index == 0 ? is_xml_name_start(character) : is_xml_name_character(character))
                             ? static_cast<char>(character)
                             : '_');
    }
    return result;
}

SPGroup* editable_layer(lua_State* state, DocumentHandle* handle) {
    auto desktop = handle->app->get_active_desktop();
    if (!desktop || desktop->getDocument() != handle->document) {
        luaL_error(state, "document is not the active editable document");
    }
    auto layer = desktop->layerManager().currentLayer();
    if (!layer) {
        luaL_error(state, "document has no current layer");
    }
    return layer;
}

int optional_style_table(lua_State* state, int index) {
    if (lua_isnoneornil(state, index)) {
        return 0;
    }
    luaL_checktype(state, index, LUA_TTABLE);
    return index;
}

struct OptionalNumberAndStyle {
    bool has_number;
    int style_index;
};

OptionalNumberAndStyle optional_number_and_style(lua_State* state, int number_index) {
    const int argument_count = lua_gettop(state);
    if (argument_count == number_index && lua_istable(state, number_index)) {
        return {false, number_index};
    }
    return {argument_count >= number_index && !lua_isnil(state, number_index),
            argument_count == number_index + 1 ? optional_style_table(state, number_index + 1) : 0};
}

void apply_style_table(lua_State* state, SPItem* item, int table_index) {
    table_index = lua_absindex(state, table_index);
    luaL_checktype(state, table_index, LUA_TTABLE);
    lua_pushnil(state);
    while (lua_next(state, table_index) != 0) {
        if (lua_type(state, -2) != LUA_TSTRING) {
            luaL_error(state, "style table keys must be strings");
        }
        const auto property = css_style_name(lua_tostring(state, -2));
        set_style_property(item, property.c_str(), state, -1);
        lua_pop(state, 1);
    }
}

SPItem* create_document_item(
    lua_State* state, DocumentHandle* handle, const char* tag, const char* id_base,
    std::initializer_list<std::pair<const char*, const char*>> attributes,
    std::initializer_list<std::pair<const char*, double>> numeric_attributes,
    const std::function<void(SPItem*)>& configure = {}, int style_index = 0) {
    auto layer = editable_layer(state, handle);
    auto repr = handle->document->getReprDoc()->createElement(tag);
    auto id = handle->document->generate_unique_id(id_base);
    repr->setAttribute("id", id.c_str());
    for (const auto& [name, value] : attributes) {
        repr->setAttribute(name, value);
    }

    auto created = layer->appendChildRepr(repr);
    Inkscape::GC::release(repr);
    auto item = cast<SPItem>(created);
    if (!item) {
        luaL_error(state, "created element is not an item");
    }
    item->transform = layer->i2doc_affine().inverse();
    for (const auto& [name, value] : numeric_attributes) {
        item->setAttributeDouble(name, value);
    }
    if (configure) {
        configure(item);
    }
    if (style_index != 0) {
        apply_style_table(state, item, style_index);
    }
    item->updateRepr();
    engineFor(state)->markModified();
    return item;
}

int document_create(lua_State* state) {
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    auto layer = editable_layer(state, handle);

    const char* requested_tag = luaL_checkstring(state, 2);
    const std::string local_tag = sanitize_xml_local_name(requested_tag);
    if (local_tag.empty()) {
        return luaL_error(state, "invalid tag name");
    }
    const std::string tag = "svg:" + local_tag;
    luaL_checktype(state, 3, LUA_TTABLE);

    auto repr = handle->document->getReprDoc()->createElement(tag.c_str());
    std::vector<std::pair<std::string, double>> numeric_attributes;
    lua_pushnil(state);
    while (lua_next(state, 3) != 0) {
        const char* name = luaL_checkstring(state, -2);
        switch (lua_type(state, -1)) {
            case LUA_TNUMBER:
                if (std::string_view(name) == "id") {
                    luaL_error(state, "SVG id attributes must be strings");
                }
                numeric_attributes.emplace_back(name, luaL_checknumber(state, -1));
                break;
            case LUA_TSTRING: {
                const char* value = luaL_checkstring(state, -1);
                if (std::string_view(name) == "id") {
                    auto id = generate_similar_unique_id(handle->document, Glib::ustring(value));
                    repr->setAttribute("id", id.c_str());
                } else {
                    repr->setAttribute(name, value);
                }
                break;
            }
            case LUA_TBOOLEAN:
                repr->setAttribute(name, lua_toboolean(state, -1) ? "true" : "false");
                break;
            default:
                luaL_error(state, "SVG attribute values must be strings, numbers, or booleans");
        }
        lua_pop(state, 1);
    }
    if (!repr->attribute("id")) {
        auto id = generate_similar_unique_id(handle->document, Glib::ustring(local_tag));
        repr->setAttribute("id", id.c_str());
    }

    auto created = layer->appendChildRepr(repr);
    Inkscape::GC::release(repr);
    auto item = cast<SPItem>(created);
    if (!item) {
        return luaL_error(state, "created element is not an item");
    }
    item->transform = layer->i2doc_affine().inverse();
    for (const auto& [name, value] : numeric_attributes) {
        item->setAttributeDouble(name.c_str(), value);
    }
    item->updateRepr();
    engineFor(state)->markModified();
    push_object(state, handle->app, handle->document, created);
    return 1;
}

int document_create_path(lua_State* state) {
    const int argument_count = lua_gettop(state);
    if (argument_count != 1 && argument_count != 2) {
        return luaL_error(state, "create_path expects an optional style table");
    }
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    const int style_index = optional_style_table(state, 2);
    auto created = create_document_item(state, handle, "svg:path", "path", {{"d", ""}}, {},
                                        {}, style_index);
    push_object(state, handle->app, handle->document, created);
    return 1;
}

int document_create_rect(lua_State* state) {
    const int argument_count = lua_gettop(state);
    if (argument_count != 5 && argument_count != 6) {
        return luaL_error(state, "create_rect expects x, y, width, height, and optional style");
    }
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    const double x = luaL_checknumber(state, 2);
    const double y = luaL_checknumber(state, 3);
    const double width = luaL_checknumber(state, 4);
    const double height = luaL_checknumber(state, 5);
    const int style_index = optional_style_table(state, 6);
    auto created = create_document_item(state, handle, "svg:rect", "rect", {},
                                        {{"x", x}, {"y", y}, {"width", width}, {"height", height}},
                                        {}, style_index);
    push_object(state, handle->app, handle->document, created);
    return 1;
}

int document_create_ellipse(lua_State* state) {
    const int argument_count = lua_gettop(state);
    if (argument_count < 4 || argument_count > 6) {
        return luaL_error(state, "create_ellipse expects x, y, r1, optional r2, and optional style");
    }
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    const double x = luaL_checknumber(state, 2);
    const double y = luaL_checknumber(state, 3);
    const double r1 = luaL_checknumber(state, 4);
    const auto optional = optional_number_and_style(state, 5);

    SPItem* created = nullptr;
    if (!optional.has_number) {
        created = create_document_item(state, handle, "svg:circle", "circle", {},
                                       {{"cx", x}, {"cy", y}, {"r", r1}}, {},
                                       optional.style_index);
    } else {
        const double r2 = luaL_checknumber(state, 5);
        created = create_document_item(state, handle, "svg:ellipse", "ellipse", {},
                                       {{"cx", x}, {"cy", y}, {"rx", r1}, {"ry", r2}},
                                       {}, optional.style_index);
    }
    push_object(state, handle->app, handle->document, created);
    return 1;
}

int document_create_polygon(lua_State* state) {
    const int argument_count = lua_gettop(state);
    if (argument_count != 5 && argument_count != 6) {
        return luaL_error(state, "create_polygon expects x, y, sides, side_length, and optional style");
    }
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    const double x = luaL_checknumber(state, 2);
    const double y = luaL_checknumber(state, 3);
    const lua_Integer sides_argument = luaL_checkinteger(state, 4);
    if (sides_argument < 3 || sides_argument > 1024) {
        return luaL_error(state, "create_polygon sides must be between 3 and 1024");
    }
    const int sides = static_cast<int>(sides_argument);
    const double side_length = luaL_checknumber(state, 5);
    const int style_index = optional_style_table(state, 6);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(side_length) || side_length <= 0) {
        return luaL_error(state, "create_polygon expects finite coordinates and a positive side_length");
    }

    auto created = create_document_item(
        state, handle, "svg:path", "polygon", {{"sodipodi:type", "star"}}, {},
        [state, x, y, sides, side_length](SPItem* item) {
            auto star = cast<SPStar>(item);
            if (!star) {
                luaL_error(state, "created polygon is not an SPStar");
                return;
            }
            const Geom::Point center = Geom::Point(x, y) * item->transform;
            sp_star_set_regular_polygon(star, sides, center, side_length);
        }, style_index);
    push_object(state, handle->app, handle->document, created);
    return 1;
}

int document_create_star(lua_State* state) {
    const int argument_count = lua_gettop(state);
    if (argument_count < 5 || argument_count > 7) {
        return luaL_error(state, "create_star expects x, y, corners, r1, optional r2, and optional style");
    }
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    const double x = luaL_checknumber(state, 2);
    const double y = luaL_checknumber(state, 3);
    const lua_Integer sides_argument = luaL_checkinteger(state, 4);
    if (sides_argument < 2 || sides_argument > 1024) {
        return luaL_error(state, "create_star sides must be between 2 and 1024");
    }
    const int sides = static_cast<int>(sides_argument);
    const double r1 = luaL_checknumber(state, 5);
    const auto optional = optional_number_and_style(state, 6);
    const double r2 = optional.has_number ? luaL_checknumber(state, 6) : r1 * 0.5;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(r1) || !std::isfinite(r2) ||
        r1 <= 0 || r2 <= 0 || r2 > r1) {
        return luaL_error(state, "create_star expects finite coordinates and radii with 0 < r2 <= r1");
    }

    const double arg1 = -std::numbers::pi / 2.0;
    auto created = create_document_item(
        state, handle, "svg:path", "star", {{"sodipodi:type", "star"}}, {},
        [state, x, y, sides, r1, r2, arg1](SPItem* item) {
            auto star = cast<SPStar>(item);
            if (!star) {
                luaL_error(state, "created star is not an SPStar");
                return;
            }
            const Geom::Point center = Geom::Point(x, y) * item->transform;
            sp_star_position_set(star, sides, center, r1, r2,
                                 arg1, arg1 + std::numbers::pi / sides,
                                 false, 0.0, 0.0);
        }, optional.style_index);
    push_object(state, handle->app, handle->document, created);
    return 1;
}

int document_create_text(lua_State* state) {
    const int argument_count = lua_gettop(state);
    if (argument_count != 4 && argument_count != 5) {
        return luaL_error(state, "create_text expects x, y, content, and optional style");
    }
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    const double x = luaL_checknumber(state, 2);
    const double y = luaL_checknumber(state, 3);
    const std::string content = luaL_checkstring(state, 4);
    const int style_index = optional_style_table(state, 5);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return luaL_error(state, "create_text expects finite coordinates");
    }

    auto created = create_document_item(
        state, handle, "svg:text", "text", {{"xml:space", "preserve"}},
        {{"x", x}, {"y", y}},
        [state, content](SPItem* item) {
            auto text = cast<SPText>(item);
            if (!text) {
                luaL_error(state, "created element is not a text object");
                return;
            }
            sp_te_set_repr_text_multiline(text, content.c_str());
        }, style_index);
    push_object(state, handle->app, handle->document, created);
    return 1;
}

SPPath* path(lua_State* state, int index) {
    auto result = cast<SPPath>(object(state, index));
    if (!result) {
        luaL_error(state, "object is not a path");
    }
    return result;
}

int append_path_command(lua_State* state, const char* command, int argument_count) {
    auto path_object = path(state, 1);
    if (lua_gettop(state) != argument_count + 1) {
        return luaL_error(state, "%s expects %d numeric arguments", command, argument_count);
    }

    const std::string existing = path_object->getAttribute("d") ? path_object->getAttribute("d") : "";
    Inkscape::CSSOStringStream path_data;
    path_data << existing;
    if (!existing.empty()) {
        path_data << ' ';
    }
    path_data << command;
    for (int index = 0; index < argument_count; ++index) {
        path_data << ' ' << luaL_checknumber(state, index + 2);
    }
    const auto d = path_data.str();
    path_object->setAttribute("d", d.c_str());
    path_object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    path_object->updateRepr();
    engineFor(state)->markModified();
    return 0;
}

int path_close(lua_State* state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "close expects no arguments");
    }
    return append_path_command(state, "Z", 0);
}

int path_command(lua_State* state) {
    auto command = luaL_checkstring(state, lua_upvalueindex(1));
    const auto argument_count = static_cast<int>(luaL_checkinteger(state, lua_upvalueindex(2)));
    return append_path_command(state, command, argument_count);
}

int object_get_attribute(lua_State* state) {
    auto item = object(state, 1);
    auto name = luaL_checkstring(state, 2);
    auto value = item->getAttribute(name);
    if (value) {
        lua_pushstring(state, value);
    } else {
        lua_pushnil(state);
    }
    return 1;
}

int object_set_attribute(lua_State* state) {
    auto item = object(state, 1);
    auto name = luaL_checkstring(state, 2);
    auto value = luaL_checkstring(state, 3);
    item->setAttribute(name, value);
    item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    if (auto engine = engineFor(state)) {
        engine->markModified();
    }
    return 0;
}

int object_set_style(lua_State* state) {
    auto item = object(state, 1);
    auto name = luaL_checkstring(state, 2);
    set_style_property(item, name, state, 3);
    engineFor(state)->markModified();
    return 0;
}

int object_delete(lua_State* state) {
    auto item = object(state, 1);
    item->deleteObject();
    engineFor(state)->markModified();
    return 0;
}

std::unique_ptr<Inkscape::ObjectSet> object_set(lua_State* state, SPObject* object) {
    auto set = std::make_unique<Inkscape::ObjectSet>(object->document);
    if (!set->add(object, true)) {
        luaL_error(state, "unable to add object to script set");
    }
    return set;
}

int object_translate(lua_State* state) {
    auto item = cast<SPItem>(object(state, 1));
    if (!item) {
        return luaL_error(state, "object is not an item");
    }
    auto set = object_set(state, item);
    set->moveRelative(luaL_checknumber(state, 2), luaL_checknumber(state, 3));
    engineFor(state)->markModified();
    return 0;
}

int object_scale(lua_State* state) {
    auto item = cast<SPItem>(object(state, 1));
    if (!item) {
        return luaL_error(state, "object is not an item");
    }
    auto set = object_set(state, item);
    auto bounds = set->visualBounds();
    if (!bounds) {
        return luaL_error(state, "object has no bounds");
    }
    const double sx = luaL_checknumber(state, 2);
    const double sy = lua_gettop(state) >= 3 ? luaL_checknumber(state, 3) : sx;
    set->scaleRelative(bounds->midpoint(), Geom::Scale(sx, sy));
    engineFor(state)->markModified();
    return 0;
}

int object_rotate(lua_State* state) {
    auto item = cast<SPItem>(object(state, 1));
    if (!item) {
        return luaL_error(state, "object is not an item");
    }
    auto set = object_set(state, item);
    auto bounds = set->visualBounds();
    if (!bounds) {
        return luaL_error(state, "object has no bounds");
    }
    set->rotateRelative(bounds->midpoint(), luaL_checknumber(state, 2));
    engineFor(state)->markModified();
    return 0;
}

int object_shear(lua_State* state) {
    auto item = cast<SPItem>(object(state, 1));
    if (!item) {
        return luaL_error(state, "object is not an item");
    }
    auto set = object_set(state, item);
    auto bounds = set->visualBounds();
    if (!bounds) {
        return luaL_error(state, "object has no bounds");
    }
    const double sx = luaL_checknumber(state, 2);
    const double sy = lua_gettop(state) >= 3 ? luaL_checknumber(state, 3) : 0;
    set->skewRelative(bounds->midpoint(), sx, sy);
    engineFor(state)->markModified();
    return 0;
}

int object_gc(lua_State* state) {
    auto handle = static_cast<ObjectHandle*>(luaL_checkudata(state, 1, object_metatable));
    handle->~ObjectHandle();
    return 0;
}

int document_gc(lua_State* state) {
    auto handle = static_cast<DocumentHandle*>(luaL_checkudata(state, 1, document_metatable));
    handle->~DocumentHandle();
    return 0;
}

void register_metatables(lua_State* state) {
    // Document userdata owns only a checked application/document handle.
    // Its methods are exposed through __index below.
    luaL_newmetatable(state, document_metatable);
    const luaL_Reg methods[] = {
        // clang-format off
        {"object", document_object},
        {"create", document_create},
        {"create_rect", document_create_rect},
        {"create_ellipse", document_create_ellipse},
        {"create_polygon", document_create_polygon},
        {"create_star", document_create_star},
        {"create_text", document_create_text},
        {"create_path", document_create_path},
        {"__gc", document_gc},
        {nullptr, nullptr},
        // clang-format on
    };
    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pop(state, 1);

    // Selection userdata is tied to a desktop, not to a document. Its
    // accessors validate that the desktop is still the active one.
    luaL_newmetatable(state, selection_metatable);
    const luaL_Reg selection_methods[] = {
        // clang-format off
        {"count", selection_count}, {"ids", selection_ids}, {"items", selection_items},
        {"set", selection_set},     {"add", selection_add}, {"remove", selection_remove},
        {"clear", selection_clear}, {"__gc", selection_gc}, {nullptr, nullptr},
        // clang-format on
    };
    luaL_setfuncs(state, selection_methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pop(state, 1);

    // Style userdata is returned by object.style. __newindex turns Lua
    // property assignment into an inline CSS property update.
    luaL_newmetatable(state, style_metatable);
    lua_pushcfunction(state, style_index);
    lua_setfield(state, -2, "__index");
    lua_pushcfunction(state, style_newindex);
    lua_setfield(state, -2, "__newindex");
    lua_pushcfunction(state, style_gc);
    lua_setfield(state, -2, "__gc");
    lua_pop(state, 1);

    // Object userdata stores a document identity and object ID, resolving
    // the object on every operation so deleted objects cannot be reused.
    luaL_newmetatable(state, object_metatable);
    const luaL_Reg object_methods[] = {
        // clang-format off
        {"get_attribute", object_get_attribute},
        {"set_attribute", object_set_attribute},
        {"set_style", object_set_style},
        {"delete", object_delete},
        {"translate", object_translate},
        {"scale", object_scale},
        {"rotate", object_rotate},
        {"shear", object_shear},
        {"close", path_close},
        {"__gc", object_gc},
        {nullptr, nullptr},
        // clang-format on
    };
    luaL_setfuncs(state, object_methods, 0);
    struct PathCommand {
        const char* name;
        const char* command;
        int arguments;
    };
    static constexpr PathCommand path_commands[] = {
        // clang-format off
        {"move_to", "M", 2},
        {"move_by", "m", 2},
        {"line_to", "L", 2},
        {"line_by", "l", 2},
        {"horizontal_to", "H", 1},
        {"horizontal_by", "h", 1},
        {"vertical_to", "V", 1},
        {"vertical_by", "v", 1},
        {"cubic_to", "C", 6},
        {"cubic_by", "c", 6},
        {"smooth_cubic_to", "S", 4},
        {"smooth_cubic_by", "s", 4},
        {"quadratic_to", "Q", 4},
        {"quadratic_by", "q", 4},
        {"smooth_quadratic_to", "T", 2},
        {"smooth_quadratic_by", "t", 2},
        {"arc_to", "A", 7},
        {"arc_by", "a", 7},
        // clang-format on
    };
    for (const auto& path_command_spec : path_commands) {
        lua_pushstring(state, path_command_spec.command);
        lua_pushinteger(state, path_command_spec.arguments);
        lua_pushcclosure(state, path_command, 2);
        lua_setfield(state, -2, path_command_spec.name);
    }
    lua_pushcfunction(state, object_index);
    lua_setfield(state, -2, "__index");
    lua_pushcfunction(state, object_newindex);
    lua_setfield(state, -2, "__newindex");
    lua_pop(state, 1);
}

} // namespace

void registerBindings(lua_State* state, LineaApplication* app, Engine* engine) {
    (void)engine;
    register_metatables(state);

    lua_newtable(state);
    lua_pushlightuserdata(state, app);
    lua_pushcclosure(state, linea_document, 1);
    lua_setfield(state, -2, "document");
    lua_pushlightuserdata(state, app);
    lua_pushcclosure(state, linea_selection, 1);
    lua_setfield(state, -2, "selection");
    lua_pushcfunction(state, run_action);
    lua_setfield(state, -2, "run_action");
    lua_setglobal(state, "linea");
}

} // namespace Linea::Script
