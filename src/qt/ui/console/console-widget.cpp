// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Interactive console widget: read-only output above an editable input line,
 * with command history and optional completion.
 *
 * Ported from QConsoleWidget (https://github.com/gapost/qconsolewidget,
 * MIT-licensed, part of QDaq). Adapted for Qt6/Linea: the QIODevice
 * interface was removed and command history is per-instance.
 */

#include "console-widget.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QTimer>

#include "console-completer.h"

namespace Linea::UI {

namespace {

QString longestCommonPrefix(const QStringList& strings) {
    if (strings.isEmpty()) {
        return {};
    }
    QString prefix = strings.first();
    for (const QString& s : strings) {
        int i = 0;
        while (i < prefix.size() && i < s.size() && prefix.at(i) == s.at(i)) {
            ++i;
        }
        prefix.truncate(i);
    }
    return prefix;
}

// ANSI colors 0-15 (Tango palette)
QColor ansiColor(int index) {
    static const QRgb table[16] = {
        0x000000, 0xcc0000, 0x4e9a06, 0xc4a000, 0x3465a4, 0x75507b, 0x06989a, 0xd3d7cf,
        0x555753, 0xef2929, 0x8ae234, 0xfce94f, 0x729fcf, 0xad7fa8, 0x34e2e2, 0xeeeeec,
    };
    return QColor(table[qBound(0, index, 15)]);
}

// linear blend of fg toward bg by t (0 = fg, 1 = bg)
QColor blend(const QColor& fg, const QColor& bg, qreal t) {
    return QColor::fromRgbF(fg.redF() * (1 - t) + bg.redF() * t,
                            fg.greenF() * (1 - t) + bg.greenF() * t,
                            fg.blueF() * (1 - t) + bg.blueF() * t,
                            fg.alphaF() * (1 - t) + bg.alphaF() * t);
}

// ANSI 256-color palette: 0-15 standard, 16-231 color cube, 232-255 grayscale
QColor ansi256(int n) {
    if (n < 16) {
        return ansiColor(n);
    }
    if (n < 232) {
        n -= 16;
        const int levels[6] = {0, 95, 135, 175, 215, 255};
        return QColor(levels[n / 36], levels[(n / 6) % 6], levels[n % 6]);
    }
    const int gray = 8 + (qBound(232, n, 255) - 232) * 10;
    return QColor(gray, gray, gray);
}

} // namespace

ConsoleWidget::ConsoleWidget(QWidget* parent)
    : QPlainTextEdit(parent) {
    const QTextCharFormat fmt = currentCharFormat();
    for (auto& f : _channelFormat) {
        f = fmt;
    }
    // channel colors readable on the dark console background
    _channelFormat[static_cast<int>(ConsoleChannel::StandardOutput)].setForeground(QColor(0x6cb3f0));
    _channelFormat[static_cast<int>(ConsoleChannel::StandardError)].setForeground(QColor(0xf47c7c));

    auto mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (font().pointSize() > 0) {
        mono.setPointSize(font().pointSize());
    }
    setFont(mono);
    // no padding between the widget bounds and the text rows
    document()->setDocumentMargin(0);
    setContentsMargins(0, 0, 0, 0);
    // terminal-style hard wrap: break anywhere so the horizontal scrollbar
    // can never appear when the panel is narrowed
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setUndoRedoEnabled(false);
}

QTextCharFormat ConsoleWidget::channelCharFormat(ConsoleChannel channel) const {
    if (channel < ConsoleChannel::StandardInput || channel >= ConsoleChannel::ChannelCount) {
        return QTextCharFormat();
    }
    return _channelFormat[static_cast<int>(channel)];
}

void ConsoleWidget::setChannelCharFormat(ConsoleChannel channel, const QTextCharFormat& fmt) {
    if (channel < ConsoleChannel::StandardInput || channel >= ConsoleChannel::ChannelCount) {
        return;
    }
    _channelFormat[static_cast<int>(channel)] = fmt;
}

void ConsoleWidget::setMode(ConsoleMode mode) {
    if (mode == _mode) {
        return;
    }
    _mode = mode;
    if (mode != ConsoleMode::Input) {
        return;
    }
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
    setCurrentCharFormat(channelCharFormat(ConsoleChannel::StandardInput));
    _inputPos = cursor.position();
}

void ConsoleWidget::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    // keep the newest output visible: scroll to the bottom once the
    // scrollbar range has been updated for the new viewport size
    QTimer::singleShot(0, this, [this] { verticalScrollBar()->setValue(verticalScrollBar()->maximum()); });
}

