// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt syntax highlighting for text editing widgets.
 */

#include "syntax.h"

#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextDocument>

#include <functional>
#include <stdexcept>
#include <vector>

#include "util/svg-path-parser.h"

namespace Linea::UI::Syntax {

QTextCharFormat Style::toFormat() const {
    QTextCharFormat fmt;
    if (color) {
        fmt.setForeground(*color);
    }
    if (background) {
        fmt.setBackground(*background);
    }
    if (bold) {
        fmt.setFontWeight(QFont::Bold);
    }
    if (italic) {
        fmt.setFontItalic(true);
    }
    if (underline) {
        fmt.setFontUnderline(true);
    }
    return fmt;
}

namespace {

struct ColorTheme {
    QColor text;
    QColor background;
    QColor keyword;
    QColor string;
    QColor number;
    QColor comment;
    QColor property;
    QColor value;
    QColor command;
    QColor punctuation;
    QColor path_node;
    QColor path_control;
    QColor path_angle;
    QColor path_flags;
    QColor error;
};

ColorTheme themeFromPalette(const QPalette& palette) {
    ColorTheme t;
    t.background = palette.base().color();
    t.text = palette.text().color();
    const bool dark = t.background.lightness() < 128;
    if (dark) {
        t.keyword = QColor(0x88, 0xbb, 0xff);
        t.string = QColor(0xff, 0x88, 0x66);
        t.number = QColor(0xbb, 0x88, 0xff);
        t.comment = QColor(0x88, 0x88, 0x88);
        t.property = QColor(0x88, 0xbb, 0xff);
        t.value = QColor(0xbb, 0x88, 0xff);
        t.command = QColor(0x88, 0xbb, 0xff);
        t.punctuation = QColor(0xcc, 0xcc, 0xcc);
        t.path_node = QColor(0xff, 0x66, 0x66);
        t.path_control = QColor(0x55, 0xcc, 0xcc);
        t.path_angle = QColor(0xff, 0xcc, 0x66);
        t.path_flags = QColor(0x99, 0xdd, 0x99);
        t.error = QColor(0xff, 0x55, 0x55);
    } else {
        t.keyword = QColor(0x00, 0x00, 0xcc);
        t.string = QColor(0xcc, 0x00, 0x00);
        t.number = QColor(0x88, 0x00, 0x88);
        t.comment = QColor(0x66, 0x66, 0x66);
        t.property = QColor(0x00, 0x00, 0xcc);
        t.value = QColor(0x88, 0x00, 0x88);
        t.command = QColor(0x00, 0x00, 0xcc);
        t.punctuation = QColor(0x44, 0x44, 0x44);
        t.path_node = QColor(0xdd, 0x22, 0x22);
        t.path_control = QColor(0x00, 0x88, 0x88);
        t.path_angle = QColor(0xaa, 0x66, 0x00);
        t.path_flags = QColor(0x22, 0x77, 0x22);
        t.error = QColor(0xcc, 0x00, 0x00);
    }
    return t;
}

QTextCharFormat makeFormat(const QColor& color, bool bold = false) {
    QTextCharFormat fmt;
    fmt.setForeground(color);
    if (bold) {
        fmt.setFontWeight(QFont::Bold);
    }
    return fmt;
}

class RuleHighlighter : public QSyntaxHighlighter {
public:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    RuleHighlighter(QTextDocument* parent, std::vector<Rule> rules, bool svgPath = false)
        : QSyntaxHighlighter(parent)
        , _rules(std::move(rules))
        , _svgPath(svgPath) {}

    void setRules(std::vector<Rule> rules) {
        _rules = std::move(rules);
        rehighlight();
    }

