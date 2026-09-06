// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt syntax highlighting for text editing widgets.
 */

#ifndef LINEA_UI_SYNTAX_H
#define LINEA_UI_SYNTAX_H

#include <QColor>
#include <QString>
#include <QTextCharFormat>
#include <memory>
#include <optional>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Linea::UI::Syntax {

/** The style of a single highlighted element. */
struct Style {
    std::optional<QColor> color;
    std::optional<QColor> background;
    bool bold = false;
    bool italic = false;
    bool underline = false;

    bool isDefault() const {
        return !color && !background && !bold && !italic && !underline;
    }

    QTextCharFormat toFormat() const;
};

/** The styles used for simple XML syntax highlighting. */
struct XMLStyles {
    Style prolog;
    Style comment;
    Style angular_brackets;
    Style tag_name;
    Style attribute_name;
    Style attribute_value;
    Style content;
    Style error;
};

/// Syntax highlighting mode (language).
enum class SyntaxMode {
    PlainText,     ///< Plain text (no highlighting).
    InlineCss,     ///< Inline CSS (contents of a style="..." attribute).
    CssStyle,      ///< File-scope CSS (contents of a CSS file or a <style> tag).
    SvgPathData,   ///< Contents of the 'd' attribute of the SVG <path> element.
    SvgPolyPoints, ///< Contents of the 'points' attribute of <polyline> or <polygon>.
    JavaScript     ///< JavaScript code.
};

/// Base class for styled text editing widget.
class TextEditView {
public:
    virtual ~TextEditView() = default;
    virtual void setStyle(const QString& theme) = 0;
    virtual void setText(const QString& text) = 0;
    virtual QString getText() const = 0;
    virtual QPlainTextEdit& getEditor() const = 0;

    static std::unique_ptr<TextEditView> create(SyntaxMode mode);
};

/// Build XML display styles from a syntax theme name.
XMLStyles buildXmlStyles(const QString& theme);

} // namespace Linea::UI::Syntax

#endif // LINEA_UI_SYNTAX_H
