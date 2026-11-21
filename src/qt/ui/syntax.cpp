// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt syntax highlighting for text editing widgets.
 */

#include "syntax.h"

#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHash>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QXmlStreamReader>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <vector>

#include "attributes.h"
#include "colors/color.h"
#include "colors/parser.h"
#include "colors/utils.h"
#include "io/resource.h"
#include "object/sp-factory.h"
#include "style-enums.h"
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
    QColor css_value;
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
    t.property = color("def:keyword");
    t.css_value = color("def:identifier");
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

QString joinPatternAlternatives(QStringList names) {
    for (auto& name : names) {
        name = QRegularExpression::escape(name);
    }
    // Longer alternatives must come first so e.g. stroke-width wins over stroke.
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) {
        return a.size() > b.size();
    });
    return names.join(QLatin1Char('|'));
}

QStringList cssColorFunctionNames() {
    QStringList names;
    for (const auto& name : Inkscape::Colors::Parsers::get().getCssFunctionNames()) {
        names.append(QString::fromStdString(name));
    }
    return names;
}

const QRegularExpression& cssColorTokenPattern() {
    static const QRegularExpression pattern = [] {
        QStringList funcs = cssColorFunctionNames();
        funcs.append(QStringLiteral("url"));
        const QString expr =
            QStringLiteral("(?<![\\w#.@-])(?:#[0-9a-fA-F]{3,8}(?![\\w-])(?!\\s*\\{)|(?:") +
            joinPatternAlternatives(std::move(funcs)) +
            QStringLiteral(")\\([^()]*\\)|[A-Za-z]+(?![\\w(-])(?!\\s*[:{]))");
        return QRegularExpression(expr, QRegularExpression::CaseInsensitiveOption);
    }();
    return pattern;
}

// A color value in a text block, shown as an in-line color patch.
struct ColorPatch {
    int pos;
    int len;
    QColor color;
};

class ColorBlockData : public QTextBlockUserData {
public:
    std::vector<ColorPatch> patches;
    // Horizontal gap reserved after each token's last char (letter spacing)
    // for the patch to be painted into.
    int gap = 0;
};

class RuleHighlighter : public QSyntaxHighlighter {
public:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
        // Matched text (comments, strings) is never scanned for color patches.
        bool hidesColors = false;
    };

    RuleHighlighter(QTextDocument* parent, std::vector<Rule> rules, bool svgPath = false,
                    bool colorPatches = false)
        : QSyntaxHighlighter(parent)
        , _rules(std::move(rules))
        , _svgPath(svgPath)
        , _colorPatches(colorPatches) {}

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
        std::vector<std::pair<int, int>> opaque;
        for (const auto& rule : _rules) {
            auto it = rule.pattern.globalMatch(text);
            while (it.hasNext()) {
                const auto match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
                if (_colorPatches && rule.hidesColors) {
                    opaque.emplace_back(match.capturedStart(), match.capturedEnd());
                }
            }
        }
        if (_colorPatches) {
            highlightColorPatches(text, opaque);
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

    // Record a ColorPatch for every token that parses as a CSS color, and
    // reserve space after it via letter spacing for the painted patch.
    void highlightColorPatches(const QString& text, const std::vector<std::pair<int, int>>& opaque) {
        // Hex literals, identifiers, and function calls (validation decides).
        const auto& re = cssColorTokenPattern();

        auto data = new ColorBlockData;
        data->gap = qRound(QFontMetricsF(document()->defaultFont()).height() * 0.8) + 4;

        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            const int start = match.capturedStart();
            const int len = match.capturedLength();
            const bool hidden = std::any_of(opaque.begin(), opaque.end(),
                [start, len](const auto& range) { return start < range.second && start + len > range.first; });
            if (hidden) {
                continue;
            }
            const auto color = Inkscape::Colors::Color::parse(match.captured().toStdString());
            if (!color) {
                continue;
            }
            const uint32_t rgba = color->toRGBA();
            data->patches.push_back({start, len,
                QColor(SP_RGBA32_R_U(rgba), SP_RGBA32_G_U(rgba), SP_RGBA32_B_U(rgba),
                       SP_RGBA32_A_U(rgba))});
            QTextCharFormat gap = format(start + len - 1);
            gap.setFontLetterSpacingType(QFont::AbsoluteSpacing);
            gap.setFontLetterSpacing(data->gap);
            setFormat(start + len - 1, 1, gap);
        }
        setCurrentBlockUserData(data);
    }

    std::vector<Rule> _rules;
    bool _svgPath = false;
    bool _colorPatches = false;
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

