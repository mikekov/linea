// SPDX-License-Identifier: GPL-2.0-or-later
#include "script-registry.h"

namespace Linea::Script {

bool Registry::registerScript(std::string name, std::string text) {
    if (name.empty()) {
        return false;
    }

    _scripts[std::move(name)] = std::move(text);
    return true;
}

bool Registry::removeScript(std::string_view name) {
    const auto it = _scripts.find(name);
    if (it == _scripts.end()) {
        return false;
    }

    _scripts.erase(it);
    if (_current == name) {
        _current.clear();
    }
    return true;
}

bool Registry::setCurrent(std::string name, std::string text) {
    if (name.empty()) {
        return false;
    }

    _current = std::move(name);
    _scripts[_current] = std::move(text);
    return true;
}

std::optional<Source> Registry::find(std::string_view name) const {
    const auto it = _scripts.find(name);
    if (it == _scripts.end()) {
        return std::nullopt;
    }

    return Source{it->first, it->second};
}

std::optional<Source> Registry::current() const {
    if (_current.empty()) {
        return std::nullopt;
    }
    return find(_current);
}

std::vector<std::string> Registry::names() const {
    std::vector<std::string> result;
    result.reserve(_scripts.size());
    for (const auto& [name, text] : _scripts) {
        result.push_back(name);
    }
    return result;
}

} // namespace Linea::Script
