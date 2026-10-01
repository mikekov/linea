// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt syntax highlighting for text editing widgets.
 */

#ifndef LINEA_UI_SYNTAX_H
#define LINEA_UI_SYNTAX_H

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QLineEdit>
#include <QString>
#include <QStyle>
#include <QStyledItemDelegate>
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

    /// Qt rich text markup tags for this style (empty if default).
    QString openingTag() const;
    QString closingTag() const;
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
    JavaScript,    ///< JavaScript code
    Lua            ///< Lua script code.
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

/**
 * A formatter for XML syntax, producing Qt rich text markup.
 *
 * Used by the XML tree view to syntax-highlight node labels.
 */
class XMLFormatter {
public:
    XMLFormatter() = default;
    explicit XMLFormatter(XMLStyles styles)
        : _style(std::move(styles)) {}

    void setStyle(const XMLStyles& newStyle) { _style = newStyle; }

    void openTag(const QString& tagName);
    void addAttribute(const QString& name, const QString& value);
    QString finishTag(bool selfClose = false);

    QString formatContent(const QString& content) const;

private:
    QString format(const Style& style, const QString& content) const;

    XMLStyles _style;
    QString _wip;
};

/// Build XML display styles from a syntax theme name.
XMLStyles buildXmlStyles(const QString& theme);

/// The system fixed-width font sized to match the given font (the system
/// fixed font's default size is typically smaller than the UI font's).
QFont fixedFont(const QFont& base);

/// Applies the fixed-width font (or restores the inherited font) to a text
/// editing widget and its document.
void setMonoFont(QPlainTextEdit& editor, bool enabled);

/**
 * Item delegate that forces a fixed-width font.
 *
 * QWidget::setFont() on a widget matched by a stylesheet rule with font
 * properties can be reverted when the application style sheet is re-applied.
 * Forcing the font in initStyleOption() and createEditor() makes the
 * delegate immune to that.
 */
class FixedFontDelegate : public QStyledItemDelegate {
public:
    explicit FixedFontDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void setFixedFont(const QFont& font) { _font = font; }
    void clearFixedFont() { _font.reset(); }

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        auto editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (!editor) {
            return nullptr;
        }
        // Opt out of the global QLineEdit min-height/border styling so the
        // editor fits inside the row.
        editor->setProperty("class", "cell-editor");
        if (_font) {
            editor->setFont(*_font);
        }
        return editor;
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        // QLineEdit centers its text at (height - fm.height() + 1) / 2, while the
        // style paints item text centered at (height - fm.height()) / 2 via
        // QStyle::alignedRect(). Trim the editor's height by the difference so
        // the inline text lands on the painted text's baseline.
        const int textHeight = opt.fontMetrics.height();
        editor->setGeometry(opt.rect.adjusted(0, 0, 0,
            (opt.rect.height() - textHeight) / 2 - (opt.rect.height() - textHeight + 1) / 2));

        // Painted item text is inset by PM_FocusFrameHMargin + 1; a QLineEdit's
        // own left margin is smaller. Compensate so the text doesn't jump.
        if (auto lineEdit = qobject_cast<QLineEdit*>(editor)) {
            const int dx = lineEdit->style()->pixelMetric(
                QStyle::PM_FocusFrameHMargin, &opt, opt.widget) - 1;
            lineEdit->setTextMargins(dx, 0, -dx, 0);
        }
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        if (_font) {
            option->font = *_font;
            option->fontMetrics = QFontMetrics(*_font);
        }
    }

private:
    std::optional<QFont> _font;
};

} // namespace Linea::UI::Syntax

#endif // LINEA_UI_SYNTAX_H