QRegularExpression makeKeywordPattern(QStringList names, const QString& lookahead,
                                      QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption) {
    return QRegularExpression(QStringLiteral("(?<![\\w-])(?:") + joinPatternAlternatives(std::move(names)) +
                                  QStringLiteral(")") + lookahead,
                              options);
}

const QRegularExpression& cssPropertyNamePattern() {
    static const QRegularExpression pattern = [] {
        QStringList names;
        for (const auto& name : sp_attribute_name_list(true)) {
            names.append(QString::fromUtf8(name.c_str()));
        }
        return makeKeywordPattern(std::move(names), QStringLiteral("(?=\\s*:)"));
    }();
    return pattern;
}

const QRegularExpression& cssFunctionNamePattern() {
    static const QRegularExpression pattern = [] {
        QStringList names = cssColorFunctionNames();
        // "url" (paint servers/filters/markers) and "var" (custom-property references
        // resolved in SPStyle::_mergeDecl) are not color parsers.
        names.append(QStringLiteral("url"));
        names.append(QStringLiteral("var"));
        // CSS function names are case-insensitive; getCssPrefix lowercases.
        return makeKeywordPattern(std::move(names), QStringLiteral("(?=\\()"),
                                  QRegularExpression::CaseInsensitiveOption);
    }();
    return pattern;
}

void appendEnumKeys(const SPStyleEnum* table, QStringList& names) {
    for (const SPStyleEnum* entry = table; entry->key; ++entry) {
        const QString key = QString::fromUtf8(entry->key);
        // Purely numeric keys (e.g. font-weight "100".."900") are handled by the number rule.
        if (!key.isEmpty() && std::all_of(key.begin(), key.end(), [](QChar c) { return c.isDigit(); })) {
            continue;
        }
        names.append(key);
    }
}

const QRegularExpression& cssValueKeywordPattern() {
    static const QRegularExpression pattern = [] {
        QStringList names;
        for (const SPStyleEnum* table : {
                enum_fill_rule,
                enum_stroke_linejoin,
                enum_stroke_linecap,
                enum_font_style,
                enum_font_size,
                enum_font_variant,
                enum_font_weight,
                enum_font_stretch,
                enum_font_variant_ligatures,
                enum_font_variant_position,
                enum_font_variant_caps,
                enum_font_variant_numeric,
                enum_font_variant_alternates,
                enum_font_variant_east_asian,
                enum_text_align,
                enum_text_transform,
                enum_text_anchor,
                enum_white_space,
                enum_direction,
                enum_writing_mode,
                enum_text_orientation,
                enum_baseline,
                enum_baseline_shift,
                enum_visibility,
                enum_overflow,
                enum_isolation,
                enum_blend_mode,
                enum_display,
                enum_shape_rendering,
                enum_color_rendering,
                enum_image_rendering,
                enum_text_rendering,
                enum_enable_background,
                enum_clip_rule,
                enum_vector_effect,
                enum_stroke_extensions,
            }) {
            appendEnumKeys(table, names);
        }
        // Universal values parsed by hand in style-internal.cpp, not in any enum table.
        names.append({"inherit", "none", "currentColor", "context-fill", "context-stroke", "auto"});
        names.removeDuplicates();
        return makeKeywordPattern(std::move(names), QStringLiteral("(?![\\w-])"));
    }();
    return pattern;
}

