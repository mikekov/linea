// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Helpers for the document's <svg:style> element.
 */

#ifndef LINEA_STYLE_HELPERS_H
#define LINEA_STYLE_HELPERS_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xml/node-observer.h"

class SPDocument;

namespace Inkscape::XML {
class Node;
}

namespace Inkscape::Css {

// True if the node is an <svg:style> element.
bool is_style_element(Inkscape::XML::Node const& node);

// The first <svg:style> child of the document root, or nullptr.
Inkscape::XML::Node* first_style_element(SPDocument* document);

// The first <svg:style> child of the document root, creating it (inserted
// as the root's first child) if absent.
Inkscape::XML::Node* ensure_style_element(SPDocument* document);

// Replace the element's entire textual content: the first text child gets
// the full text and any remaining text children are removed, so the whole
// concatenated content equals the given text. Non-text children (e.g.
// comments) are left in place.
void set_style_text(SPDocument* document, Inkscape::XML::Node* elem, char const* text);

// Watches the document root's direct children; on_change fires when an
// <svg:style> child is added, removed, or renamed to or from something else.
class StyleRootWatcher final : public Inkscape::XML::NodeObserver {
public:
    StyleRootWatcher(Inkscape::XML::Node* root, std::function<void()> on_change);
    ~StyleRootWatcher() override;

private:
    void notifyChildAdded(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node*) override;
    void notifyChildRemoved(Inkscape::XML::Node&, Inkscape::XML::Node& child, Inkscape::XML::Node*) override;
    void notifyElementNameChanged(Inkscape::XML::Node&, GQuark, GQuark) override;

    Inkscape::XML::Node* _root;
    std::function<void()> _on_change;
};

// Watches an <svg:style> element's subtree; on_change fires when text or
// children change (e.g. via the XML editor or undo).
class StyleSubtreeWatcher final : public Inkscape::XML::NodeObserver {
public:
    StyleSubtreeWatcher(Inkscape::XML::Node* elem, std::function<void()> on_change);
    ~StyleSubtreeWatcher() override;

private:
    void notifyChildAdded(Inkscape::XML::Node&, Inkscape::XML::Node&, Inkscape::XML::Node*) override;
    void notifyChildRemoved(Inkscape::XML::Node&, Inkscape::XML::Node&, Inkscape::XML::Node*) override;
    void notifyContentChanged(Inkscape::XML::Node&, Inkscape::Util::ptr_shared,
                              Inkscape::Util::ptr_shared) override;

    Inkscape::XML::Node* _elem;
    std::function<void()> _on_change;
};

// Normalize user input into a CSS custom property name: trims, turns
// whitespace into '-', and supplies the leading "--" if missing. Returns
// std::nullopt when the result is not a valid custom property name
// (empty, digit as the first identifier char, or chars outside
// [a-zA-Z0-9_-]).
std::optional<std::string> normalize_custom_property_name(std::string_view input);

// A custom property declaration inside a ':root' rule.
// A custom property name without the "--" prefix, for user-facing display.
// The CSS-layer naming convention lives here; UI never decodes the prefix itself.
inline std::string custom_property_display_name(std::string_view name) {
    return name.starts_with("--") ? std::string(name.substr(2)) : std::string(name);
}

struct CustomCSSProperty {
    std::string name;   // includes the leading "--"
    std::string value;  // raw, trimmed

    std::string display_name() const { return custom_property_display_name(name); }
};

// Custom properties of the first ':root { ... }' rule in the stylesheet, in order.
std::vector<CustomCSSProperty> root_custom_properties(std::string_view css);

// The CSS var() function call referencing a custom property (name
// includes the "--" prefix).
std::string css_var(std::string_view name);

// Extract the custom property name from a var(--name[, fallback]) token.
// Returns std::nullopt when value (after trimming) is not a var() call.
std::optional<std::string> parse_css_var(std::string_view value);

// Custom properties of the first ':root' rule in the document's first
// <svg:style> element, read directly without a watcher.
std::vector<CustomCSSProperty> document_custom_properties(SPDocument* document);

// Replace the value of a custom property in the first ':root' rule.
// Returns false if the property is not defined there.
bool set_root_custom_property(std::string& css, std::string_view name, std::string_view value);

// Rename a custom property in the first ':root' rule, keeping its position
// and value. Returns false if the old name is not defined there or the new
// name already is.
bool rename_root_custom_property(std::string& css, std::string_view old_name, std::string_view new_name);

// Add a custom property to the first ':root' rule, creating the rule at the
// end of the stylesheet if it does not exist.
void add_root_custom_property(std::string& css, std::string_view name, std::string_view value);

// Remove a custom property from the first ':root' rule.
// Returns false if the property is not defined there.
bool remove_root_custom_property(std::string& css, std::string_view name);

// High-level document model: follows the document's first <svg:style>
// element across its lifetime (created, removed, replaced, edited) and
// fires on_change whenever the stylesheet text may have changed. All
// repr-level plumbing (element lookup, node observers, text extraction)
// stays inside.
class DocumentStyleSheet {
public:
    explicit DocumentStyleSheet(std::function<void()> on_change);
    ~DocumentStyleSheet();

    // Rebind to another document; nullptr detaches. Fires on_change.
    void setDocument(SPDocument* document);

    // The element's full text content, or "" when it does not exist.
    std::string text() const;

    // Replace the element's text content, creating the element lazily.
    void setText(std::string const& text);

    // Custom properties of the first ':root' rule, in order.
    std::vector<CustomCSSProperty> customProperties() const;

private:
    void syncStyleElement();

    SPDocument* _document = nullptr;
    Inkscape::XML::Node* _elem = nullptr;
    std::unique_ptr<StyleRootWatcher> _root;
    std::unique_ptr<StyleSubtreeWatcher> _subtree;
    std::function<void()> _on_change;
};

} // namespace Inkscape::Css

#endif // LINEA_STYLE_HELPERS_H
