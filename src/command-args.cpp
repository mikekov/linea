// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * ParsedArgs — named argument values produced by a command's spec.
 */

#include "command-args.h"

namespace Linea {

bool ParsedArgs::has(std::string_view name) const {
    return _values.contains(name);
}

double ParsedArgs::num(std::string_view name, double dflt) const {
    const auto it = _values.find(name);
    if (it == _values.end() || !std::holds_alternative<std::vector<double>>(it->second)) {
        return dflt;
    }
    const auto& list = std::get<std::vector<double>>(it->second);
    return list.empty() ? dflt : list.front();
}

const std::string& ParsedArgs::str(std::string_view name) const {
    static const std::string empty;
    const auto it = _values.find(name);
    if (it == _values.end() || !std::holds_alternative<std::string>(it->second)) {
        return empty;
    }
    return std::get<std::string>(it->second);
}

std::vector<double> ParsedArgs::nums(std::string_view name) const {
    const auto it = _values.find(name);
    if (it == _values.end() || !std::holds_alternative<std::vector<double>>(it->second)) {
        return {};
    }
    return std::get<std::vector<double>>(it->second);
}

void ParsedArgs::put(std::string name, std::string value) {
    _values.insert_or_assign(std::move(name), std::move(value));
}

void ParsedArgs::putNum(const std::string& name, double value) {
    auto it = _values.find(name);
    if (it == _values.end()) {
        it = _values.emplace(name, std::vector<double>{}).first;
    }
    std::get<std::vector<double>>(it->second).push_back(value);
}

} // namespace Linea
