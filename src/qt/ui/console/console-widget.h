// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Interactive console widget: read-only output above an editable input line,
 * with command history and optional completion.
 *
 * Ported from QConsoleWidget (https://github.com/gapost/qconsolewidget,
 * MIT-licensed, part of QDaq). Adapted for Qt6/Linea: the QIODevice
 * interface was removed, command history is per-instance, and the widget
 * is in the Linea::UI namespace.
 */

#ifndef LINEA_UI_CONSOLE_CONSOLE_WIDGET_H
#define LINEA_UI_CONSOLE_CONSOLE_WIDGET_H

#include <QColor>
#include <QPlainTextEdit>
#include <QStringList>
#include <QTextCharFormat>

class QContextMenuEvent;
class QKeyEvent;

namespace Linea::UI {

class ConsoleCompleter;

class ConsoleWidget : public QPlainTextEdit {
    Q_OBJECT

public:
    enum class ConsoleMode {
        Input, ///< the last block is an editable input line
        Output ///< the whole document is read-only output
    };

    enum class ConsoleChannel { StandardInput = 0, StandardOutput, StandardError, ChannelCount };

    explicit ConsoleWidget(QWidget* parent = nullptr);

    ConsoleMode mode() const { return _mode; }
    void setMode(ConsoleMode mode);

    // per-channel character format (input/output/error colors)
    QTextCharFormat channelCharFormat(ConsoleChannel channel) const;
    void setChannelCharFormat(ConsoleChannel channel, const QTextCharFormat& fmt);

    // write a formatted message; in Input mode it is inserted above the
    // editable line, in Output mode it is appended. ANSI SGR escape
    // sequences (ESC[...m) are parsed and applied on top of fmt; other
    // control sequences are stripped.
    void write(const QString& message, const QTextCharFormat& fmt);

    // write a prompt and switch to input mode
    void showPrompt(const QString& text);

    // the text in the editable input line
    QString commandLine() const;
    void replaceCommandLine(const QString& text);

    // actual height of one text row as laid out by the document
    // (more accurate than QFontMetrics::lineSpacing)
    int lineHeight() const;

    // command history of this widget, most recent first
    const QStringList& history() const { return _history; }
    void setHistoryLimit(int limit) { _historyLimit = limit; }

    // attach a completer (not owned); nullptr detaches
    void setCompleter(ConsoleCompleter* completer);

    QSize sizeHint() const override { return QSize(600, 400); }
    // QAbstractScrollArea reports ~scroll-bar height; the console may shrink to a single row
    QSize minimumSizeHint() const override { return QSize(0, 0); }

public Q_SLOTS:
    // write to StandardOutput / StandardError
    void writeStdOut(const QString& text);
    void writeStdErr(const QString& text);
    // clear the document, preserving the current mode
    void clearConsole();

Q_SIGNALS:
    // fired when the user hits Return on the input line
    void commandEntered(const QString& command);
    // fired when the user presses Ctrl-Q
    void abortEvaluation();
    // fired when the user presses Escape on the input line (the line is cleared)
    void escapePressed();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void handleReturnKey();
    void handleTabKey();
    void historyMove(bool older);
    void addToHistory(const QString& command);
    void insertCompletion(const QString& completion);
    bool isSelectionInEditZone() const;
    bool isCursorInEditZone() const;
    bool canPaste() const;
    bool canCut() const { return isSelectionInEditZone(); }

    ConsoleMode _mode = ConsoleMode::Output;
    int _inputPos = 0; ///< document position where the editable zone starts

    QStringList _history; ///< most recent first
    int _historyLimit = 100;
    int _historyIndex = -1; ///< -1 = not navigating, otherwise index into _history
    QString _historyToken;  ///< prefix typed before history navigation started

    ConsoleCompleter* _completer = nullptr;
    bool _tabRepeat = false; ///< previous key was Tab: list candidates if still stuck
    QTextCharFormat _channelFormat[static_cast<int>(ConsoleChannel::ChannelCount)];

    // ANSI SGR (ESC[...m) rendition state, persistent across writes like a
    // real terminal; reset to channel defaults by ESC[0m and on each new
    // prompt, so attributes can't leak into the next command's output
    struct SgrState {
        bool bold = false;
        bool dim = false;
        bool italic = false;
        bool underline = false;
        bool inverse = false;
        QColor fg; ///< invalid = channel default
        QColor bg; ///< invalid = channel default
    };
    // insert text into the document, parsing SGR sequences
    void writeFormatted(QTextCursor& tc, const QString& message, const QTextCharFormat& fmt);
    // channel format plus the current SGR state
    QTextCharFormat sgrFormat(const QTextCharFormat& base) const;
    // effective fg/bg: SGR color, else channel format, else palette
    QColor sgrFg(const QTextCharFormat& base) const;
    QColor sgrBg(const QTextCharFormat& base) const;
    void applySgr(const QString& params);
    void resetSgr();

    SgrState _sgr;
    QString _sgrPending; ///< escape sequence split across write() calls
};

} // namespace Linea::UI

#endif // LINEA_UI_CONSOLE_CONSOLE_WIDGET_H
