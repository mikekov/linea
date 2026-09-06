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

    RuleHighlighter(QTextDocument* parent, std::vector<Rule> rules)
        : QSyntaxHighlighter(parent)
        , _rules(std::move(rules)) {}

    void setRules(std::vector<Rule> rules) {
        _rules = std::move(rules);
        rehighlight();
    }

protected:
    void highlightBlock(const QString& text) override {
        for (const auto& rule : _rules) {
            auto it = rule.pattern.globalMatch(text);
            while (it.hasNext()) {
                const auto match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }

private:
    std::vector<Rule> _rules;
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
    HighlightingEditView(RuleBuilder builder, Formatter prettify, Formatter minify);

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

HighlightingEditView::HighlightingEditView(RuleBuilder builder, Formatter prettify, Formatter minify)
    : _editor(std::make_unique<QPlainTextEdit>())
    , _highlighter(std::make_unique<RuleHighlighter>(_editor->document(), std::vector<Rule>{}))
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
            return std::make_unique<HighlightingEditView>(makeSvgPathRules, &prettifySvgd, &minifySvgd);
        case SyntaxMode::SvgPolyPoints:
            return std::make_unique<HighlightingEditView>(makeSvgPointsRules, noReformat(), noReformat());
        case SyntaxMode::JavaScript:
            return std::make_unique<HighlightingEditView>(makeJsRules, noReformat(), noReformat());
        default:
            throw std::runtime_error("Missing case in TextEditView::create()");
    }
}

} // namespace Linea::UI::Syntax
