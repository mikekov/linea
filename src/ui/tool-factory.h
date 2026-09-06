// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Factory for ToolBase tree
 *
 * Authors:
 *   Markus Engel
 *
 * Copyright (C) 2013 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef TOOL_FACTORY_SEEN
#define TOOL_FACTORY_SEEN

#include <string>

class SPDesktop;
namespace Inkscape {
namespace UI {
namespace Tools {
class ToolBase;
}
}
}

namespace ToolFactory {
    Inkscape::UI::Tools::ToolBase* createObject(SPDesktop* desktop, const std::string& tool_name);
};

#endif