    void setPathTheme(const ColorTheme& theme) {
        _pathCommand = makeFormat(theme.command, true);
        _pathNode = makeFormat(theme.path_node);
        _pathControl = makeFormat(theme.path_control);
        _pathAngle = makeFormat(theme.path_angle);
        _pathFlags = makeFormat(theme.path_flags);
        _pathPunctuation = makeFormat(theme.punctuation);
        rehighlight();
    }

protected:
    void highlightBlock(const QString& text) override {
        if (_svgPath) {
            highlightSvgPath(text);
            return;
        }
        for (const auto& rule : _rules) {
            auto it = rule.pattern.globalMatch(text);
            while (it.hasNext()) {
                const auto match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }

private:
    static int argumentCount(QChar command) {
        switch (command.toUpper().toLatin1()) {
            case 'M': case 'L': case 'T': return 2;
            case 'H': case 'V': return 1;
            case 'C': return 6;
            case 'S': case 'Q': return 4;
            case 'A': return 7;
            case 'Z': return 0;
            default: return 0;
        }
    }

    QTextCharFormat formatForArgument(QChar command, int index) const {
        const char upper = command.toUpper().toLatin1();
        if (upper == 'C') return index % 6 < 4 ? _pathControl : _pathNode;
        if (upper == 'S' || upper == 'Q') return index % 4 < 2 ? _pathControl : _pathNode;
        if (upper == 'A') {
            const int position = index % 7;
            if (position < 2) return _pathControl;
            if (position == 2) return _pathAngle;
            if (position == 3 || position == 4) return _pathFlags;
        }
        return _pathNode;
    }

    void highlightSvgPath(const QString& text) {
        QRegularExpression token(QStringLiteral("[MmLlHhVvCcSsQqTtAaZz]|[-+]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][-+]?\\d+)?"));
        auto match = token.globalMatch(text);
        QChar command;
        int argument = 0;
        if (previousBlockState() >= 0) {
            command = QChar((previousBlockState() >> 8) & 0xff);
            argument = previousBlockState() & 0xff;
        }
        while (match.hasNext()) {
            const auto item = match.next();
            const QString value = item.captured();
            if (value.size() == 1 && value.at(0).isLetter()) {
                command = value.at(0);
                argument = 0;
                setFormat(item.capturedStart(), item.capturedLength(), _pathCommand);
                continue;
            }
            if (command.isNull() || argumentCount(command) == 0) {
                continue;
            }
            setFormat(item.capturedStart(), item.capturedLength(), formatForArgument(command, argument));
            argument = (argument + 1) % argumentCount(command);
        }
        if (!command.isNull()) {
            setCurrentBlockState((command.unicode() << 8) | argument);
        }
    }

    std::vector<Rule> _rules;
    bool _svgPath = false;
    QTextCharFormat _pathCommand;
    QTextCharFormat _pathNode;
    QTextCharFormat _pathControl;
    QTextCharFormat _pathAngle;
    QTextCharFormat _pathFlags;
    QTextCharFormat _pathPunctuation;
};

using Rule = RuleHighlighter::Rule;
using RuleBuilder = std::function<std::vector<Rule>(const ColorTheme&)>;
using Formatter = std::function<QString(const QString&)>;

Formatter noReformat() {
    return [](const QString& s) { return s; };
}

std::vector<Rule> makeCssRules(const ColorTheme& t) {
    std::vector<Rule> rules;
    // Single-line block comments only; multi-line CSS comments are rare here.
    rules.push_back({QRegularExpression("/\\*.*?\\*/"), makeFormat(t.comment)});
    rules.push_back({QRegularExpression("//.*"), makeFormat(t.comment)});
    rules.push_back({QRegularExpression(R"("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')"), makeFormat(t.string)});
    rules.push_back({QRegularExpression(R"(\b([a-zA-Z-]+)(?=\s*:))"), makeFormat(t.property, true)});
    rules.push_back({QRegularExpression(R"(\B#[0-9a-fA-F]{3,8}\b)"), makeFormat(t.number)});
    rules.push_back({QRegularExpression(R"(\b[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?(?:px|pt|em|rem|ex|ch|cm|mm|in|pc|%)?\b)"), makeFormat(t.number)});
    rules.push_back({QRegularExpression("[;:{},]"), makeFormat(t.punctuation)});
    return rules;
}

std::vector<Rule> makeSvgPathRules(const ColorTheme& t) {
    std::vector<Rule> rules;
    rules.push_back({QRegularExpression("[MmLlHhVvCcSsQqTtAaZz]"), makeFormat(t.command, true)});
    rules.push_back({QRegularExpression("[-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?"), makeFormat(t.number)});
    rules.push_back({QRegularExpression(","), makeFormat(t.punctuation)});
    return rules;
}

std::vector<Rule> makeSvgPointsRules(const ColorTheme& t) {
    std::vector<Rule> rules;
    rules.push_back({QRegularExpression("[-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?"), makeFormat(t.number)});
    rules.push_back({QRegularExpression("[,]"), makeFormat(t.punctuation)});
    return rules;
}

std::vector<Rule> makeJsRules(const ColorTheme& t) {
    std::vector<Rule> rules;
    rules.push_back({QRegularExpression("/\\*.*?\\*/"), makeFormat(t.comment)});
    rules.push_back({QRegularExpression("//.*"), makeFormat(t.comment)});
    rules.push_back({QRegularExpression(R"("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`)"), makeFormat(t.string)});
    rules.push_back({QRegularExpression(R"(\b(?:function|var|let|const|if|else|for|while|do|return|class|new|this|true|false|null|undefined|import|export|from|try|catch|throw|async|await)\b)"), makeFormat(t.keyword, true)});
    rules.push_back({QRegularExpression(R"(\b\d+\.?\d*\b)"), makeFormat(t.number)});
    rules.push_back({QRegularExpression(R"(\b([a-zA-Z_$][\w$]*)(?=\s*\())"), makeFormat(t.value)});
    return rules;
}

std::vector<Rule> makeLuaRules(const ColorTheme& t) {
    std::vector<Rule> rules;
    rules.push_back({QRegularExpression(R"("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')"), makeFormat(t.string)});
    rules.push_back({QRegularExpression(R"(\b(?:and|break|do|else|elseif|end|false|for|function|goto|if|in|local|nil|not|or|repeat|return|then|true|until|while)\b)"), makeFormat(t.keyword, true)});
    rules.push_back({QRegularExpression(R"(\b\d+(?:\.\d+)?\b)"), makeFormat(t.number)});
    rules.push_back({QRegularExpression(R"(\b[a-zA-Z_][a-zA-Z0-9_]*(?=\s*\())"), makeFormat(t.value)});
    // Apply comments last so keywords and numbers inside comments keep comment styling.
    rules.push_back({QRegularExpression("--.*"), makeFormat(t.comment)});
    return rules;
}

QString prettifyCss(const QString& css) {
    QString out = css;
    QRegularExpression re1(":([^\\s/])");
    out.replace(re1, ": \\1");
    QRegularExpression re2(";([^\\r\\n])");
    out.replace(re2, ";\n\\1");
    if (!out.isEmpty() && !out.endsWith(';')) {
        out.append(';');
    }
    return out;
}

QString minifyCss(const QString& css) {
    QString out = css;
    QRegularExpression re("(:|;)[\\s]+");
    out.replace(re, "\\1");
    if (out.endsWith(';')) {
        out.chop(1);
    }
    return out;
}

QString minifySvgd(const QString& d) {
    QString out = d;
    QRegularExpression re("[\\s]+");
    out.replace(re, " ");
    out = out.trimmed();
    return out;
}

QString prettifySvgd(const QString& d) {
    Glib::ustring u = Inkscape::SvgPathParser::prettify_svgd(Glib::ustring(d.toStdString()));
    return QString::fromStdString(u.raw());
}

class PlainTextEditView : public TextEditView {
public:
    PlainTextEditView();

    void setStyle(const QString& /*theme*/) override;
    void setText(const QString& text) override;
    QString getText() const override;
    QPlainTextEdit& getEditor() const override;

private:
    std::unique_ptr<QPlainTextEdit> _editor;
};

class HighlightingEditView : public TextEditView {
public:
    HighlightingEditView(RuleBuilder builder, Formatter prettify, Formatter minify, bool svgPath = false);

    void setStyle(const QString& /*theme*/) override;
    void setText(const QString& text) override;
    QString getText() const override;
    QPlainTextEdit& getEditor() const override;

private:
    std::unique_ptr<QPlainTextEdit> _editor;
    std::unique_ptr<RuleHighlighter> _highlighter;
    RuleBuilder _builder;
    Formatter _prettify;
    Formatter _minify;
};

PlainTextEditView::PlainTextEditView()
    : _editor(std::make_unique<QPlainTextEdit>()) {
    _editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    _editor->setObjectName("SyntaxTextEdit");
}

void PlainTextEditView::setStyle(const QString& /*theme*/) {
    _editor->setPalette(QGuiApplication::palette());
}

void PlainTextEditView::setText(const QString& text) {
    _editor->setPlainText(text);
}

QString PlainTextEditView::getText() const {
    return _editor->toPlainText();
}

QPlainTextEdit& PlainTextEditView::getEditor() const {
    return *_editor;
}

HighlightingEditView::HighlightingEditView(RuleBuilder builder, Formatter prettify, Formatter minify, bool svgPath)
    : _editor(std::make_unique<QPlainTextEdit>())
    , _highlighter(std::make_unique<RuleHighlighter>(_editor->document(), std::vector<Rule>{}, svgPath))
    , _builder(std::move(builder))
    , _prettify(std::move(prettify))
    , _minify(std::move(minify)) {
    _editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    _editor->setObjectName("SyntaxTextEdit");
    setStyle(QString());
}

void HighlightingEditView::setStyle(const QString& /*theme*/) {
    const ColorTheme t = themeFromPalette(QGuiApplication::palette());
    _highlighter->setRules(_builder(t));
    _highlighter->setPathTheme(t);
    QPalette p = _editor->palette();
    p.setColor(QPalette::Base, t.background);
    p.setColor(QPalette::Text, t.text);
    _editor->setPalette(p);
}

void HighlightingEditView::setText(const QString& text) {
    _editor->setPlainText(_prettify(text));
}

QString HighlightingEditView::getText() const {
    return _minify(_editor->toPlainText());
}

QPlainTextEdit& HighlightingEditView::getEditor() const {
    return *_editor;
}

} // namespace

XMLStyles buildXmlStyles(const QString& /*theme*/) {
    const ColorTheme t = themeFromPalette(QGuiApplication::palette());
    XMLStyles s;
    s.prolog = Style{t.keyword, {}, true};
    s.comment = Style{t.comment, {}, false, true};
    s.angular_brackets = Style{t.punctuation};
    s.tag_name = Style{t.command, {}, true};
    s.attribute_name = Style{t.number};
    s.attribute_value = Style{t.string};
    s.content = Style{t.string};
    s.error = Style{t.error, {}, true};
    return s;
}

std::unique_ptr<TextEditView> TextEditView::create(SyntaxMode mode) {
    switch (mode) {
        case SyntaxMode::PlainText:
            return std::make_unique<PlainTextEditView>();
        case SyntaxMode::InlineCss:
            return std::make_unique<HighlightingEditView>(makeCssRules, &prettifyCss, &minifyCss);
        case SyntaxMode::CssStyle:
            return std::make_unique<HighlightingEditView>(makeCssRules, noReformat(), noReformat());
        case SyntaxMode::SvgPathData:
            return std::make_unique<HighlightingEditView>(makeSvgPathRules, &prettifySvgd, &minifySvgd, true);
        case SyntaxMode::SvgPolyPoints:
            return std::make_unique<HighlightingEditView>(makeSvgPointsRules, noReformat(), noReformat());
        case SyntaxMode::JavaScript:
            return std::make_unique<HighlightingEditView>(makeJsRules, noReformat(), noReformat());
        case SyntaxMode::Lua:
            return std::make_unique<HighlightingEditView>(makeLuaRules, noReformat(), noReformat());
        default:
            throw std::runtime_error("Missing case in TextEditView::create()");
    }
}

} // namespace Linea::UI::Syntax
