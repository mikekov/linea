// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Completion support for ConsoleWidget.
 */

#include "console-completer.h"

#include <QFileInfo>

#include "command-interpreter.h"

namespace Linea::UI {

CommandCompleter::CommandCompleter(QObject* parent)
    : ConsoleCompleter(parent)
    , _workDir(QDir::current()) {
    setModel(&_model);
    setCaseSensitivity(Qt::CaseInsensitive);
    setCompletionMode(QCompleter::PopupCompletion);
}

CommandCompleter::CommandCompleter(const QStringList& keywords, QObject* parent)
    : CommandCompleter(parent) {
    setKeywords(keywords);
}

void CommandCompleter::setKeywords(const QStringList& keywords) {
    _keywords = keywords;
    _keywords.removeDuplicates();
    _keywords.sort(Qt::CaseInsensitive);
}

int CommandCompleter::updateCompletionModel(const QString& commandText) {
    // complete the last whitespace-delimited token before the cursor
    const int tokenStart =
        std::max(commandText.lastIndexOf(u' '), commandText.lastIndexOf(u'\t')) + 1;
    const QString token = commandText.mid(tokenStart);
    _insertPos = tokenStart;

    const bool isCommandToken = (tokenStart == 0);
    const bool looksLikePath =
        token.contains(u'/') || token.startsWith(u'~') || token.startsWith(u'.');

    QStringList candidates;
    if (isCommandToken && !looksLikePath) {
        for (const QString& keyword : _keywords) {
            if (keyword.startsWith(token, Qt::CaseInsensitive)) {
                candidates.append(keyword);
            }
        }
    } else if (_fileCompletion) {
        candidates = fileCompletions(token);
    }

    _model.setStringList(candidates);
    setCompletionPrefix(token);
    return candidates.size();
}

QStringList CommandCompleter::fileCompletions(const QString& token) const {
    // split the token into the directory part (as typed) and the name prefix
    const int slash = token.lastIndexOf(u'/');
    const QString typedDir = slash >= 0 ? token.left(slash) : QString();
    const QString namePrefix = token.mid(slash + 1);

    QString dirPath = typedDir;
    if (dirPath.startsWith(u'~')) {
        dirPath = QDir::homePath() + dirPath.mid(1);
    }

    QDir dir;
    if (dirPath.isEmpty()) {
        dir = _workDir;
    } else if (QDir::isAbsolutePath(dirPath)) {
        dir = QDir(dirPath);
    } else {
        dir = QDir(_workDir.absoluteFilePath(dirPath));
    }

    QStringList result;
    const auto entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                                           QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& entry : entries) {
        const QString name = entry.fileName();
        if (!name.startsWith(namePrefix, Qt::CaseInsensitive)) {
            continue;
        }
        // the completion replaces the whole token, so reattach the typed dir part
        QString completion = typedDir.isEmpty() ? name : typedDir + u'/' + name;
        if (entry.isDir()) {
            completion += u'/';
        }
        result.append(completion);
    }
    return result;
}

InterpreterCompleter::InterpreterCompleter(Linea::CommandInterpreter* interpreter,
                                           LineaApplication* app, QObject* parent)
    : ConsoleCompleter(parent)
    , _interpreter(interpreter)
    , _app(app) {
    setModel(&_model);
    setCompletionMode(QCompleter::PopupCompletion);
}

int InterpreterCompleter::updateCompletionModel(const QString& commandText) {
    _insertPos = 0;

    QStringList candidates;
    if (_interpreter) {
        // commandText runs from the start of the input line to the cursor;
        // the interpreter speaks UTF-8 — its byte offsets map back to
        // UTF-16 positions by decoding the prefix
        const std::string utf8 = commandText.toStdString();
        const Linea::CompletionResult result = _interpreter->completions(utf8, _app);
        _insertPos = static_cast<int>(QString::fromUtf8(utf8.data(), result.replaceFrom).size());
        for (const Linea::CompletionItem& item : result.items) {
            candidates += QString::fromStdString(item.completion);
        }
    }

    _model.setStringList(candidates);
    setCompletionPrefix(commandText.mid(_insertPos));
    return candidates.size();
}

} // namespace Linea::UI