int ConsoleWidget::lineHeight() const {
    const auto block = document()->firstBlock();
    return qMax(15, static_cast<int>(document()->documentLayout()->blockBoundingRect(block).height()));
}

QString ConsoleWidget::commandLine() const {
    if (_mode == ConsoleMode::Output) {
        return QString();
    }
    // select text in the edit zone (from the input position to the end)
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.setPosition(_inputPos, QTextCursor::KeepAnchor);
    QString code = cursor.selectedText();
    code.replace(QChar::ParagraphSeparator, QChar::LineFeed);
    return code;
}

void ConsoleWidget::replaceCommandLine(const QString& text) {
    // select the text after the input position and replace it
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.setPosition(_inputPos, QTextCursor::KeepAnchor);
    cursor.insertText(text, channelCharFormat(ConsoleChannel::StandardInput));
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
}

void ConsoleWidget::showPrompt(const QString& text) {
    // new prompt = new command boundary: SGR attributes can't leak through
    resetSgr();
    write(text, channelCharFormat(ConsoleChannel::StandardInput));
    setMode(ConsoleMode::Input);
}

void ConsoleWidget::handleReturnKey() {
    const QString code = commandLine();

    // start a new block and switch to output mode
    appendPlainText(QString());
    setMode(ConsoleMode::Output);

    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);

    if (!code.isEmpty()) {
        addToHistory(code);
    }
    // a command may destroy this widget (close/quit) or move focus
    // elsewhere (e.g. delete resets the active tool, whose constructor
    // grabs canvas focus) — reclaim focus if we survived and are shown
    const QPointer<ConsoleWidget> guard(this);
    Q_EMIT commandEntered(code);
    if (guard && guard->isVisible()) {
        setFocus();
    }
}

void ConsoleWidget::handleTabKey() {
    if (!_completer) {
        return;
    }
    // the input line from the editable-zone start to the cursor
    QTextCursor cursor = textCursor();
    cursor.setPosition(_inputPos, QTextCursor::KeepAnchor);
    const QString commandText = cursor.selectedText();

    const int count = _completer->updateCompletionModel(commandText);
    if (count == 0) {
        QApplication::beep();
        _tabRepeat = false;
        return;
    }
    if (count == 1) {
        insertCompletion(_completer->currentCompletion());
        _tabRepeat = false;
        return;
    }

    QStringList candidates;
    for (int i = 0; i < count; ++i) {
        _completer->setCurrentRow(i);
        candidates << _completer->currentCompletion();
    }

    // extend the current token to the longest common prefix; on a repeated
    // Tab with no further progress, list the candidates above the input line
    const QString token = commandText.mid(_completer->insertPos());
    const QString common = longestCommonPrefix(candidates);
    if (common.size() > token.size() && common.startsWith(token)) {
        insertCompletion(common);
    } else if (_tabRepeat) {
        writeStdOut(candidates.join("  ") + "\n");
    } else {
        QApplication::beep();
    }
    _tabRepeat = true;
}

void ConsoleWidget::historyMove(bool older) {
    if (older) {
        // scan towards older entries
        for (int i = _historyIndex + 1; i < _history.size(); ++i) {
            if (_history.at(i).startsWith(_historyToken)) {
                _historyIndex = i;
                replaceCommandLine(_history.at(i));
                return;
            }
        }
        QApplication::beep();
        return;
    }

    // scan towards newer entries
    for (int i = _historyIndex - 1; i >= 0; --i) {
        if (_history.at(i).startsWith(_historyToken)) {
            _historyIndex = i;
            replaceCommandLine(_history.at(i));
            return;
        }
    }
    // no match: restore the text typed before navigation started
    if (_historyIndex == -1) {
        QApplication::beep();
        return;
    }
    _historyIndex = -1;
    replaceCommandLine(_historyToken);
}

void ConsoleWidget::addToHistory(const QString& command) {
    if (_history.isEmpty() || _history.first() != command) {
        _history.prepend(command);
        while (_history.size() > _historyLimit) {
            _history.removeLast();
        }
    }
    _historyIndex = -1;
}

void ConsoleWidget::insertCompletion(const QString& completion) {
    if (!_completer) {
        return;
    }
    // select from the insertion position to the cursor and replace it
    QTextCursor tc = textCursor();
    tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, tc.position() - _inputPos - _completer->insertPos());
    tc.insertText(completion, channelCharFormat(ConsoleChannel::StandardInput));
    setTextCursor(tc);
}

void ConsoleWidget::setCompleter(ConsoleCompleter* completer) {
    if (_completer == completer) {
        return;
    }
    _completer = completer;
}

