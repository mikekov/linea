// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * CanvasFrame implementation.
 */

#include "canvas-frame.h"

#include <QGridLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QStackedWidget>

#include "desktop-events.h"
#include "desktop.h"
#include "document.h"
#include "object/object-set.h"
#include "object/sp-guide.h"
#include "object/sp-namedview.h"
#include "object/sp-page.h"
#include "page-manager.h"
#include "ruler-widget.h"
#include "selection.h"
#include "snap.h"
#include "ui/util.h"
#include "ui/widget/canvas.h"

namespace Linea::UI {

CanvasFrame::CanvasFrame(QWidget* parent)
    : QWidget(parent) {
    setObjectName("CanvasFrame");

    auto layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Row 0: tab strip spanning both columns (hidden until a 2nd desktop is added).
    _tabStrip = new TabStrip(this);
    Inkscape::UI::add_drop_shadow(_tabStrip, 0, 0);
    _tabStrip->setShowLabels(TabStrip::ShowLabels::Always);
    _tabStrip->setShowCloseButton(true);
    _tabStrip->setRearrangingTabs(TabStrip::Rearrange::Externally);
    _tabStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(_tabStrip, 0, 0, 1, 2);

    const int rs = 18; // default ruler size

    // Corner — empty widget, painted by the frame to match ruler edge lines.
    _corner = new QWidget(this);
    _corner->setFixedSize(rs, rs);
    _corner->setObjectName("RulerCorner");
    layout->addWidget(_corner, 1, 0);

    // Horizontal ruler
    _hruler = new RulerWidget(Qt::Horizontal, this);
    _hruler->setRulerSize(rs);
    layout->addWidget(_hruler, 1, 1);

    // Vertical ruler
    _vruler = new RulerWidget(Qt::Vertical, this);
    _vruler->setRulerSize(rs);
    layout->addWidget(_vruler, 2, 0);

    // Canvas stack — one page per open document's canvas
    _stack = new QStackedWidget(this);
    // _stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(_stack, 2, 1);

    // Keep the tab strip on top so its drop shadow renders over the rulers/canvas.
    _tabStrip->raise();

    // Wire ruler drag signals for guide creation.
    connect(_hruler, &RulerWidget::rulerDragStarted, this, &CanvasFrame::onRulerDragStarted);
    connect(_vruler, &RulerWidget::rulerDragStarted, this, &CanvasFrame::onRulerDragStarted);
    connect(_hruler, &RulerWidget::unitChanged, this, &CanvasFrame::setRulersUnit);
    connect(_vruler, &RulerWidget::unitChanged, this, &CanvasFrame::setRulersUnit);
}

CanvasFrame::~CanvasFrame() = default;

void CanvasFrame::addCanvas(Inkscape::UI::Widget::Canvas* canvas) {
    if (!canvas) return;

    _stack->addWidget(canvas);
    canvas->on_mouse_moved = [this](auto pos) { onCanvasMouseMoved(pos); };
    canvas->on_mouse_left = [this]() { onCanvasMouseLeft(); };
    canvas->installEventFilter(this);
}

void CanvasFrame::removeCanvas(Inkscape::UI::Widget::Canvas* canvas) {
    if (!canvas) return;

    _stack->removeWidget(canvas);
}

void CanvasFrame::setCurrentCanvas(SPDesktop* desktop, Inkscape::UI::Widget::Canvas* canvas) {
    if (canvas) {
        _stack->setCurrentWidget(canvas);
    }
    setDesktop(desktop);
}

Inkscape::UI::Widget::Canvas* CanvasFrame::currentCanvas() const {
    return dynamic_cast<Inkscape::UI::Widget::Canvas*>(_stack->currentWidget());
}

void CanvasFrame::onCanvasMouseMoved(QPointF pos) {
    _lastMousePos = pos;
    _hruler->setMarkerVisible(true);
    _vruler->setMarkerVisible(true);
    _hruler->setCursorPosition(pos.x());
    _vruler->setCursorPosition(pos.y());
}

void CanvasFrame::onCanvasMouseLeft() {
    _hruler->setMarkerVisible(false);
    _vruler->setMarkerVisible(false);
}

void CanvasFrame::setRulersVisible(bool visible) {
    if (_rulersVisible == visible) return;

    _rulersVisible = visible;
    _corner->setVisible(visible);
    _hruler->setVisible(visible);
    _vruler->setVisible(visible);
    update();
}

void CanvasFrame::setDesktop(SPDesktop* desktop) {
    if (_desktop == desktop) return;

    _desktop = desktop;

    _pageSelectedConn.disconnect();
    _pageModifiedConn.disconnect();
    _selModifiedConn.disconnect();
    _selChangedConn.disconnect();

    auto canvas = currentCanvas();
    if (!canvas) return;

    auto document = _desktop->getDocument();
    if (!document) return;

    auto& pm = document->getPageManager();
    auto sel = _desktop->getSelection();

    // Set up connections when the desktop changes, so rulers refresh on
    // page/selection modifications (same lazy pattern as CanvasGrid).
    _desktop = desktop;

    // there's only one ruler widget for all documents; reset units
    auto unit = document->getNamedView()->getDisplayUnit();
    _hruler->setUnit(unit);
    _vruler->setUnit(unit);

    _pageSelectedConn = pm.connectPageSelected([this](SPPage const*) { updateRulers(); });
    _pageModifiedConn = pm.connectPageModified([this](SPPage const*) { updateRulers(); });

    if (sel) {
        _selModifiedConn = sel->connectModified([this](Inkscape::Selection const*, int) { updateRulers(); });
        _selChangedConn = sel->connectChanged([this](Inkscape::Selection const*) { updateRulers(); });
    }

    if (_desktop) {
        updateRulers();
    }
}

void CanvasFrame::setRulersUnit(const QString& abbr) {
    if (abbr.isEmpty()) return;

    const auto unit_name = Glib::ustring(abbr.toStdString());
    auto unit = Inkscape::Util::UnitTable::get().getUnit(unit_name);
    if (!unit) return;

    _hruler->setUnit(unit);
    _vruler->setUnit(unit);
    updateRulers();
}

void CanvasFrame::updateRulers() {
    if (!_desktop || !_desktop->document()) return;

    auto canvas = currentCanvas();
    if (!canvas) return;
    
    // Set unit from document display units if none is selected
    auto unit = _hruler->unit();
    if (!_hruler->unit() || !_vruler->unit()) {
        unit = _desktop->document()->getNamedView()->getDisplayUnit();
        _hruler->setUnit(unit);
        _vruler->setUnit(unit);
    }

    auto& pm = _desktop->document()->getPageManager();

    // Compute ruler ranges (port of CanvasGrid::updateRulers)
    Geom::Rect viewbox = canvas->get_area_world();
    Geom::Rect startbox = viewbox;
    if (_desktop->document()->get_origin_follows_page()) {
        auto page_transform = pm.getSelectedPageAffine().inverse() * _desktop->d2w();
        startbox += page_transform.translation();
    }

    auto d2c_scalerot = canvas->get_affine();
    double w2r_scale = 1.0 / (unit->factor * d2c_scalerot.expansionX());
    const auto rulerbox = startbox * Geom::Scale{w2r_scale};

    _hruler->setRange(rulerbox.left(), rulerbox.right());
    if (_desktop->yaxisdown()) {
        _vruler->setRange(rulerbox.top(), rulerbox.bottom());
    } else {
        _vruler->setRange(-rulerbox.top(), -rulerbox.bottom());
    }

    // Page box
    Geom::Point pos(canvas->get_pos());
    auto d2c = d2c_scalerot * Geom::Translate(-pos);
    auto pagebox = pm.getSelectedPageRect() * d2c;
    _hruler->setPage(pagebox.left(), pagebox.right());
    _vruler->setPage(pagebox.top(), pagebox.bottom());

    // Selection box
    Geom::Rect selbox = Geom::IntRect(0, 0, 0, 0);
    if (auto sel = _desktop->getSelection()) {
        if (const auto bbox = sel->preferredBounds()) {
            selbox = *bbox * d2c;
        }
    }
    _hruler->setSelection(selbox.left(), selbox.right());
    _vruler->setSelection(selbox.top(), selbox.bottom());

    // Update cursor position from cached mouse position (don't change marker visibility)
    _hruler->setCursorPosition(_lastMousePos.x());
    _vruler->setCursorPosition(_lastMousePos.y());
}

bool CanvasFrame::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease && _rulerDragActive) {
        auto canvas = currentCanvas();
        if (canvas) {
            canvas->releaseMouse();
        }
        _rulerDragActive = false;
    }
    return QWidget::eventFilter(watched, event);
}

