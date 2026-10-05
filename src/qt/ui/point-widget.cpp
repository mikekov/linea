// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PointWidget — editable SVG path and polyline point data.
 */

#include "point-widget.h"

#include <QComboBox>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <algorithm>
#include <glibmm/ustring.h>

#include "document-undo.h"
#include "object/sp-object.h"
#include "object/sp-path.h"
#include "object/sp-polygon.h"
#include "object/sp-polyline.h"
#include "object/sp-shape.h"
#include "ui/util.h"
#include "ui_point-widget.h"

namespace Linea::UI {

PointWidget::PointWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PointWidget>())
    , _editor(Syntax::TextEditView::create(Syntax::SyntaxMode::SvgPathData)) {
    _ui->setupUi(this);
    _ui->editorLayout->addWidget(&_editor->getEditor(), 1);
    _editorWidget = &_editor->getEditor();
    _editor->setStyle(QString());
    _editor->getEditor().installEventFilter(this);

    setMinimumSize(240, 120);
    setProperty("class", "point-widget");
    setAttribute(Qt::WA_StyledBackground);

    connect(_ui->commitButton, &QPushButton::clicked, this, &PointWidget::onCommit);
    connect(_ui->roundButton, &QPushButton::clicked, this, &PointWidget::roundNumbers);
    connect(_ui->precisionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &PointWidget::setPrecision);
    connect(&_editor->getEditor(), &QPlainTextEdit::textChanged, this,
            [this] { _ui->commitButton->setEnabled(!_updating && _object != nullptr); });

    _ui->precisionCombo->setCurrentIndex(_precision);
    _ui->commitButton->setEnabled(false);
}

PointWidget::~PointWidget() {
    setObject(nullptr);
}

void PointWidget::setObject(SPObject* object) {
    _modified.disconnect();
    _release.disconnect();
    _object = isSupportedObject(object) ? object : nullptr;

    const auto mode =
        dynamic_cast<SPPath*>(_object) ? Syntax::SyntaxMode::SvgPathData : Syntax::SyntaxMode::SvgPolyPoints;
    setSyntaxMode(mode);

    if (!_object) {
        _updating = true;
        _editor->setText(QString());
        _updating = false;
        _ui->infoLabel->setText(tr("No path or points selected"));
        _ui->commitButton->setEnabled(false);
        return;
    }

    _release = _object->connectRelease([this](SPObject* released) {
        if (released == _object) {
            setObject(nullptr);
        }
    });
    _modified = _object->connectModified([this](SPObject*, unsigned) {
        if (!_updating) {
            updateUi();
        }
    });
    updateUi();
}

void PointWidget::updateUi() {
    if (!_object || _updating) {
        return;
    }
    updateEditor();
}

void PointWidget::updateEditor() {
    _pathOriginal = false;
    if (auto path = dynamic_cast<SPPath*>(_object)) {
        const auto original = path->getAttribute("inkscape:original-d");
        _pathOriginal = path->hasPathEffect() && original != nullptr;
    }
    const auto value = _object->getAttribute(attributeName().toUtf8().constData());
    _updating = true;
    _editor->setText(value ? QString::fromUtf8(value) : QString());
    _updating = false;

    const auto path = dynamic_cast<SPPath*>(_object);
    const QString type = path ? tr("Path data") : tr("Points");
    const int nodes = nodeCount();
    _ui->infoLabel->setText(path && nodes > 0 ? tr("%1 — %2 nodes").arg(type).arg(nodes) : type);
    _ui->commitButton->setEnabled(true);
}

void PointWidget::onCommit() {
    commitValue();
    Q_EMIT committed();
}

void PointWidget::commitValue() {
    if (!_object || _updating) {
        return;
    }

    const auto value = _editor->getText();
    const auto attribute = attributeName();
    _updating = true;
    _object->setAttribute(attribute.toUtf8().constData(), value.toUtf8().constData());
    _updating = false;
    Inkscape::DocumentUndo::maybeDone(_object->document, undoKey().toUtf8().constData(),
                                      Inkscape::Util::Internal::ContextString(undoLabel().toUtf8().constData()), "");
    updateUi();
}

void PointWidget::roundNumbers() {
    if (_updating) return;

    auto& editor = _editor->getEditor();
    auto cursor = editor.textCursor();
    const bool hadSelection = cursor.hasSelection();
    const QString source = hadSelection ? cursor.selectedText() : editor.toPlainText();
    const auto rounded = round_numbers(Glib::ustring(source.toStdString()), _precision);
    if (hadSelection) {
        cursor.insertText(QString::fromUtf8(rounded.c_str()));
        editor.setTextCursor(cursor);
    } else {
        editor.setPlainText(QString::fromUtf8(rounded.c_str()));
    }
    commitValue();
}

bool PointWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _editorWidget && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
            (keyEvent->modifiers() & Qt::ShiftModifier)) {
            onCommit();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PointWidget::setPrecision(int precision) {
    _precision = std::clamp(precision, 0, 5);
}

void PointWidget::setSyntaxMode(Syntax::SyntaxMode mode) {
    if (_syntaxMode == mode && _editor) return;

    if (_editor) {
        auto widget = &_editor->getEditor();
        _ui->editorLayout->removeWidget(widget);
        _editorWidget = nullptr;
        _editor.reset();
    }
    _syntaxMode = mode;
    _editor = Syntax::TextEditView::create(mode);
    _editorWidget = &_editor->getEditor();
    _editor->setStyle(QString());
    _ui->editorLayout->addWidget(&_editor->getEditor(), 1);
    _editor->getEditor().installEventFilter(this);
}

QString PointWidget::attributeName() const {
    if (_pathOriginal) {
        return QStringLiteral("inkscape:original-d");
    }
    return dynamic_cast<SPPath*>(_object) ? QStringLiteral("d") : QStringLiteral("points");
}

QString PointWidget::undoKey() const {
    return dynamic_cast<SPPath*>(_object) ? QStringLiteral("path-data") : QStringLiteral("polyline-data");
}

QString PointWidget::undoLabel() const {
    if (attributeName() == "d") {
        return tr("Change path");
    }
    return dynamic_cast<SPPolygon*>(_object) ? tr("Change polygon") : tr("Change polyline");
}

bool PointWidget::isSupportedObject(SPObject* object) const {
    return dynamic_cast<SPPath*>(object) || dynamic_cast<SPPolyLine*>(object) ||
           dynamic_cast<SPPolygon*>(object);
}

int PointWidget::nodeCount() const {
    auto path = dynamic_cast<SPPath*>(_object);
    if (!path) {
        return 0;
    }

    auto curve = path->curveBeforeLPE();
    if (!curve) {
        curve = path->curve();
    }
    std::size_t node_count = curve ? curve->curveCount() : 0;
    if (node_count == 0 && curve && !curve->empty()) {
        for (auto const &subpath : *curve) {
            if (subpath.closed()) {
                // An "empty" closed path has one node (the starting move), but zero curves
                // because the degenerate close curve is discounted.
                node_count += 1;
            }
        }
    }
    return static_cast<int>(node_count);
}

} // namespace Linea::UI