void ConsoleWidget::keyPressEvent(QKeyEvent* event) {
    // consecutive Tab presses enable candidate listing
    if (event->key() != Qt::Key_Tab) {
        _tabRepeat = false;
    }

    QTextCursor textCursor = this->textCursor();
    const bool selectionInEditZone = isSelectionInEditZone();
    const Qt::KeyboardModifiers mods = event->modifiers();

    // check for user abort request
    if ((mods & Qt::ControlModifier) && event->key() == Qt::Key_C) {
        Q_EMIT abortEvaluation();
        event->accept();
        return;
    }

    // allow copying anywhere in the console
    if (event->key() == Qt::Key_C && mods == Qt::MetaModifier) {
        if (textCursor.hasSelection()) {
            copy();
        }
        event->accept();
        return;
    }

    // the rest of the key events are ignored in output mode
    if (mode() != ConsoleMode::Input) {
        event->ignore();
        return;
    }

    // allow cut only if the selection is limited to the edit zone
    if (event->key() == Qt::Key_X && mods == Qt::MetaModifier) {
        if (selectionInEditZone) {
            cut();
        }
        event->accept();
        return;
    }

    // allow paste only if the selection/cursor is in the edit zone
    if (event->key() == Qt::Key_V && mods == Qt::MetaModifier) {
        if (selectionInEditZone || isCursorInEditZone()) {
            const QMimeData* const clipboard = QApplication::clipboard()->mimeData();
            const QString text = clipboard->text();
            if (!text.isNull()) {
                textCursor.insertText(text, channelCharFormat(ConsoleChannel::StandardInput));
            }
        }
        event->accept();
        return;
    }

    const int key = event->key();
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);

    if (_historyIndex != -1 && key != Qt::Key_Up && key != Qt::Key_Down) {
        _historyIndex = -1;
    }

    // force the cursor back to the edit zone for all keys except modifiers
    if (!isCursorInEditZone() && key != Qt::Key_Control && key != Qt::Key_Shift && key != Qt::Key_Alt &&
        key != Qt::Key_Meta) {
        textCursor.movePosition(QTextCursor::End);
        setTextCursor(textCursor);
    }

    // readline-style editing: Ctrl+A/E/U/K/W
    if (mods & Qt::ControlModifier) {
        // the local textCursor may be stale after the edit-zone repositioning
        QTextCursor tc = this->textCursor();
        bool handled = true;
        switch (key) {
            case Qt::Key_A:
                tc.setPosition(_inputPos, shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                break;
            case Qt::Key_E:
                tc.movePosition(QTextCursor::End, shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                break;
            case Qt::Key_U:
                tc.setPosition(_inputPos, QTextCursor::KeepAnchor);
                tc.removeSelectedText();
                break;
            case Qt::Key_K:
                tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                tc.removeSelectedText();
                break;
            case Qt::Key_W:
                tc.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
                if (tc.position() < _inputPos) {
                    tc.setPosition(_inputPos, QTextCursor::KeepAnchor);
                }
                tc.removeSelectedText();
                break;
            default:
                handled = false;
        }
        if (handled) {
            setTextCursor(tc);
            event->accept();
            return;
        }
    }

    switch (key) {
        case Qt::Key_Up:
            // start history navigation with the current line as the match prefix
            if (_historyIndex == -1) {
                _historyToken = commandLine();
            }
            historyMove(true);
            event->accept();
            break;

        case Qt::Key_Down:
            historyMove(false);
            event->accept();
            break;

        case Qt::Key_Left:
            if (textCursor.position() > _inputPos) {
                QPlainTextEdit::keyPressEvent(event);
            } else {
                QApplication::beep();
            }
            event->accept();
            break;

        case Qt::Key_Delete:
            event->accept();
            if (selectionInEditZone) {
                cut();
            } else if (textCursor.position() < _inputPos) {
                // cursor must be in the edit zone
                QApplication::beep();
            } else {
                QPlainTextEdit::keyPressEvent(event);
            }
            break;

        case Qt::Key_Backspace:
            event->accept();
            if (selectionInEditZone) {
                cut();
            } else if (textCursor.position() <= _inputPos) {
                // cursor must be in the edit zone
                QApplication::beep();
            } else {
                QPlainTextEdit::keyPressEvent(event);
            }
            break;

        case Qt::Key_Tab:
            event->accept();
            handleTabKey();
            return;

        case Qt::Key_Home:
            event->accept();
            textCursor.setPosition(_inputPos, shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
            setTextCursor(textCursor);
            break;

        case Qt::Key_Enter:
        case Qt::Key_Return:
            event->accept();
            handleReturnKey();
            break;

        case Qt::Key_Escape:
            event->accept();
            // first Esc clears the input line; Esc on an empty line escapes
            if (commandLine().isEmpty()) {
                Q_EMIT escapePressed();
            } else {
                replaceCommandLine(QString());
            }
            break;

        default:
            event->accept();
            setCurrentCharFormat(channelCharFormat(ConsoleChannel::StandardInput));
            QPlainTextEdit::keyPressEvent(event);
            break;
    }
}

void ConsoleWidget::contextMenuEvent(QContextMenuEvent* event) {
    auto menu = createStandardContextMenu();

    if (auto a = menu->findChild<QAction*>("edit-cut")) {
        a->setEnabled(canCut());
    }
    if (auto a = menu->findChild<QAction*>("edit-delete")) {
        a->setEnabled(canCut());
    }
    if (auto a = menu->findChild<QAction*>("edit-paste")) {
        a->setEnabled(canPaste());
    }

    menu->exec(event->globalPos());
    delete menu;
}

bool ConsoleWidget::isSelectionInEditZone() const {
    const QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        return false;
    }
    return cursor.selectionStart() >= _inputPos && cursor.selectionEnd() >= _inputPos;
}

bool ConsoleWidget::isCursorInEditZone() const {
    return textCursor().position() >= _inputPos;
}

bool ConsoleWidget::canPaste() const {
    const QTextCursor cursor = textCursor();
    return cursor.position() >= _inputPos && cursor.anchor() >= _inputPos;
}

void ConsoleWidget::write(const QString& message, const QTextCharFormat& fmt) {
    const QTextCharFormat currfmt = currentCharFormat();
    QTextCursor tc = textCursor();

    if (mode() == ConsoleMode::Input) {
        // in Input mode, output messages are inserted before the editable line

        // get the offset of the current position from the end
        int editpos = tc.position();
        tc.movePosition(QTextCursor::End);
        editpos = tc.position() - editpos;

        // convert the input position to relative from the end
        _inputPos = tc.position() - _inputPos;

        // insert a block before the edit zone
        tc.movePosition(QTextCursor::StartOfBlock);
        tc.insertBlock();
        tc.movePosition(QTextCursor::PreviousBlock);

        writeFormatted(tc, message, fmt);
        tc.movePosition(QTextCursor::End);
        // restore the input position
        _inputPos = tc.position() - _inputPos;
        // restore the edit position
        tc.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, editpos);
        setTextCursor(tc);
        setCurrentCharFormat(currfmt);
        return;
    }

    // in Output mode, messages are appended
    QTextCursor endCursor = tc;
    endCursor.movePosition(QTextCursor::End);

    // check if the cursor was not at the end (e.g. moved per mouse action)
    const bool needsRestore = endCursor.position() != tc.position();

    setTextCursor(endCursor);
    QTextCursor outCursor = textCursor();
    writeFormatted(outCursor, message, fmt);
    ensureCursorVisible();

    if (needsRestore) {
        setTextCursor(tc);
    }
}

