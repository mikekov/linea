// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt6 to Linea CanvasEvent converter.
 *
 * Translates Qt mouse, keyboard, and wheel events into the CanvasEvent
 * format used by Linea's tool/event system.
 */

#ifndef LINEA_EVENTCONVERTER_H
#define LINEA_EVENTCONVERTER_H

#include <memory>
#include <QPoint>

#include <2geom/point.h>

QT_BEGIN_NAMESPACE
class QMouseEvent;
class QTabletEvent;
class QWheelEvent;
class QKeyEvent;
QT_END_NAMESPACE

namespace Inkscape {
namespace UI::Widget {
class Canvas;
}

struct CanvasEvent;
struct ButtonPressEvent;
struct ButtonReleaseEvent;
struct MotionEvent;
struct KeyPressEvent;
struct KeyReleaseEvent;
struct ScrollEvent;
}

namespace Linea {

/**
 * Converts Qt events to Linea CanvasEvent structures.
 *
 * This replaces the GTK event translation in the original Canvas widget.
 * The CanvasEvent system is toolkit-agnostic once the events are converted.
 */

// Mouse events
std::unique_ptr<Inkscape::ButtonPressEvent> convertMousePress(QMouseEvent* event, Inkscape::UI::Widget::Canvas* canvas);
std::unique_ptr<Inkscape::ButtonReleaseEvent> convertMouseRelease(QMouseEvent* event, Inkscape::UI::Widget::Canvas* canvas);
std::unique_ptr<Inkscape::MotionEvent> convertMotion(QMouseEvent* event, Inkscape::UI::Widget::Canvas* canvas);
std::unique_ptr<Inkscape::ButtonPressEvent> convertTabletPress(QTabletEvent* event, Inkscape::UI::Widget::Canvas* canvas);
std::unique_ptr<Inkscape::ButtonReleaseEvent> convertTabletRelease(QTabletEvent* event, Inkscape::UI::Widget::Canvas* canvas);
std::unique_ptr<Inkscape::MotionEvent> convertTabletMotion(QTabletEvent* event, Inkscape::UI::Widget::Canvas* canvas);

// Wheel/scroll event
std::unique_ptr<Inkscape::ScrollEvent> convertWheel(QWheelEvent* event, Inkscape::UI::Widget::Canvas* canvas);

// Keyboard events
std::unique_ptr<Inkscape::KeyPressEvent> convertKeyPress(QKeyEvent* event);
std::unique_ptr<Inkscape::KeyReleaseEvent> convertKeyRelease(QKeyEvent* event);
bool isTextInputKey(QKeyEvent* event);

} // namespace Linea

#endif // LINEA_EVENTCONVERTER_H
