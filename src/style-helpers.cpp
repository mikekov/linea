// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Helpers for the document's <svg:style> element.
 */

#include "style-helpers.h"

#include <cctype>
#include <cstring>
#include <optional>
#include <utility>

#include "document.h"
#include "xml/node.h"
#include "xml/repr.h"

namespace Inkscape::Css {

bool is_style_element(const Inkscape::XML::Node& node) {
    return node.type() == Inkscape::XML::NodeType::ELEMENT_NODE && !std::strcmp(node.name(), "svg:style");
}

Inkscape::XML::Node* first_style_element(SPDocument* document) {
    if (!document) return nullptr;
    return sp_repr_lookup_name(document->getReprRoot(), "svg:style", 1);
}

Inkscape::XML::Node* ensure_style_element(SPDocument* document) {
    if (!document) return nullptr;
    if (auto elem = first_style_element(document)) {
        return elem;
    }

    auto root = document->getReprRoot();
    auto elem = document->getReprDoc()->createElement("svg:style");
    root->addChild(elem, nullptr);
    Inkscape::GC::release(elem);
    return elem;
}

void set_style_text(SPDocument* document, Inkscape::XML::Node* elem, const char* text) {
    if (!document || !elem) return;

    bool written = false;
    for (auto child = elem->firstChild(); child;) {
        auto next = child->next();
        if (child->type() == Inkscape::XML::NodeType::TEXT_NODE) {
            if (written) {
                elem->removeChild(child);
            } else {
                written = true;
                child->setContent(text);
            }
        }
        child = next;
    }
    if (!written) {
        auto textNode = document->getReprDoc()->createTextNode(text);
        elem->appendChild(textNode);
        Inkscape::GC::release(textNode);
    }
}

StyleRootWatcher::StyleRootWatcher(Inkscape::XML::Node* root, std::function<void()> on_change)
    : _root(root)
    , _on_change(std::move(on_change)) {
    if (!_root) return;
    Inkscape::GC::anchor(_root);
    _root->addObserver(*this);
}

StyleRootWatcher::~StyleRootWatcher() {
    if (!_root) return;
    _root->removeObserver(*this);
    Inkscape::GC::release(_root);
}

void StyleRootWatcher::notifyChildAdded(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node*) {
    if (is_style_element(child)) _on_change();
}

void StyleRootWatcher::notifyChildRemoved(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node*) {
    if (is_style_element(child)) _on_change();
}

void StyleRootWatcher::notifyElementNameChanged(Inkscape::XML::Node&, GQuark, GQuark) {
    _on_change();
}

StyleSubtreeWatcher::StyleSubtreeWatcher(Inkscape::XML::Node* elem, std::function<void()> on_change)
    : _elem(elem)
    , _on_change(std::move(on_change)) {
    if (!_elem) return;
    Inkscape::GC::anchor(_elem);
    _elem->addSubtreeObserver(*this);
}

StyleSubtreeWatcher::~StyleSubtreeWatcher() {
    if (!_elem) return;
    _elem->removeSubtreeObserver(*this);
    Inkscape::GC::release(_elem);
}

void StyleSubtreeWatcher::notifyChildAdded(Inkscape::XML::Node&, Inkscape::XML::Node&, Inkscape::XML::Node*) {
    _on_change();
}

void StyleSubtreeWatcher::notifyChildRemoved(Inkscape::XML::Node&, Inkscape::XML::Node&, Inkscape::XML::Node*) {
    _on_change();
}

void StyleSubtreeWatcher::notifyContentChanged(Inkscape::XML::Node&, Inkscape::Util::ptr_shared,
                                               Inkscape::Util::ptr_shared) {
    _on_change();
}

DocumentStyleSheet::DocumentStyleSheet(std::function<void()> on_change)
    : _on_change(std::move(on_change)) {}

DocumentStyleSheet::~DocumentStyleSheet() = default;

void DocumentStyleSheet::setDocument(SPDocument* document) {
    if (document == _document) return;
    _document = document;
    _subtree.reset();
    _elem = nullptr;
    _root.reset();
    if (_document) {
        _root = std::make_unique<StyleRootWatcher>(_document->getReprRoot(),
                                                 [this] { syncStyleElement(); });
    }
    syncStyleElement();
}

std::string DocumentStyleSheet::text() const {
    if (!_elem) return {};
    return sp_repr_text_content(_elem).raw();
}

void DocumentStyleSheet::setText(std::string const& text) {
    if (!_document) return;
    // Creating the element fires the root watcher synchronously, binding _elem.
    set_style_text(_document, ensure_style_element(_document), text.c_str());
}

std::vector<CustomCSSProperty> DocumentStyleSheet::customProperties() const {
    return root_custom_properties(text());
}

void DocumentStyleSheet::syncStyleElement() {
    auto* elem = first_style_element(_document);
    if (elem != _elem) {
        _subtree.reset();
        _elem = elem;
        if (elem) {
            _subtree = std::make_unique<StyleSubtreeWatcher>(elem, _on_change);
        }
    }
    _on_change();
}

namespace {

// Scanning helpers over raw CSS text; all positions are bounded by `limit`
// so the same code works on the whole stylesheet or a '{...}' block span.

bool is_space(char ch) {
    return std::isspace(static_cast<unsigned char>(ch));
}

void skip_trivia(std::string_view css, size_t& i, size_t limit) {
    while (i < limit) {
        if (is_space(css[i])) {
            ++i;
            continue;
        }
        if (css[i] == '/' && i + 1 < limit && css[i + 1] == '*') {
            i += 2;
            while (i + 1 < limit && !(css[i] == '*' && css[i + 1] == '/')) {
                ++i;
            }
            i = std::min(i + 2, limit);
            continue;
        }
        break;
    }
}

void skip_string(std::string_view css, size_t& i, size_t limit) {
    const char quote = css[i++];
    while (i < limit && css[i] != quote) {
        if (css[i] == '\\') ++i;
        ++i;
    }
    if (i < limit) ++i;
}

void skip_comment(std::string_view css, size_t& i, size_t limit) {
    i += 2;
    while (i + 1 < limit && !(css[i] == '*' && css[i + 1] == '/')) {
        ++i;
    }
    i = std::min(i + 2, limit);
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && is_space(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && is_space(s.back()))
        s.remove_suffix(1);
    return s;
}

// Selector is a comma-separated list; ':root' may appear as one member.
bool selector_has_root(std::string_view selector) {
    while (!selector.empty()) {
        const auto comma = selector.find(',');
        if (trim(selector.substr(0, comma)) == ":root") {
            return true;
        }
        if (comma == std::string_view::npos) break;
        selector.remove_prefix(comma + 1);
    }
    return false;
}

// Content span [start, end) of the first top-level ':root { ... }' rule.
std::optional<std::pair<size_t, size_t>> find_root_block(std::string_view css) {
    const size_t n = css.size();
    size_t i = 0;
    while (i < n) {
        const size_t prelude_start = i;
        char term = 0;
        while (i < n && term == 0) {
            const char ch = css[i];
            if (ch == '"' || ch == '\'') {
                skip_string(css, i, n);
            } else if (ch == '/' && i + 1 < n && css[i + 1] == '*') {
                skip_comment(css, i, n);
            } else if (ch == '{' || ch == ';' || ch == '}') {
                term = ch;
            } else {
                ++i;
            }
        }
        const auto prelude = trim(css.substr(prelude_start, i - prelude_start));
        if (term == '{') {
            ++i;
            const size_t content_start = i;
            int depth = 1;
            while (i < n && depth > 0) {
                const char ch = css[i];
                if (ch == '"' || ch == '\'') {
                    skip_string(css, i, n);
                } else if (ch == '/' && i + 1 < n && css[i + 1] == '*') {
                    skip_comment(css, i, n);
                } else {
                    if (ch == '{') ++depth;
                    if (ch == '}') --depth;
                    ++i;
                }
            }
            const size_t content_end = depth == 0 ? i - 1 : n;
            if (selector_has_root(prelude)) {
                return std::pair{content_start, content_end};
            }
        } else if (term != 0) {
            ++i;
        }
    }
    return {};
}

// A declaration inside a ':root { ... }' block, with source positions.
struct RootDecl {
    std::string name;
    size_t name_start;
    size_t value_start;
    size_t value_end;
    size_t end; // just past the terminating ';' (or decl end)
};

std::vector<RootDecl> parse_root_decls(std::string_view css, size_t start, size_t end) {
    std::vector<RootDecl> decls;
    size_t i = start;
    auto skip_statement = [&] {
        while (i < end && css[i] != ';')
            ++i;
        if (i < end) ++i;
    };
    while (i < end) {
        skip_trivia(css, i, end);
        if (i >= end) break;

        const size_t name_start = i;
        const char first = css[i];
        if (!std::isalpha(static_cast<unsigned char>(first)) && first != '-' && first != '_' && first != '\\') {
            skip_statement();
            continue;
        }
        while (i < end) {
            const char ch = css[i];
            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_' && ch != '\\') {
                break;
            }
            ++i;
        }
        const std::string name(css.substr(name_start, i - name_start));
        skip_trivia(css, i, end);
        if (i >= end || css[i] != ':') {
            skip_statement();
            continue;
        }
        ++i;
        skip_trivia(css, i, end);
        const size_t value_start = i;
        int depth = 0;
        while (i < end) {
            const char ch = css[i];
            if (ch == '"' || ch == '\'') {
                skip_string(css, i, end);
            } else if (ch == '/' && i + 1 < end && css[i + 1] == '*') {
                skip_comment(css, i, end);
            } else {
                if (ch == ';' && depth == 0) break;
                if (ch == '(' || ch == '{' || ch == '[') {
                    ++depth;
                } else if ((ch == ')' || ch == '}' || ch == ']') && depth > 0) {
                    --depth;
                }
                ++i;
            }
        }
        size_t value_end = i;
        while (value_end > value_start && is_space(css[value_end - 1])) {
            --value_end;
        }
        if (i < end && css[i] == ';') ++i;
        decls.push_back({std::move(name), name_start, value_start, value_end, i});
    }
    return decls;
}

const RootDecl* find_decl(const std::vector<RootDecl>& decls, std::string_view name) {
    for (const auto& decl : decls) {
        if (decl.name == name) {
            return &decl;
        }
    }
    return nullptr;
}

} // namespace

