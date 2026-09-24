// SPDX-License-Identifier: GPL-2.0-or-later
#include "script-editor-panel.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

#include "document-undo.h"
#include "linea-application.h"
#include "script/script-engine.h"
#include "script/script-registry.h"
#include "syntax.h"
#include "ui_script-editor-panel.h"

namespace Linea::UI {
namespace {

class LineNumberArea final : public QWidget {
public:
    LineNumberArea(QPlainTextEdit* editor, QWidget* parent)
        : QWidget(parent)
        , _editor(editor) {
        setObjectName("LineNumberArea");
        setAttribute(Qt::WA_StyledBackground);
        setFont(editor->font());
        setAutoFillBackground(true);
        updateWidth(0);

        connect(editor, &QPlainTextEdit::blockCountChanged, this, &LineNumberArea::updateWidth);
        connect(editor, &QPlainTextEdit::updateRequest, this,
                [this](const QRect& rect, int dy) {
                    if (dy) {
                        scroll(0, dy);
                    } else {
                        update(rect);
                    }
                    if (rect.contains(_editor->viewport()->rect())) {
                        updateWidth(_editor->blockCount());
                    }
                });
        connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this] { update(); });
    }

    void updateWidth(int block_count) {
        int digits = 1;
        for (int value = qMax(1, block_count); value >= 10; value /= 10) {
            ++digits;
        }
        setFixedWidth(fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 10);
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        QPainter painter(this);
        auto c = palette().text().color();
        c.setAlphaF(0.2);
        painter.setPen(c);

        auto block = _editor->cursorForPosition(QPoint(0, 0)).block();
        while (block.isValid()) {
            QTextCursor block_cursor(block);
            const int top = _editor->cursorRect(block_cursor).top();
            if (top > event->rect().bottom()) {
                break;
            }
            if (top >= event->rect().top()) {
                painter.drawText(0, top+1, width() - 5, fontMetrics().height(), Qt::AlignRight,
                                 QString::number(block.blockNumber() + 1));
            }
            block = block.next();
        }
    }

private:
    QPlainTextEdit* _editor;
};

} // namespace

ScriptEditorPanel::ScriptEditorPanel(LineaApplication& app, QWidget* parent)
    : QWidget(parent)
    , _app(app)
    , _ui(std::make_unique<Ui::ScriptEditorPanel>())
    , _editor(Syntax::TextEditView::create(Syntax::SyntaxMode::Lua)) {
    _ui->setupUi(this);

    auto editor = &_editor->getEditor();
    auto mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (const auto point_size = editor->font().pointSize(); point_size > 0) {
        mono.setPointSize(point_size);
    }
    editor->setFont(mono);
    editor->setPlaceholderText(tr("Enter script"));
    auto line_numbers = new LineNumberArea(editor, _ui->editorContainer);
    _ui->editorLayout->addWidget(line_numbers);
    _ui->editorLayout->addWidget(editor, 1);
    _ui->output->setMaximumBlockCount(1000);

    connect(_ui->addButton, &QPushButton::clicked, this, &ScriptEditorPanel::addScript);
    connect(_ui->deleteButton, &QPushButton::clicked, this, &ScriptEditorPanel::deleteScript);
    connect(_ui->runButton, &QPushButton::clicked, this, &ScriptEditorPanel::runCurrentScript);

    for (const auto& name : _app.scriptRegistry().names()) {
        _ui->scriptSelector->addItem(QString::fromStdString(name));
    }
    const auto current = _app.scriptRegistry().current();
    const auto initial_name = current ? QString::fromStdString(current->name) : _ui->scriptSelector->currentText();
    if (!initial_name.isEmpty()) {
        _ui->scriptSelector->setCurrentText(initial_name);
        loadSelectedScript(initial_name);
    }

    connect(_ui->scriptSelector, &QComboBox::currentTextChanged, this, &ScriptEditorPanel::loadSelectedScript);
}

ScriptEditorPanel::~ScriptEditorPanel() = default;

void ScriptEditorPanel::addScript() {
    const auto name = scriptName().trimmed();
    if (name.isEmpty()) {
        _ui->output->appendPlainText(tr("A script name is required."));
        return;
    }

    _app.scriptRegistry().setCurrent(name.toStdString(), scriptText().toStdString());
    if (_ui->scriptSelector->findText(name) < 0) {
        _ui->scriptSelector->addItem(name);
    }
    _ui->scriptSelector->setCurrentText(name);
}

void ScriptEditorPanel::deleteScript() {
    const auto name = scriptName().trimmed();
    const auto index = _ui->scriptSelector->findText(name);
    if (index < 0) {
        return;
    }

    _app.scriptRegistry().removeScript(name.toStdString());
    _ui->scriptSelector->removeItem(index);
    if (_ui->scriptSelector->count() == 0) {
        _editor->getEditor().clear();
        _ui->output->clear();
        return;
    }

    const auto selected = scriptName().trimmed();
    const auto source = _app.scriptRegistry().find(selected.toStdString());
    if (source) {
        _app.scriptRegistry().setCurrent(source->name, source->text);
    }
}

QString ScriptEditorPanel::scriptName() const {
    return _ui->scriptSelector->currentText();
}

QString ScriptEditorPanel::scriptText() const {
    return _editor->getEditor().toPlainText();
}

void ScriptEditorPanel::loadSelectedScript(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    const auto source = _app.scriptRegistry().find(name.toStdString());
    if (!source) {
        return;
    }
    _editor->getEditor().setPlainText(QString::fromStdString(source->text));
    _ui->output->clear();
}

void ScriptEditorPanel::showError(const Script::Error& error) {
    _ui->output->appendPlainText(QString::fromStdString(error.message));
    if (error.line <= 0) {
        return;
    }

    auto cursor = _editor->getEditor().textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, error.line - 1);
    if (error.column > 0) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, error.column - 1);
    }
    _editor->getEditor().setTextCursor(cursor);
    _editor->getEditor().ensureCursorVisible();
}

void ScriptEditorPanel::runCurrentScript() {
    const auto name = scriptName().trimmed();
    if (name.isEmpty()) {
        _ui->output->appendPlainText(tr("A script name is required."));
        return;
    }

    if (_ui->scriptSelector->findText(name) < 0) {
        _ui->scriptSelector->addItem(name);
    }
    _app.scriptRegistry().setCurrent(name.toStdString(), scriptText().toStdString());
    _ui->output->clear();
    auto source = _app.scriptRegistry().current();
    if (!source) {
        return;
    }

    const auto error = _app.scriptEngine().evaluate(*source, [this](std::string_view text) {
        _ui->output->insertPlainText(QString::fromUtf8(text.data(), text.size()));
    });
    if (!error.message.empty()) {
        showError(error);
        return;
    }

    if (_app.scriptEngine().modified() && _app.get_active_document()) {
        Inkscape::DocumentUndo::done(_app.get_active_document(), RC_("Undo", "Run script"), "");
    }
}

} // namespace Linea::UI
