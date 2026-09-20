// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Command interpreter interface for the console.
 *
 * An interpreter evaluates command lines against a LineaApplication
 * instance, which resolves the active desktop, document, and selection.
 * Implementations produce output via the stdOut/stdErr signals and emit
 * evaluationFinished() when the console may show the next prompt.
 */

#ifndef LINEA_COMMAND_INTERPRETER_H
#define LINEA_COMMAND_INTERPRETER_H

#include <QObject>
#include <QString>
#include <string>
#include <string_view>
#include <vector>

class LineaApplication;

namespace Linea {

/// A single completion candidate.
struct CompletionItem {
    std::string completion; ///< text inserted when the candidate is chosen
    std::string detail;     ///< optional extra info (type, description)
};

/// Completion candidates for an input line and the range they replace.
/// Offsets are byte offsets into the UTF-8 input passed to completions().
struct CompletionResult {
    int replaceFrom = 0; ///< start offset in the input line
    int replaceTo = 0;   ///< end offset in the input line
    std::vector<CompletionItem> items;
};

class CommandInterpreter : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    /**
     * Context-aware completion: sees the input line up to the cursor, and
     * the application (active selection, document, ...). Called only on
     * explicit user request (Tab), never per keystroke.
     */
    virtual CompletionResult completions(std::string_view input, LineaApplication* app) const {
        return {};
    }

    /**
     * Evaluate a command line in the context of the application.
     * app->get_active_desktop()/get_active_selection()/get_active_document()
     * resolve the current context; app may have no active desktop.
     */
    virtual void evaluate(std::string_view command, LineaApplication* app) = 0;

    /// Cooperative cancel; wired to the console's Ctrl-C.
    virtual void abort() {}

Q_SIGNALS:
    void stdOut(const QString& text);
    void stdErr(const QString& text);
    void evaluationFinished();
};

} // namespace Linea

#endif // LINEA_COMMAND_INTERPRETER_H
