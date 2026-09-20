// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Concrete command interpreter for the console.
 *
 * A dispatcher: commands are described by a CommandDef metadata table
 * (see builtin-commands.cpp) — name, help text, handler, and an optional
 * per-command completer. Command names complete on the first token;
 * argument completion is delegated to the command's completer.
 *
 * Additionally, every action in the action registry is also a command.
 */

#ifndef LINEA_INTERPRETER_H
#define LINEA_INTERPRETER_H

#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <2geom/point.h>

#include "command-args.h"
#include "command-interpreter.h"

namespace Linea {

class Interpreter : public CommandInterpreter {
    Q_OBJECT

public:
    /// Handlers emit output through the interpreter (printOut/printErr).
    /// `args` holds the values captured by the command's spec; for
    /// spec-less commands it carries raw tokens and remainder text.
    using Handler = std::function<void(Interpreter& interp, const ParsedArgs& args, LineaApplication* app)>;

    /// Per-command argument completion; sees the segment up to the cursor.
    using Completer = std::function<CompletionResult(std::string_view input, LineaApplication* app)>;

    /// Static command description, as declared in command tables.
    struct CommandDef {
        const char* name;
        const char* help;
        void (*handler)(Interpreter& interp, const ParsedArgs& args, LineaApplication* app);
        ArgSpec spec = nullptr;
        CompletionResult (*completer)(std::string_view input, LineaApplication* app) = nullptr;
    };

    explicit Interpreter(QObject* parent = nullptr);

    void registerCommand(std::string name, std::string help, Handler handler, ArgSpec spec = {},
                         Completer completer = {});
    void registerCommands(std::span<const CommandDef> commands);

    /// Registered commands, for `help` and completion implementations.
    std::vector<std::string> commandNames() const;
    std::string commandHelp(std::string_view name) const;

    /// Emit output on the interpreter's channels (for use from handlers).
    /// UTF-8; converted to QString at the signal boundary.
    void printOut(std::string_view text) { stdOut(QString::fromUtf8(text.data(), text.size())); }
    void printErr(std::string_view text) { stdErr(QString::fromUtf8(text.data(), text.size())); }

    /// Start offset of the token at the end of input (a quote begins a token).
    static size_t tokenStart(std::string_view input);

    /// Completion for the current token as a filesystem path.
    static CompletionResult completeFilePath(std::string_view input, LineaApplication* app);

    CompletionResult completions(std::string_view input, LineaApplication* app) const override;
    void evaluate(std::string_view command, LineaApplication* app) override;
    void abort() override;

    /// True between abort() and the start of the next evaluate().
    bool abortRequested() const { return _abortRequested; }

    /// Default x/y for shape commands, set by `location`.
    Geom::Point location() const { return _location.value_or(Geom::Point(0, 0)); }
    void setLocation(const Geom::Point& p) { _location = p; }

private:
    struct Command {
        std::string help;
        Handler handler;
        ArgSpec spec;
        Completer completer;
    };

    /// Runs a single command (no ';' splitting); false on any failure.
    bool evaluateOne(std::string_view command, LineaApplication* app);

    // std::less<> enables heterogeneous find() on std::string_view names
    std::map<std::string, Command, std::less<>> _commands;
    std::atomic<bool> _abortRequested{false};
    std::optional<Geom::Point> _location;
};

/// Table of built-in commands, defined in builtin-commands.cpp.
std::span<const Interpreter::CommandDef> builtinCommands();

} // namespace Linea

#endif // LINEA_INTERPRETER_H
