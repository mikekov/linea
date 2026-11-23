// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Styles panel — editor for the document's global CSS stylesheet.
 */

#include "styles-panel.h"
#include "ui_styles-panel.h"

#include <QPlainTextEdit>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <glibmm/i18n.h>

#include "document-undo.h"
#include "document.h"
#include "preferences.h"
#include "style-helpers.h"
#include "swatch-editor.h"
#include "util-string/context-string.h"

namespace Linea::UI {

StylesPanel::StylesPanel(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::StylesPanel>()) {
    _ui->setupUi(this);

    _editor = Syntax::TextEditView::create(Syntax::SyntaxMode::CssStyle);
    Syntax::setMonoFont(_editor->getEditor(), true);
    _editor->getEditor().setPlaceholderText(tr("Add CSS rules to define document styles"));
    auto editorLayout = new QVBoxLayout(_ui->editorPlaceholder);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->addWidget(&_editor->getEditor());

    connect(&_editor->getEditor(), &QPlainTextEdit::textChanged, this, &StylesPanel::onEditorTextChanged);

    _savedPanePosition = Inkscape::Preferences::get()->getIntLimited(
        "/dialogs/stylePanel/stylesHandlePos", 400, 40, 9999);
    connect(_ui->splitter, &QSplitter::splitterMoved, this, [](int pos, int) {
        Inkscape::Preferences::get()->setInt("/dialogs/stylePanel/stylesHandlePos", pos);
    });
}

// setSizes distributes space proportionally, so the saved handle position
// can only be applied once the splitter's real height is known — that is
// on first show, not in the ctor.
void StylesPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!_panePositionRestored) {
        QTimer::singleShot(0, this, &StylesPanel::restorePanePosition);
    }
}

void StylesPanel::restorePanePosition() {
    const int total = _ui->splitter->height();
    if (total <= 0) return;
    _panePositionRestored = true;
    const int pos = std::min(_savedPanePosition, total);
    _ui->splitter->setSizes({pos, std::max(0, total - pos)});
}

StylesPanel::~StylesPanel() = default;

void StylesPanel::setDocument(SPDocument* document) {
    if (_document == document) return;

    _document = document;
    _stylesheetChanged.disconnect();
    if (document) {
        _stylesheetChanged = document->connectStyleSheetChanged([this] { onStyleContentChanged(); });
    }
    _ui->swatchEditor->setDocument(document);

    // Different document: the editor must adopt its stylesheet even when
    // it holds unsaved edits from the previous document.
    if (!_update.pending()) {
        auto scoped = _update.block();
        _editor->setText(QString::fromStdString(document ? document->styleSheetText() : ""));
    }
}

void StylesPanel::setDesktop(SPDesktop* desktop) {
    _ui->swatchEditor->setDesktop(desktop);
}

void StylesPanel::onStyleContentChanged() {
    // During a pending update (e.g. element creation from onEditorTextChanged)
    // the editor already holds the authoritative text — don't clobber it.
    if (_update.pending()) return;

    // Typing writes through to the element on every keystroke, so the editor
    // and the stylesheet only diverge on external writes (e.g. the swatch
    // editor adding a property). Refresh when they differ.
    const auto text = QString::fromStdString(_document ? _document->styleSheetText() : "");
    if (text == _editor->getText()) return;

    auto scoped = _update.block();
    _editor->setText(text);
}

void StylesPanel::onEditorTextChanged() {
    if (!_document || _update.pending()) return;

    const auto text = _editor->getText();
    auto scoped = _update.block();

    // The stylesheet watcher fires synchronously on element creation inside
    // setStyleSheetText; the pending update keeps the editor as the authority.
    _document->setStyleSheetText(std::string(text.toUtf8().constData()));
    Inkscape::DocumentUndo::maybeDone(_document, "document-styles", RC_("Undo", "Edit document styles"), "");
}

} // namespace Linea::UI
