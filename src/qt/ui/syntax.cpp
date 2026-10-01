// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt syntax highlighting for text editing widgets.
 */

#include "syntax.h"

#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QPalette>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QXmlStreamReader>

#include <functional>
#include <stdexcept>
#include <vector>

#include "io/resource.h"
#include "object/sp-factory.h"
#include "theme.h"
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

/**
 * Resolve a syntax theme name to a gtksourceview style scheme file.
 *
 * An empty theme auto-selects "inkscape-light" or "inkscape-dark" to match
 * the current UI theme. Unknown ids fall back to that same default.
 */
QString findSchemeFile(QString theme) {
    const auto dir = QString::fromStdString(Inkscape::IO::Resource::get_path_string(
        Inkscape::IO::Resource::SYSTEM, Inkscape::IO::Resource::UIS, "syntax-themes"));
    if (dir.isEmpty()) {
        return {};
    }

    const auto pathFor = [&dir](const QString& id) {
        // "inkscape-light" -> "light", "-none-" -> "none"
        QString suffix = id;
        if (suffix.startsWith(QLatin1String("inkscape-"))) {
            suffix = suffix.mid(9);
        }
        suffix.remove(QLatin1Char('-'));
        return dir + "/syntax-theme-" + suffix + ".xml";
    };

    const bool dark = isDarkTheme();
    const auto fallback = [&]() {
        return pathFor(dark ? QStringLiteral("inkscape-dark") : QStringLiteral("inkscape-light"));
    };

    if (theme.isEmpty() || theme == QLatin1String("inkscape-light") ||
        theme == QLatin1String("inkscape-dark")) {
        return fallback();
    }

    const QString path = pathFor(theme);
    return QFileInfo::exists(path) ? path : fallback();
}

/**
 * Parse a gtksourceview style scheme file into a map of style id -> Style.
 * Results are cached per resolved file path.
 */
const QHash<QString, Style>& schemeStyles(const QString& theme) {
    static QHash<QString, QHash<QString, Style>> cache;

    const QString path = findSchemeFile(theme);
    auto it = cache.find(path);
    if (it != cache.end()) {
        return it.value();
    }

    QHash<QString, QString> colors;
    struct RawStyle {
        QString foreground;
        QString background;
        bool bold = false;
        bool italic = false;
        bool underline = false;
    };
    QHash<QString, RawStyle> rawStyles;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            if (xml.readNext() != QXmlStreamReader::StartElement) {
                continue;
            }
            const auto attrs = xml.attributes();
            if (xml.name() == QLatin1String("color")) {
                colors[attrs.value(QLatin1String("name")).toString()] =
                    attrs.value(QLatin1String("value")).toString();
            } else if (xml.name() == QLatin1String("style")) {
                auto& s = rawStyles[attrs.value(QLatin1String("name")).toString()];
                s.foreground = attrs.value(QLatin1String("foreground")).toString();
                s.background = attrs.value(QLatin1String("background")).toString();
                s.bold = attrs.value(QLatin1String("bold")) == QLatin1String("true");
                s.italic = attrs.value(QLatin1String("italic")) == QLatin1String("true");
                const auto underline = attrs.value(QLatin1String("underline"));
                s.underline = !underline.isEmpty() && underline != QLatin1String("none") &&
                              underline != QLatin1String("false");
            }
        }
    }

    // Resolve <color> name references in style attributes
    auto resolve = [&colors](const QString& ref) -> std::optional<QColor> {
        if (ref.isEmpty()) {
            return {};
        }
        const QColor color(ref.startsWith(QLatin1Char('#')) ? ref : colors.value(ref));
        return color.isValid() ? std::optional(color) : std::nullopt;
    };

    QHash<QString, Style> styles;
    for (auto it = rawStyles.constBegin(); it != rawStyles.constEnd(); ++it) {
        Style& s = styles[it.key()];
        s.color = resolve(it.value().foreground);
        s.background = resolve(it.value().background);
        s.bold = it.value().bold;
        s.italic = it.value().italic;
        s.underline = it.value().underline;
    }

    return cache.insert(path, styles).value();
}

/**
 * Build a ColorTheme from a syntax scheme file, mapping fields to the same
 * style ids used by the bundled .lang definitions (svgd, svgpoints, css)
 * and by upstream's build_xml_styles().
 */