std::vector<Rule> makeCssRules(const ColorTheme& t) {
    std::vector<Rule> rules;
    // Single-line block comments only; multi-line CSS comments are rare here.
    rules.push_back({QRegularExpression("/\\*.*?\\*/"), makeFormat(t.comment), true});
    rules.push_back({QRegularExpression("//.*"), makeFormat(t.comment), true});
    rules.push_back({QRegularExpression(R"("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')"), makeFormat(t.string), true});
    // Value keywords come before property names so a keyword that is also a
    // property name keeps the property color (later rules overwrite earlier ones).
    rules.push_back({cssValueKeywordPattern(), makeFormat(t.css_value)});
    // Inkscape only accepts the exact `!important` suffix (strip_important in style-internal.cpp);
    // maps to def:keyword in upstream css.lang.
    rules.push_back({QRegularExpression(QStringLiteral("!important(?![\\w-])")), makeFormat(t.keyword, true)});
    // Custom (--x) and vendor (-x) properties are stored without being understood
    // (SPStyle::_mergeDecl); shown non-bold to distinguish them from known properties.
    // The known-property rule follows and overwrites, so e.g. -inkscape-font-specification stays bold.
    rules.push_back({QRegularExpression(QStringLiteral(R"((?<![\w-])--?[A-Za-z_][\w-]*(?=\s*:))")), makeFormat(t.property), true});
    rules.push_back({cssPropertyNamePattern(), makeFormat(t.property, true), true});
    rules.push_back({cssFunctionNamePattern(), makeFormat(t.value)});
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

// File-scope CSS formatter for <style> element content: breaks the
// stylesheet into lines (one declaration per line, braces on their own
// lines, depth-indented). Strings, comments and parenthesized values such
// as url(data:...;base64,...) are emitted verbatim.
QString prettifyStylesheet(const QString& css) {
    QString out;
    out.reserve(css.size() + css.size() / 4);

    int parens = 0;
    QChar quote;
    bool comment = false;
    bool pendingSpace = false;
    // Per open brace: whether the block holds declarations (a ruleset or a
    // declaration-style at-rule like @font-face) vs nested rules (@media).
    std::vector<bool> declBlock;

    auto lastChar = [&out] { return out.isEmpty() ? QChar() : out.back(); };
    auto flushSpace = [&] {
        if (!pendingSpace) return;
        pendingSpace = false;
        const QChar last = lastChar();
        if (last != ' ' && last != '\n' && last != '(' && last != '{') {
            out += ' ';
        }
    };
    auto newline = [&] {
        while (!out.isEmpty() && out.back() == ' ') {
            out.chop(1);
        }
        if (!out.isEmpty() && out.back() != '\n') {
            out += '\n';
        }
        out.append(QString(2 * declBlock.size(), ' '));
        pendingSpace = false;
    };

    const qsizetype n = css.size();
    for (qsizetype i = 0; i < n; ++i) {
        const QChar c = css[i];

        if (comment) {
            out += c;
            if (c == '*' && i + 1 < n && css[i + 1] == '/') {
                out += '/';
                ++i;
                comment = false;
            }
            continue;
        }
        if (!quote.isNull()) {
            out += c;
            if (c == '\\' && i + 1 < n) {
                out += css[++i];
                continue;
            }
            if (c == quote) quote = QChar();
            continue;
        }
        if (c == '/' && i + 1 < n && css[i + 1] == '*') {
            flushSpace();
            out += "/*";
            ++i;
            comment = true;
            continue;
        }
        if (c == '"' || c == '\'') {
            flushSpace();
            out += c;
            quote = c;
            continue;
        }
        if (c == '\\' && i + 1 < n) {
            flushSpace();
            out += c;
            out += css[++i];
            continue;
        }
        if (c.isSpace()) {
            pendingSpace = true;
            continue;
        }

        if (parens == 0) {
            if (c == '{') {
                // Classify the block by the text chunk preceding '{': an
                // at-rule whose block holds nested rules vs a ruleset or a
                // declaration-style at-rule like @font-face.
                qsizetype j = out.size() - 1;
                while (j >= 0 && out[j].isSpace()) --j;
                const qsizetype chunkEnd = j;
                while (j >= 0 && out[j] != '\n' && out[j] != '{' && out[j] != '}' && out[j] != ';') --j;
                const QString chunk = out.mid(j + 1, chunkEnd - j).trimmed();
                bool declarations = true;
                if (!chunk.isEmpty() && chunk.front() == '@') {
                    qsizetype w = 1;
                    while (w < chunk.size() && (chunk[w].isLetterOrNumber() || chunk[w] == '-')) ++w;
                    static const QStringList containers =
                        {"media", "supports", "keyframes", "layer", "scope", "document", "-moz-document"};
                    declarations = !containers.contains(chunk.mid(1, w - 1).toLower());
                }
                declBlock.push_back(declarations);

                const QChar last = lastChar();
                if (last != ' ' && last != '\n' && last != '(' && last != '{' && !out.isEmpty()) {
                    out += ' ';
                }
                out += '{';
                newline();
                continue;
            }
            if (c == '}') {
                if (!declBlock.empty()) declBlock.pop_back();
                newline();
                out += '}';
                newline();
                continue;
            }
            if (c == ';') {
                out += ';';
                newline();
                continue;
            }
            if (c == ',') {
                out += ", ";
                pendingSpace = false;
                continue;
            }
            if (c == ':' && !declBlock.empty() && declBlock.back()) {
                out += ": ";
                pendingSpace = false;
                continue;
            }
        }

        if (c == '(') {
            ++parens;
        } else if (c == ')' && parens > 0) {
            --parens;
        }
        flushSpace();
        out += c;
    }

    QString result = out.trimmed();
    if (!result.isEmpty()) result += '\n';
    return result;
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

// QPlainTextEdit that paints in-line color patches into the letter-spacing
// gaps reserved by RuleHighlighter's ColorBlockData.
class ColorPatchEdit : public QPlainTextEdit {
public:
    using QPlainTextEdit::QPlainTextEdit;

protected:
    void paintEvent(QPaintEvent* event) override {
        QPlainTextEdit::paintEvent(event);

        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        const QPointF offset = contentOffset();
        const QColor checkerDark(0xcc, 0xcc, 0xcc);
        for (QTextBlock block = firstVisibleBlock(); block.isValid(); block = block.next()) {
            const QRectF geo = blockBoundingGeometry(block).translated(offset);
            if (geo.top() > event->rect().bottom()) {
                break;
            }
            auto data = static_cast<ColorBlockData*>(block.userData());
            if (!data || geo.bottom() < event->rect().top()) {
                continue;
            }
            for (const auto& patch : data->patches) {
                QTextLine line = block.layout()->lineForTextPosition(patch.pos + patch.len - 1);
                if (!line.isValid()) {
                    continue;
                }
                // x where the cursor after the token sits, i.e. the far edge
                // of the reserved letter-spacing gap. cursorToX positions are
                // relative to the block text, not the wrapped line.
                const qreal endX = line.cursorToX(patch.pos + patch.len);
                const qreal size = std::min<qreal>(data->gap - 4, line.height() - 3);
                const QRectF rect(geo.left() + line.x() + endX - data->gap + 2,
                                  geo.top() + line.y() + (line.height() - size) / 2.0,
                                  size, size);

                painter.save();
                QPainterPath clip;
                clip.addRoundedRect(rect, 2.5, 2.5);
                painter.setClipPath(clip);
                painter.fillRect(rect, Qt::white);
                if (patch.color.alpha() < 255) {
                    const qreal half = rect.width() / 2.0;
                    painter.fillRect(QRectF(rect.topLeft(), QSizeF(half, half)), checkerDark);
                    painter.fillRect(QRectF(rect.center(), QSizeF(half, half)), checkerDark);
                }
                painter.fillRect(rect, patch.color);
                painter.restore();

                painter.setPen(palette().color(QPalette::Mid));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(rect, 2.5, 2.5);
            }
        }
    }
};

class HighlightingEditView : public TextEditView {
public:
    HighlightingEditView(RuleBuilder builder, Formatter prettify, Formatter minify,
                         bool svgPath = false, bool colorPatches = false);

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

HighlightingEditView::HighlightingEditView(RuleBuilder builder, Formatter prettify, Formatter minify,
                                           bool svgPath, bool colorPatches)
    : _editor(std::make_unique<ColorPatchEdit>())
    , _highlighter(std::make_unique<RuleHighlighter>(_editor->document(), std::vector<Rule>{}, svgPath, colorPatches))
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

// Monospace glyphs read larger than proportional ones at the same nominal size, so shrink slightly to match optically.
constexpr double kMonoFontScale = 0.95;

QFont fixedFont(const QFont& base) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (base.pointSizeF() > 0) {
        font.setPointSizeF(base.pointSizeF() * kMonoFontScale);
    } else if (base.pixelSize() > 0) {
        font.setPixelSize(qRound(base.pixelSize() * kMonoFontScale));
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
            return std::make_unique<HighlightingEditView>(makeCssRules, &prettifyCss, &minifyCss, false, true);
        case SyntaxMode::CssStyle:
            return std::make_unique<HighlightingEditView>(makeCssRules, &prettifyStylesheet, noReformat(), false, true);
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
