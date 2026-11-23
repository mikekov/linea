// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RecentColors — a per-document most-recently-used color list.
 */

#include "recent-colors.h"

#include <algorithm>
#include <sstream>

#include "document.h"
#include "style-helpers.h"
#include "xml/node.h"

namespace Linea {

namespace {

// Attribute on sodipodi:namedview holding the list, entries joined by
// ';'. A swatch serializes as its var() reference, a flat color as its
// opaque string form — both are the entry's _key().
constexpr char recent_colors_attr[] = "inkscape:recent-colors";

std::string_view trim_token(std::string_view token) {
    const auto begin = token.find_first_not_of(" \t\n\r");
    if (begin == std::string_view::npos) {
        return {};
    }
    return token.substr(begin, token.find_last_not_of(" \t\n\r") - begin + 1);
}

} // namespace

RecentColors::RecentColors(SPDocument& document)
    : _document(document) {
    auto namedview = document.getReprNamedView();
    if (!namedview) {
        return;
    }
    const auto attr = namedview->attribute(recent_colors_attr);
    if (!attr) {
        return;
    }

    std::istringstream stream(attr);
    std::string token;
    while (_entries.size() < capacity && std::getline(stream, token, ';')) {
        const auto trimmed = trim_token(token);
        if (trimmed.empty()) {
            continue;
        }
        if (auto name = Inkscape::Css::parse_css_var(trimmed)) {
            _entries.emplace_back(std::move(*name));
        } else if (auto color = Inkscape::Colors::Color::parse(std::string(trimmed))) {
            color->enableOpacity(false);
            _entries.emplace_back(std::move(*color));
        }
        // Unparseable tokens are skipped.
    }
}

std::string RecentColors::_key(const Entry& entry) {
    if (const auto color = std::get_if<Inkscape::Colors::Color>(&entry)) {
        return color->toString();
    }
    return Inkscape::Css::css_var(std::get<std::string>(entry));
}

void RecentColors::noteColor(const Inkscape::Colors::Color& color, const char* undo_key) {
    auto flat = color;
    flat.enableOpacity(false);
    _note(std::move(flat), undo_key);
}

void RecentColors::noteSwatch(const Swatch& swatch, const char* undo_key) {
    _note(swatch.cssName(), undo_key);
}

void RecentColors::_note(Entry entry, const char* undo_key) {
    const auto serial = _document.undoCommitSerial();
    if (undo_key && !_entries.empty() && _head_commit && _head_commit->first == serial &&
        _head_commit->second == undo_key) {
        // Same undo step: update the provisional head in place.
        _entries.front() = std::move(entry);
        _signal_changed.emit();
        return;
    }

    _finalizeHead();
    _entries.push_front(std::move(entry));
    if (undo_key) {
        _head_commit.emplace(serial, undo_key);
    } else {
        // A discrete entry is final immediately: dedup and trim now.
        _finalizeHead();
    }
    _signal_changed.emit();
}

void RecentColors::_finalizeHead() {
    if (_entries.empty()) {
        return;
    }
    const auto head = _key(_entries.front());
    for (auto it = std::next(_entries.begin()); it != _entries.end();) {
        if (_key(*it) == head) {
            it = _entries.erase(it);
        } else {
            ++it;
        }
    }
    while (_entries.size() > capacity) {
        _entries.pop_back();
    }
    _head_commit.reset();
}

void RecentColors::renameSwatch(const std::string& old_name, const std::string& new_name) {
    for (auto& entry : _entries) {
        if (auto name = std::get_if<std::string>(&entry); name && *name == old_name) {
            *name = new_name;
        }
    }
    _signal_changed.emit();
}

std::vector<FlatColor> RecentColors::entries() const {
    std::vector<FlatColor> out;
    if (_entries.empty()) {
        return out;
    }

    const auto head = _key(_entries.front());
    bool is_head = true;
    for (const auto& entry : _entries) {
        // Older duplicates of the head are kept but hidden until
        // finalization removes them.
        if (!is_head && _key(entry) == head) {
            continue;
        }
        is_head = false;

        if (const auto color = std::get_if<Inkscape::Colors::Color>(&entry)) {
            out.push_back(*color);
        } else {
            const auto& name = std::get<std::string>(entry);
            if (auto swatch = _document.findSwatch(name)) {
                out.push_back(*swatch);
            }
        }
        if (out.size() >= capacity) {
            break;
        }
    }
    return out;
}

void RecentColors::save(Inkscape::XML::Node& namedview) {
    _finalizeHead();

    std::string value;
    for (auto it = _entries.begin(); it != _entries.end();) {
        if (const auto name = std::get_if<std::string>(&*it)) {
            if (!_document.hasSwatch(*name)) {
                it = _entries.erase(it);
                continue;
            }
        }
        if (!value.empty()) {
            value += ';';
        }
        value += _key(*it);
        ++it;
    }
    namedview.setAttribute(recent_colors_attr, value.empty() ? nullptr : value.c_str());
}

} // namespace Linea