std::optional<std::string> normalize_custom_property_name(std::string_view input) {
    const auto trimmed = trim(input);
    std::string name;
    if (trimmed.starts_with("--")) {
        name = trimmed;
    } else if (trimmed.starts_with('-')) {
        name = '-';
        name += trimmed;
    } else {
        name = "--";
        name += trimmed;
    }
    // Runs of whitespace collapse to a single '-'.
    for (size_t i = 2; i < name.size(); ++i) {
        if (is_space(name[i])) {
            name[i] = '-';
            if (name[i - 1] == '-') {
                name.erase(i, 1);
                --i;
            }
        }
    }
    // A custom property name is "--" followed by an identifier, which
    // must not start with a digit.
    if (name.size() <= 2 || std::isdigit(static_cast<unsigned char>(name[2]))) {
        return {};
    }
    for (size_t i = 2; i < name.size(); ++i) {
        const char ch = name[i];
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_') {
            return {};
        }
    }
    return name;
}

std::vector<CustomCSSProperty> root_custom_properties(std::string_view css) {
    std::vector<CustomCSSProperty> props;
    const auto block = find_root_block(css);
    if (!block) {
        return props;
    }
    for (const auto& decl : parse_root_decls(css, block->first, block->second)) {
        if (decl.name.starts_with("--")) {
            props.push_back({decl.name, std::string(css.substr(decl.value_start, decl.value_end - decl.value_start))});
        }
    }
    return props;
}