// Parse ESC[...m (SGR) sequences and insert the surrounding text with the
// composed format. Incomplete or non-SGR sequences: buffered / stripped.
void ConsoleWidget::writeFormatted(QTextCursor& tc, const QString& message, const QTextCharFormat& fmt) {
    const QString text = _sgrPending + message;
    _sgrPending.clear();

    int pos = 0;
    while (pos < text.size()) {
        const int esc = text.indexOf(u'\x1b', pos);
        const int chunkEnd = esc < 0 ? text.size() : esc;
        if (chunkEnd > pos) {
            tc.insertText(text.mid(pos, chunkEnd - pos), sgrFormat(fmt));
        }
        if (esc < 0) {
            break;
        }
        if (esc + 1 >= text.size()) {
            // lone ESC at the end: could be the start of a sequence
            _sgrPending = text.mid(esc);
            break;
        }
        if (text.at(esc + 1) != u'[') {
            // not a CSI sequence: drop the ESC, keep scanning
            pos = esc + 1;
            continue;
        }
        // find the sequence's final byte (0x40-0x7e)
        int end = esc + 2;
        while (end < text.size() && (text.at(end) < u'@' || text.at(end) > u'~')) {
            ++end;
        }
        if (end >= text.size()) {
            // incomplete sequence: finish it on the next write
            _sgrPending = text.mid(esc);
            break;
        }
        if (text.at(end) == u'm') {
            applySgr(text.mid(esc + 2, end - esc - 2));
        }
        pos = end + 1;
    }
}

