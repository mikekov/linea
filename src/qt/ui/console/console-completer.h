// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Completion support for ConsoleWidget.
 *
 * ConsoleCompleter is the abstract interface the console widget talks to;
 * CommandCompleter completes the first token of a command line against a
 * keyword list and subsequent tokens against the file system.
 *
 * Based on QConsoleWidgetCompleter from QConsoleWidget
 * (https://github.com/gapost/qconsolewidget, MIT-licensed).
 */

#ifndef LINEA_UI_CONSOLE_CONSOLE_COMPLETER_H
#define LINEA_UI_CONSOLE_CONSOLE_COMPLETER_H

#include <QCompleter>
#include <QDir>
#include <QStringList>
#include <QStringListModel>

class LineaApplication;

namespace Linea {
class CommandInterpreter;
}

namespace Linea::UI {

/**
 * Base class for ConsoleWidget completion.
 *
 * The console widget hands the whole input line (from the start of the
 * editable zone to the cursor) to updateCompletionModel() and shows the
 * completer popup if it returns a non-zero count. On activation, the text
 * between insertPos() and the cursor is replaced by the completion.
 */
class ConsoleCompleter : public QCompleter {
    Q_OBJECT

public:
    using QCompleter::QCompleter;

    /**
     * Update the completion model for the given command-line text
     * (the text between the start of the editable zone and the cursor).
     * Returns the number of available completions.
     */
    virtual int updateCompletionModel(const QString& commandText) = 0;

    /**
     * Offset within the command-line text where a completion is inserted;
     * text between insertPos() and the cursor is replaced.
     */
    virtual int insertPos() const = 0;
};

/**
 * Completes the first token of a command line against a keyword list.
 * Subsequent tokens (and tokens that look like paths) are completed
 * against the file system when file completion is enabled.
 */
class CommandCompleter : public ConsoleCompleter {
    Q_OBJECT

public:
    explicit CommandCompleter(QObject* parent = nullptr);
    explicit CommandCompleter(const QStringList& keywords, QObject* parent = nullptr);

    // candidates for the first token (command names)
    void setKeywords(const QStringList& keywords);
    QStringList keywords() const { return _keywords; }

    // complete argument tokens as filesystem paths (default: on)
    void setFileCompletionEnabled(bool enabled) { _fileCompletion = enabled; }
    bool fileCompletionEnabled() const { return _fileCompletion; }

    // base directory for relative path completion (default: CWD)
    void setWorkingDirectory(const QString& dir) { _workDir = QDir(dir); }
    QString workingDirectory() const { return _workDir.path(); }

    int updateCompletionModel(const QString& commandText) override;
    int insertPos() const override { return _insertPos; }

private:
    QStringList fileCompletions(const QString& token) const;

    QStringListModel _model;
    QStringList _keywords;
    QDir _workDir;
    int _insertPos = 0;
    bool _fileCompletion = true;
};

/**
 * Adapts a CommandInterpreter's context-aware completions() to the
 * console widget. The interpreter owns all completion logic; this
 * class only converts CompletionResult into the completer model.
 */
class InterpreterCompleter : public ConsoleCompleter {
    Q_OBJECT

public:
    InterpreterCompleter(Linea::CommandInterpreter* interpreter, LineaApplication* app,
                         QObject* parent = nullptr);

    int updateCompletionModel(const QString& commandText) override;
    int insertPos() const override { return _insertPos; }

private:
    Linea::CommandInterpreter* _interpreter;
    LineaApplication* _app;
    QStringListModel _model;
    int _insertPos = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_CONSOLE_CONSOLE_COMPLETER_H