bool set_root_custom_property(std::string& css, std::string_view name, std::string_view value) {
    const auto block = find_root_block(css);
    if (!block) {
        return false;
    }
    const auto decls = parse_root_decls(css, block->first, block->second);
    const auto decl = find_decl(decls, name);
    if (!decl) {
        return false;
    }
    css.replace(decl->value_start, decl->value_end - decl->value_start, value);
    return true;
}

bool rename_root_custom_property(std::string& css, std::string_view old_name, std::string_view new_name) {
    const auto block = find_root_block(css);
    if (!block) {
        return false;
    }
    const auto decls = parse_root_decls(css, block->first, block->second);
    const auto decl = find_decl(decls, old_name);
    if (!decl || find_decl(decls, new_name)) {
        return false;
    }
    css.replace(decl->name_start, decl->name.size(), new_name);
    return true;
}

void add_root_custom_property(std::string& css, std::string_view name, std::string_view value) {
    const auto block = find_root_block(css);
    if (block) {
        // Insert before the closing brace, on its own line.
        const bool ends_with_newline = block->second > block->first && css[block->second - 1] == '\n';
        std::string ins = ends_with_newline ? "" : "\n";
        ins += "  ";
        ins += name;
        ins += ": ";
        ins += value;
        ins += ";\n";
        css.insert(block->second, ins);
        return;
    }

    if (!css.empty() && css.back() != '\n') {
        css += '\n';
    }
    css += ":root {\n  ";
    css += name;
    css += ": ";
    css += value;
    css += ";\n}\n";
}

bool remove_root_custom_property(std::string& css, std::string_view name) {
    const auto block = find_root_block(css);
    if (!block) {
        return false;
    }
    const auto decls = parse_root_decls(css, block->first, block->second);
    const auto decl = find_decl(decls, name);
    if (!decl) {
        return false;
    }

    // Remove the declaration, any inline whitespace following it, and the
    // newline introducing its line if it sits alone on one.
    size_t start = decl->name_start;
    size_t end = decl->end;
    while (end < css.size() && (css[end] == ' ' || css[end] == '\t')) {
        ++end;
    }
    while (start > 0 && (css[start - 1] == ' ' || css[start - 1] == '\t')) {
        --start;
    }
    if (start > 0 && css[start - 1] == '\n') {
        --start;
        if (start > 0 && css[start - 1] == '\r') {
            --start;
        }
    }
    css.erase(start, end - start);
    return true;
}

} // namespace Inkscape::Css
