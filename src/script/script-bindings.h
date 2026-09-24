// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef LINEA_SCRIPT_BINDINGS_H
#define LINEA_SCRIPT_BINDINGS_H

struct lua_State;
class LineaApplication;

namespace Linea::Script {
class Engine;

void registerBindings(lua_State* state, LineaApplication* app, Engine* engine);

} // namespace Linea::Script

#endif // LINEA_SCRIPT_BINDINGS_H
