// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Concrete command interpreter for the console — a dispatcher over
 * commands described by CommandDef tables (see builtin-commands.cpp)
 * and actions from the action registry.
 */

#include "interpreter.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <algorithm>
#include <cctype>
#include <format>

#include "actions/action-registry.h"

namespace Linea {

// Expand nested @x{...} markup in help text to SGR sequences (ANSI escapes),
// scanning UTF-8 bytes — '@', '{', '}' are ASCII so multibyte sequences pass
// through untouched. After an inner group closes, the still-active outer
// codes are re-emitted since \x1b[0m resets everything.
static int markupCode(char letter) {
    switch (letter) {
        case 'b':
            return 1; // bold
        case 'd':
            return 2; // dim
        case 'i':
            return 3; // italic
        case 'v':
            return 7; // inverse
        case 'u':
            return 4; // underline
    }
    return 0;
}

static void expandMarkup(std::string_view text, size_t& pos, bool inGroup, std::vector<int>& active, std::string& out) {
    while (pos < text.size()) {
        const char c = text[pos++];
        if (c == '}' && inGroup) {
            out += "\x1b[0m";
            return;
        }
        const int code = c == '@' && pos + 1 < text.size() && text[pos + 1] == '{' ? markupCode(text[pos]) : 0;
        if (code) {
            pos += 2;
            out += "\x1b[" + std::to_string(code) + 'm';
            active.push_back(code);
            expandMarkup(text, pos, true, active, out);
            active.pop_back();
            for (const int outer : active) {
                out += "\x1b[" + std::to_string(outer) + 'm';
            }
            continue;
        }
        out += c;
    }
    if (inGroup) {
        out += "\x1b[0m"; // unclosed group — applies to end of text
    }
}

static std::string expandHelpMarkup(std::string_view text) {
    std::string out;
    std::vector<int> active;
    size_t pos = 0;
    expandMarkup(text, pos, false, active, out);
    return out;
}

size_t Interpreter::tokenStart(std::string_view input) {
    size_t pos = input.size();
    while (pos > 0 && !args::isSpace(input[pos - 1]) &&
           input[pos - 1] != '"' && input[pos - 1] != '\'') {
        --pos;
    }
    return pos;
}

Interpreter::Interpreter(QObject* parent)
    : CommandInterpreter(parent) {
    registerCommands(builtinCommands());
}

void Interpreter::registerCommand(std::string name, std::string help, Handler handler, ArgSpec spec,
                                  Completer completer) {
    if (name.empty() || !handler) {
        return;
    }
    _commands[std::move(name)] = {std::move(help), std::move(handler), std::move(spec), std::move(completer)};
}

void Interpreter::registerCommands(std::span<const CommandDef> commands) {
    for (const CommandDef& def : commands) {
        registerCommand(def.name, expandHelpMarkup(def.help), def.handler, def.spec, def.completer);
    }
}

std::vector<std::string> Interpreter::commandNames() const {
    std::vector<std::string> names; // _commands is ordered — already sorted
    names.reserve(_commands.size());
    for (const auto& [name, _] : _commands) {
        names.push_back(name);
    }
    return names;
}

std::string Interpreter::commandHelp(std::string_view name) const {
    const auto it = _commands.find(name);
    return it == _commands.end() ? std::string() : it->second.help;
}

CompletionResult Interpreter::completions(std::string_view input, LineaApplication* app) const {
    // a ';' starts a fresh command — complete within the last segment
    const std::string_view seg = args::splitCommands(input).back();
    const size_t offset = seg.data() - input.data();
    const size_t start = tokenStart(seg);

    // first token: complete command names
    if (std::all_of(seg.begin(), seg.begin() + start, args::isSpace)) {
        const std::string_view prefix = seg.substr(start);

        CompletionResult result;
        result.replaceFrom = static_cast<int>(offset + start);
        result.replaceTo = static_cast<int>(input.size());
        for (const auto& [cmd, def] : _commands) {
            if (cmd.starts_with(prefix)) {
                result.items.push_back({cmd, def.help});
            }
        }
        // every registered action is also a command
        auto& registry = ActionRegistry::get();
        for (const std::string& id : registry.actionIds()) {
            if (id.starts_with(prefix) && !_commands.contains(id)) {
                result.items.push_back({id, registry.action(id)->toolTip().toStdString()});
            }
        }
        return result;
    }

    // argument token: delegate to the command's completer
    const auto tokens = args::splitTokens(seg);
    const auto it = tokens.empty() ? _commands.end() : _commands.find(tokens.front());
    if (it == _commands.end() || !it->second.completer) {
        return {};
    }
    CompletionResult result = it->second.completer(seg, app);
    result.replaceFrom += offset;
    result.replaceTo += offset;
    return result;
}

void Interpreter::evaluate(std::string_view command, LineaApplication* app) {
    _abortRequested = false;

    // a command may tear down the console's owner (e.g. `quit` destroys the
    // window); QPointer tracks whether this interpreter is still alive
    const QPointer<Interpreter> guard(this);

    // several commands per line, split on ';' — stop at the first failure
    for (const std::string_view segment : args::splitCommands(command)) {
        if (!guard || _abortRequested || !evaluateOne(segment, app)) {
            break;
        }
    }

    if (guard) {
        evaluationFinished();
    }
}

bool Interpreter::evaluateOne(std::string_view command, LineaApplication* app) {
    const QPointer<Interpreter> guard(this);

    // command name = first non-space run; the spec sees the raw remainder
    const auto [name, rest] = args::splitFirst(command);

    if (!app) {
        printErr("no application context\n");
        return false;
    }
    if (name.empty()) {
        return true;
    }

    const auto it = _commands.find(name);
    if (it != _commands.end()) {
        ParsedArgs parsed;
        parsed.rest = rest;
        parsed.tokens = args::splitTokens(rest);
        std::string error;
        if (it->second.spec && !it->second.spec(rest, parsed, error)) {
            printErr(error.empty() ? "usage: " + it->second.help + "\n" : error + "\n");
            return false;
        }
        try {
            it->second.handler(*this, parsed, app);
        } catch (const std::exception& e) {
            if (guard) {
                printErr(std::format("{}: {}\n", name, e.what()));
            }
            return false;
        } catch (...) {
            if (guard) {
                printErr(std::format("{}: failed\n", name));
            }
            return false;
        }
        return true;
    }

    // fall back to the action registry: every action is a command;
    // registered commands take precedence on name collisions
    auto& registry = ActionRegistry::get();
    const std::string id{name};
    if (registry.hasAction(id)) {
        registry.action(id)->trigger();
        return true;
    }
    printErr(std::format("unknown command: {}\n", name));
    return false;
}

void Interpreter::abort() {
    _abortRequested = true;
}

CompletionResult Interpreter::completeFilePath(std::string_view input, LineaApplication* /*app*/) {
    const size_t start = tokenStart(input);
    const std::string_view token = input.substr(start);

    const size_t slash = token.rfind('/');
    const std::string_view dirPart = slash == std::string_view::npos ? "" : token.substr(0, slash + 1);
    const std::string_view base = slash == std::string_view::npos ? token : token.substr(slash + 1);

    // QDir is QString territory — the token crosses the boundary here
    QString dirName = QString::fromUtf8(dirPart.data(), dirPart.size());
    if (dirName.startsWith(u'~')) {
        dirName = QDir::homePath() + dirName.mid(1);
    }
    if (dirName.isEmpty()) {
        dirName = QStringLiteral(".");
    }

    CompletionResult result;
    result.replaceFrom = static_cast<int>(start);
    result.replaceTo = static_cast<int>(input.size());

    const QDir dir(dirName);
    const QFileInfoList entries =
        dir.entryInfoList({QString::fromUtf8(base.data(), base.size()) + u'*'},
                          QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
                          QDir::Name | QDir::DirsFirst);
    for (const QFileInfo& entry : entries) {
        result.items.push_back({std::string(dirPart) + entry.fileName().toStdString() + (entry.isDir() ? "/" : ""),
                                entry.isDir() ? "dir" : "file"});
    }
    return result;
}

} // namespace Linea