void CanvasFrame::onRulerDragStarted(RulerWidget* ruler, double position) {
    auto canvas = currentCanvas();
    auto desktop = _desktop;
    if (!canvas || !desktop || !ruler) return;

    auto document = desktop->getDocument();
    if (!document) return;

    // Ensure guides are visible so the new guide is shown.
    desktop->getNamedView()->setShowGuides(true);

    // Pulling a guide from a ruler is an explicit request to
    // create and position a guide, so unlock guides before creating it.
    if (desktop->getNamedView()->getLockGuides()) {
        desktop->getNamedView()->setLockGuides(false);
    }

    // Derive orientation from the sender ruler.
    const bool horiz = ruler->orientation() == Qt::Horizontal;

    // Convert the ruler-local position to canvas-local coordinates.
    // Use global coordinates as an intermediary — the canvas (a QOpenGLWidget)
    // may be a native window, which breaks QWidget::mapTo between siblings.
    const QPoint ruler_pos = horiz ? QPoint(static_cast<int>(position), 0)
                                   : QPoint(0, static_cast<int>(position));
    const QPoint canvas_pos = canvas->mapFromGlobal(ruler->mapToGlobal(ruler_pos));

    // Canvas → world → document coordinates.
    const Geom::Point world = canvas->canvas_to_world(Geom::Point(canvas_pos.x(), canvas_pos.y()));
    const Geom::Point dt = desktop->w2d(world);

    // Create the guide: horizontal ruler → horizontal guide, vertical ruler → vertical guide.
    Geom::Point pt1, pt2;
    if (horiz) {
        pt1 = dt;
        pt2 = Geom::Point(dt.x() + 1, dt.y());
    } else {
        pt1 = dt;
        pt2 = Geom::Point(dt.x(), dt.y() + 1);
    }

    auto guide = SPGuide::createSPGuide(document, pt1, pt2);
    if (!guide) return;

    // Begin the drag: look up the guide's canvas item, set drag_type, and grab
    // it so that sp_dt_guide_event handles all subsequent motion/release events.
    sp_dt_guide_begin_drag(guide, canvas, SP_DRAG_TRANSLATE);

    // Grab the mouse on the canvas so Qt keeps delivering events to it
    // even while the cursor is outside the canvas (e.g. over the ruler).
    canvas->grabMouse();
    _rulerDragActive = true;
}

void CanvasFrame::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
/*
    if (!_rulersVisible) return;

    // Draw corner edge lines to match the rulers' canvas-facing edges.
    const int rs = _hruler ? _hruler->rulerSize() : 20;
    const auto tick_color = palette().color(QPalette::Text);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(tick_color, 1));
    // Bottom edge of corner (aligns with HRuler's bottom line)
    painter.drawLine(0, rs - 1, rs - 1, rs - 1);
    // Right edge of corner (aligns with VRuler's right line)
    painter.drawLine(rs - 1, 0, rs - 1, rs - 1);
*/
}

} // namespace Linea::UI
