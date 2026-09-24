// SPDX-License-Identifier: GPL-2.0-or-later

// Wrapper around Lua state migrated from https://github.com/mikekov/ExifPro/blob/master/src/Lua.cpp

#ifndef LINEA_SCRIPT_ENGINE_H
#define LINEA_SCRIPT_ENGINE_H

#include <atomic>
#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "script-error.h"
#include "script-registry.h"

struct lua_State;
struct lua_Debug;
class LineaApplication;

namespace Linea::Script {

class Engine {
public:
    enum class RunMode {
        Run,
        StepInto,
        StepOver,
        StepOut,
        Steps,
    };

    enum class EventKind {
        Started,
        Line,
        Call,
        Return,
        Paused,
        Finished,
        Failed,
        Aborted,
    };

    struct Event {
        EventKind kind;
        std::string source;
        int line = 0;
        int depth = 0;
    };

    using Output = std::function<void(std::string_view)>;
    using EventCallback = std::function<void(const Event&)>;

    explicit Engine(LineaApplication* app = nullptr);
    ~Engine();

    void setApplication(LineaApplication* app);

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Error evaluate(const Source& source, Output output = {}, EventCallback events = {});
    void abort();
    void markModified() { _modified = true; }
    bool modified() const { return _modified; }

    void setMode(RunMode mode, int steps = 0);
    RunMode mode() const { return _mode; }
    void addBreakpoint(std::string source, int line);
    void removeBreakpoint(std::string_view source, int line);
    void clearBreakpoints();

private:
    static void hook(lua_State* state, lua_Debug* debug);
    static int print(lua_State* state);
    void initialize();
    void handleHook(lua_Debug* debug);
    Error makeError(ErrorKind kind, std::string message, std::string_view source) const;
    void emit(EventKind kind, std::string_view source, int line = 0);

    lua_State* _state = nullptr;
    LineaApplication* _app = nullptr;
    std::atomic<bool> _abortRequested{false};
    bool _modified = false;
    RunMode _mode = RunMode::Run;
    int _steps = 0;
    int _callDepth = 0;
    std::string _source;
    Output _output;
    EventCallback _events;
    std::set<std::pair<std::string, int>> _breakpoints;
};

} // namespace Linea::Script

#endif // LINEA_SCRIPT_ENGINE_H