QColor ConsoleWidget::sgrFg(const QTextCharFormat& base) const {
    if (_sgr.fg.isValid()) {
        return _sgr.fg;
    }
    if (base.hasProperty(QTextFormat::ForegroundBrush)) {
        return base.foreground().color();
    }
    return palette().color(QPalette::Text);
}

QColor ConsoleWidget::sgrBg(const QTextCharFormat& base) const {
    if (_sgr.bg.isValid()) {
        return _sgr.bg;
    }
    if (base.hasProperty(QTextFormat::BackgroundBrush)) {
        return base.background().color();
    }
    return palette().color(QPalette::Base);
}

QTextCharFormat ConsoleWidget::sgrFormat(const QTextCharFormat& base) const {
    QTextCharFormat fmt = base;
    if (_sgr.bold) {
        fmt.setFontWeight(QFont::Bold);
    }
    if (_sgr.italic) {
        fmt.setFontItalic(true);
    }
    if (_sgr.underline) {
        fmt.setFontUnderline(true);
    }
    if (_sgr.inverse) {
        fmt.setForeground(sgrBg(base));
        fmt.setBackground(sgrFg(base));
        return fmt;
    }
    if (_sgr.fg.isValid()) {
        fmt.setForeground(_sgr.fg);
    }
    if (_sgr.bg.isValid()) {
        fmt.setBackground(_sgr.bg);
    }
    if (_sgr.dim) {
        // mono fonts have no light weight: fade the foreground toward the bg
        fmt.setForeground(blend(sgrFg(base), sgrBg(base), 0.5));
    }
    return fmt;
}

void ConsoleWidget::applySgr(const QString& params) {
    if (params.isEmpty()) { // ESC[m == reset
        resetSgr();
        return;
    }
    const QStringList p = params.split(u';');
    for (int i = 0; i < p.size(); ++i) {
        bool ok = false;
        const int code = p.at(i).toInt(&ok);
        if (!ok) {
            continue;
        }
        switch (code) {
            case 0: resetSgr(); break;
            case 1: _sgr.bold = true; break;
            case 2: _sgr.dim = true; break;
            case 3: _sgr.italic = true; break;
            case 4: _sgr.underline = true; break;
            // 5, 6 - slow blink, rapid blink
            case 7: _sgr.inverse = true; break;
            // 8 - concealed
            // 9 - crossed-out
            // 10 - primary (default) font
            case 22: _sgr.bold = _sgr.dim = false; break;
            case 23: _sgr.italic = false; break;
            case 24: _sgr.underline = false; break;
            case 27: _sgr.inverse = false; break;
            case 39: _sgr.fg = QColor(); break;
            case 49: _sgr.bg = QColor(); break;
            case 38:
            case 48: {
                // 38/48 ; 5 ; n (256-color) or 38/48 ; 2 ; r ; g ; b (truecolor)
                QColor color;
                const int mode = i + 1 < p.size() ? p.at(i + 1).toInt() : -1;
                if (mode == 5 && i + 2 < p.size()) {
                    color = ansi256(p.at(i + 2).toInt());
                    i += 2;
                } else if (mode == 2 && i + 4 < p.size()) {
                    color = QColor(p.at(i + 2).toInt(), p.at(i + 3).toInt(), p.at(i + 4).toInt());
                    i += 4;
                }
                if (color.isValid()) {
                    (code == 38 ? _sgr.fg : _sgr.bg) = color;
                }
                break;
            }
            default:
                if (code >= 30 && code <= 37) {
                    _sgr.fg = ansiColor(code - 30);
                } else if (code >= 40 && code <= 47) {
                    _sgr.bg = ansiColor(code - 40);
                } else if (code >= 90 && code <= 97) {
                    _sgr.fg = ansiColor(code - 90 + 8);
                } else if (code >= 100 && code <= 107) {
                    _sgr.bg = ansiColor(code - 100 + 8);
                }
                break;
        }
    }
}

void ConsoleWidget::resetSgr() {
    _sgr = {};
    _sgrPending.clear();
}

void ConsoleWidget::writeStdOut(const QString& text) {
    write(text, channelCharFormat(ConsoleChannel::StandardOutput));
}

void ConsoleWidget::writeStdErr(const QString& text) {
    write(text, channelCharFormat(ConsoleChannel::StandardError));
}

void ConsoleWidget::clearConsole() {
    const bool wasInput = (_mode == ConsoleMode::Input);
    resetSgr();
    clear();
    _inputPos = 0;
    _historyIndex = -1;
    if (wasInput) {
        // re-enter input mode to mark the input position at the start
        _mode = ConsoleMode::Output;
        setMode(ConsoleMode::Input);
    }
}

} // namespace Linea::UI
