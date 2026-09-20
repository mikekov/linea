// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Resizable panel hosting a ConsoleWidget
 */

#include "console-panel.h"

#include <QFontMetrics>
#include <QLayout>
#include <QStyle>
#include <QTextDocument>
#include <QToolButton>

#include "console-widget.h"
#include "ui/util.h"
#include "ui_console-panel.h"

namespace Linea::UI {

ConsolePanel::ConsolePanel(QWidget* parent)
    : ResizableEdgeWidget(parent)
    , _ui(std::make_unique<Ui::ConsolePanel>()) {
    _ui->setupUi(this);

    setProperty("class", "console-panel");
    // plain QWidget subclasses don't paint stylesheet backgrounds without this
    setAttribute(Qt::WA_StyledBackground);

    Inkscape::UI::add_drop_shadow(this);

    connect(_ui->console, &ConsoleWidget::commandEntered, this, &ConsolePanel::commandEntered);
    connect(_ui->console, &ConsoleWidget::escapePressed, this, &ConsolePanel::closeRequested);

    // keep the resize step in sync with the console font (monospace:
    // step.x = character advance, step.y = line height); contentsChanged
    // re-measures the real row pitch once the document has rows
    _ui->console->installEventFilter(this);
    connect(_ui->console->document(), &QTextDocument::contentsChanged, this, &ConsolePanel::updateResizeStep);
    updateResizeStep();
}

bool ConsolePanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _ui->console && event->type() == QEvent::FontChange) {
        updateResizeStep();
    }
    return ResizableEdgeWidget::eventFilter(watched, event);
}

void ConsolePanel::updateResizeStep() {
    const QFontMetrics fm = _ui->console->fontMetrics();
    setResizeStep(QSize(qMax(1, fm.horizontalAdvance(u'M')), qMax(1, _ui->console->lineHeight())));
    updateMaxHeight();
}

void ConsolePanel::setMaxLines(int lines) {
    _maxLines = lines;
    updateMaxHeight();
}

QSize ConsolePanel::chromeSize() const {
    const auto margins = _ui->panelLayout->contentsMargins();
    QSize chrome(margins.left() + margins.right(), margins.top() + margins.bottom());
    // console's own overhead between widget bounds and the text rows:
    // frame + document margin + viewport margins
    chrome += QSize(_ui->console->frameWidth() * 2, _ui->console->frameWidth() * 2);
    const int docMargin = 2 * static_cast<int>(_ui->console->document()->documentMargin());
    chrome += QSize(docMargin, docMargin);
    const auto cm = _ui->console->contentsMargins();
    chrome += QSize(cm.left() + cm.right(), cm.top() + cm.bottom());
    return chrome;
}

QSize ConsolePanel::innerSize(QSize panelSize) const {
    QSize inner = (panelSize - chromeSize()).expandedTo(QSize(0, 0));
    // never smaller than one row tall or minColumns() wide
    const QFontMetrics fm = _ui->console->fontMetrics();
    inner.setWidth(qMax(inner.width(), _minColumns * fm.horizontalAdvance(u'M')));
    inner.setHeight(qMax(inner.height(), _ui->console->lineHeight()));
    return inner;
}

void ConsolePanel::showEvent(QShowEvent* event) {
    ResizableEdgeWidget::showEvent(event);
    // programmatic sizes bypass snapResize(); snap to the grid when shown
    const QSize inner = innerSize(size());
    const QSize target = snapAxes(inner, Qt::LeftEdge | Qt::RightEdge | Qt::TopEdge | Qt::BottomEdge) + chromeSize();
    resize(target);
    // the layout placed us for the pre-snap size; re-anchor to the snapped size
    Q_EMIT resized(size());
}

QSize ConsolePanel::snapResize(QSize newSize) const {
    return ResizableEdgeWidget::snapResize(innerSize(newSize)) + chromeSize();
}

void ConsolePanel::updateMaxHeight() {
    if (_maxLines <= 0) {
        setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }
    const int h = _maxLines * _ui->console->lineHeight() + chromeSize().height();
    setMaximumHeight(h);
}

ConsolePanel::~ConsolePanel() = default;

ConsoleWidget* ConsolePanel::console() const {
    return _ui->console;
}

} // namespace Linea::UI
