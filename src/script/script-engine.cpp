// SPDX-License-Identifier: GPL-2.0-or-later
#include "script-engine.h"

#include <algorithm>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <lua.hpp>

#include "script-bindings.h"

namespace Linea::Script {
namespace {

Engine* engineFor(lua_State* state) {
    return *static_cast<Engine**>(lua_getextraspace(state));
}

std::pair<int, int> errorLocation(const std::string& message) {
    static const std::regex pattern(R"(:([0-9]+)(?::([0-9]+))?:)");
    std::smatch match;
    if (!std::regex_search(message, match, pattern)) {
        return {0, 0};
    }

    return {std::stoi(match[1].str()), match[2].matched ? std::stoi(match[2].str()) : 0};
}

} // namespace

Engine::Engine(LineaApplication* app)
    : _state(luaL_newstate())
    , _app(app) {
    if (!_state) {
        throw std::runtime_error("unable to create script state");
    }

    *static_cast<Engine**>(lua_getextraspace(_state)) = this;
    initialize();
}

Engine::~Engine() {
    if (_state) {
        lua_close(_state);
    }
}

void Engine::initialize() {
    luaL_requiref(_state, "_G", luaopen_base, 1);
    lua_pop(_state, 1);
    luaL_requiref(_state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(_state, 1);
    luaL_requiref(_state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(_state, 1);
    luaL_requiref(_state, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(_state, 1);
    luaL_requiref(_state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(_state, 1);

    lua_pushcfunction(_state, &Engine::print);
    lua_setglobal(_state, "print");
    lua_sethook(_state, &Engine::hook, LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
    registerBindings(_state, _app, this);
}

void Engine::setApplication(LineaApplication* app) {
    _app = app;
    registerBindings(_state, _app, this);
}

void Engine::hook(lua_State* state, lua_Debug* debug) {
    if (auto engine = engineFor(state)) {
        engine->handleHook(debug);
    }
}

int Engine::print(lua_State* state) {
    auto engine = engineFor(state);
    if (!engine || !engine->_output) {
        return 0;
    }

    std::ostringstream output;
    const int count = lua_gettop(state);
    for (int i = 1; i <= count; ++i) {
        if (i > 1) {
            output << '\t';
        }
        size_t length = 0;
        const char* text = luaL_tolstring(state, i, &length);
        output.write(text, static_cast<std::streamsize>(length));
        lua_pop(state, 1);
    }
    output << '\n';
    engine->_output(output.str());
    return 0;
}

void Engine::handleHook(lua_Debug* debug) {
    if (_abortRequested) {
        luaL_error(_state, "script aborted");
        return;
    }

    switch (debug->event) {
        case LUA_HOOKCALL:
            ++_callDepth;
            emit(EventKind::Call, _source, debug->currentline);
            return;
        case LUA_HOOKRET:
        case LUA_HOOKTAILCALL:
            emit(EventKind::Return, _source, debug->currentline);
            _callDepth = std::max(0, _callDepth - 1);
            return;
        case LUA_HOOKLINE:
            emit(EventKind::Line, _source, debug->currentline);
            if (_breakpoints.contains({_source, debug->currentline})) {
                emit(EventKind::Paused, _source, debug->currentline);
            }
            if (_mode == RunMode::Steps && --_steps <= 0) {
                emit(EventKind::Paused, _source, debug->currentline);
            }
            return;
        default:
            return;
    }
}

void Engine::emit(EventKind kind, std::string_view source, int line) {
    if (_events) {
        _events(Event{kind, std::string(source), line, _callDepth});
    }
}

Error Engine::makeError(ErrorKind kind, std::string message, std::string_view source) const {
    auto [line, column] = errorLocation(message);
    return Error{kind, std::move(message), line, column};
}

Error Engine::evaluate(const Source& source, Output output, EventCallback events) {
    _abortRequested = false;
    _modified = false;
    _callDepth = 0;
    _source = source.name;
    _output = std::move(output);
    _events = std::move(events);
    emit(EventKind::Started, _source);

    if (luaL_loadbufferx(_state, source.text.data(), source.text.size(), source.name.c_str(), nullptr) != LUA_OK) {
        std::string message = lua_tostring(_state, -1) ? lua_tostring(_state, -1) : "script parse failed";
        lua_pop(_state, 1);
        emit(EventKind::Failed, _source);
        return makeError(ErrorKind::Parse, std::move(message), _source);
    }

    if (lua_pcall(_state, 0, LUA_MULTRET, 0) != LUA_OK) {
        std::string message = lua_tostring(_state, -1) ? lua_tostring(_state, -1) : "script failed";
        lua_pop(_state, 1);
        if (_abortRequested) {
            emit(EventKind::Aborted, _source);
            return makeError(ErrorKind::Cancelled, std::move(message), _source);
        }
        emit(EventKind::Failed, _source);
        return makeError(ErrorKind::Runtime, std::move(message), _source);
    }

    emit(EventKind::Finished, _source);
    return {};
}

void Engine::abort() {
    _abortRequested = true;
}

void Engine::setMode(RunMode mode, int steps) {
    _mode = mode;
    _steps = steps;
}

void Engine::addBreakpoint(std::string source, int line) {
    if (line > 0) {
        _breakpoints.emplace(std::move(source), line);
    }
}

void Engine::removeBreakpoint(std::string_view source, int line) {
    _breakpoints.erase({std::string(source), line});
}

void Engine::clearBreakpoints() {
    _breakpoints.clear();
}

} // namespace Linea::Script
