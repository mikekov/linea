// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef LINEA_SCRIPT_REGISTRY_H
#define LINEA_SCRIPT_REGISTRY_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Linea::Script {

struct Source {
    std::string name;
    std::string text;
    bool builtin = false;
};

class Registry {
public:
    bool registerScript(std::string name, std::string text);
    bool removeScript(std::string_view name);
    bool setCurrent(std::string name, std::string text);

    std::optional<Source> find(std::string_view name) const;
    std::optional<Source> current() const;
    std::vector<std::string> names() const;

private:
    std::map<std::string, std::string, std::less<>> _scripts;
    std::string _current;
};

} // namespace Linea::Script

#endif // LINEA_SCRIPT_REGISTRY_H