ColorTheme themeFromScheme(const QString& theme, const QPalette& palette) {
    const auto& styles = schemeStyles(theme);

    const QColor text = palette.text().color();
    auto color = [&styles, &text](const char* id) {
        const auto it = styles.constFind(QLatin1String(id));
        return it != styles.constEnd() && it->color ? *it->color : text;
    };

    ColorTheme t;
    // Schemes don't define an editor background; keep the widget's base color
    t.background = palette.base().color();
    t.text = color("text");
    t.keyword = color("def:keyword");
    t.string = color("def:string");
    t.number = color("def:number");
    t.comment = color("def:comment");
    t.property = color("def:identifier");
    t.value = color("def:function");
    t.command = color("def:keyword"); // svgd.lang maps command -> def:keyword
    t.punctuation = color("css:delimiter");
    t.path_node = color("def:number");
    t.path_control = color("def:special-constant");
    t.path_angle = color("def:type");
    t.path_flags = color("def:preprocessor");
    t.error = color("def:error");
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

void HighlightingEditView::setStyle(const QString& theme) {
    const ColorTheme t = themeFromScheme(theme, QGuiApplication::palette());
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

QString Style::openingTag() const {
    if (isDefault()) {
        return {};
    }
    QString tag = QStringLiteral("<span style=\"");
    if (color) {
        tag += QLatin1String("color:") + color->name() + QLatin1Char(';');
    }
    if (background) {
        tag += QLatin1String("background-color:") + background->name() + QLatin1Char(';');
    }
    if (bold) {
        tag += QStringLiteral("font-weight:bold;");
    }
    if (italic) {
        tag += QStringLiteral("font-style:italic;");
    }
    if (underline) {
        tag += QStringLiteral("text-decoration:underline;");
    }
    return tag + QStringLiteral("\">");
}

QString Style::closingTag() const {
    return isDefault() ? QString() : QStringLiteral("</span>");
}

QString XMLFormatter::format(const Style& style, const QString& content) const {
    return style.openingTag() + content.toHtmlEscaped() + style.closingTag();
}

void XMLFormatter::openTag(const QString& tagName) {
    _wip = format(_style.angular_brackets, QStringLiteral("<"));
    if (tagName.isEmpty()) {
        return;
    }

    // Highlight as errors unsupported tags in the SVG namespace (explicit or implicit).
    QString qualified = tagName;
    bool isSvg = true;
    if (!qualified.contains(QLatin1Char(':'))) {
        qualified = QStringLiteral("svg:") + qualified;
    } else {
        isSvg = qualified.startsWith(QStringLiteral("svg:"));
    }
    bool error = isSvg && !SPFactory::supportsType(qualified.toStdString());

    _wip += format(error ? _style.error : _style.tag_name, tagName);
}

void XMLFormatter::addAttribute(const QString& name, const QString& value) {
    _wip += QLatin1Char(' ') + format(_style.attribute_name, name) +
            format(_style.angular_brackets, QStringLiteral("=")) +
            format(_style.attribute_value, QLatin1Char('"') + value + QLatin1Char('"'));
}

QString XMLFormatter::finishTag(bool selfClose) {
    return _wip + format(_style.angular_brackets,
                         selfClose ? QStringLiteral("/>") : QStringLiteral(">"));
}

QString XMLFormatter::formatContent(const QString& content) const {
    return format(_style.content, content);
}

XMLStyles buildXmlStyles(const QString& theme) {
    const auto& styles = schemeStyles(theme);
    auto get = [&styles](const char* id) {
        return styles.value(QLatin1String(id));
    };

    // Same style-id mapping as upstream's build_xml_styles()
    XMLStyles s;
    s.prolog = get("def:warning");
    s.comment = get("def:comment");
    s.angular_brackets = get("draw-spaces");
    s.tag_name = get("def:statement");
    s.attribute_name = get("def:number");
    s.attribute_value = get("def:string");
    s.content = get("def:string");
    s.error = get("def:error");
    return s;
}

QFont fixedFont(const QFont& base) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (base.pointSizeF() > 0) {
        font.setPointSizeF(base.pointSizeF());
    } else if (base.pixelSize() > 0) {
        font.setPixelSize(base.pixelSize());
    }
    return font;
}

void setMonoFont(QPlainTextEdit& editor, bool enabled) {
    editor.setFont(enabled ? fixedFont(editor.font()) : QFont());
    editor.document()->setDefaultFont(editor.font());
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
