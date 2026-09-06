// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt6 to CanvasEvent converter implementation.
 */

#include "qt-event-converter.h"

#include <QMouseEvent>
#include <QTabletEvent>
#include <QWheelEvent>
#include <QKeyEvent>

#include <2geom/point.h>

#include "ui/widget/events/canvas-event.h"
#include "ui/widget/events/keyvals.h"
#include "ui/widget/canvas.h"
#include "ui/modifier-masks.h"

namespace Linea {

using namespace Inkscape;

namespace {

// Helper functions
template <typename Event>
Geom::Point mapToCanvas(Event* event, UI::Widget::Canvas* canvas) {
    return canvas->windowToCanvas(event->position());
}

int convertModifiers(::Qt::KeyboardModifiers qtMods);
int convertButton(::Qt::MouseButton button);
void addButtonMasks(unsigned& modifiers, ::Qt::MouseButtons buttons);

template <typename Event>
void populateButtonEvent(ButtonEvent& canvasEvent, Event* event, UI::Widget::Canvas* canvas) {
    canvasEvent.button = convertButton(event->button());
    canvasEvent.orig_pos = Geom::Point(event->position().x(), event->position().y());
    canvasEvent.pos = mapToCanvas(event, canvas);
    canvasEvent.modifiers = convertModifiers(event->modifiers());
    canvasEvent.time = event->timestamp();
}

void populateTabletInput(QTabletEvent* event, ExtendedInput& input) {
    input.pressure = event->pressure();
    input.xtilt = event->xTilt();
    input.ytilt = event->yTilt();
}

template <typename Event>
void populateMotionEvent(MotionEvent& canvasEvent, Event* event, UI::Widget::Canvas* canvas) {
    canvasEvent.pos = mapToCanvas(event, canvas);
    canvasEvent.modifiers = convertModifiers(event->modifiers());
    addButtonMasks(canvasEvent.modifiers, event->buttons());
    canvasEvent.time = event->timestamp();
}

int convertModifiers(::Qt::KeyboardModifiers qtMods) {
    int mods = 0;
    if (qtMods & ::Qt::ShiftModifier)   mods |= INK_SHIFT_MASK;
    if (qtMods & ::Qt::ControlModifier) mods |= INK_CONTROL_MASK;
    if (qtMods & ::Qt::AltModifier)     mods |= INK_ALT_MASK;
    if (qtMods & ::Qt::MetaModifier)    mods |= INK_META_MASK;
    if (qtMods & ::Qt::KeypadModifier)  mods |= INK_MOD2_MASK;  // Num Lock
    return mods;
}

int convertButton(::Qt::MouseButton button) {
    switch (button) {
        case ::Qt::LeftButton:   return 1;
        case ::Qt::MiddleButton: return 2;
        case ::Qt::RightButton:  return 3;
        default: return 0;
    }
}

void addButtonMask(unsigned& modifiers, ::Qt::MouseButton button) {
    if (button == ::Qt::LeftButton)   modifiers |= INK_BUTTON1_MASK;
    if (button == ::Qt::MiddleButton) modifiers |= INK_BUTTON2_MASK;
    if (button == ::Qt::RightButton)  modifiers |= INK_BUTTON3_MASK;
}

void addButtonMasks(unsigned& modifiers, ::Qt::MouseButtons buttons) {
    if (buttons & ::Qt::LeftButton)   modifiers |= INK_BUTTON1_MASK;
    if (buttons & ::Qt::MiddleButton) modifiers |= INK_BUTTON2_MASK;
    if (buttons & ::Qt::RightButton)  modifiers |= INK_BUTTON3_MASK;
}

unsigned convertKeyval(int qtKey, bool keypad) {
    // Map Qt keys to INK_KEY values for compatibility with Inkscape tools.
    // Qt reuses the same key codes for keypad and non-keypad keys, distinguishing
    // them via Qt::KeypadModifier.  When keypad is true we return the INK_KEY_KP_*
    // variant so that tool handlers checking for keypad keys work correctly.

    if (qtKey >= ::Qt::Key_A && qtKey <= ::Qt::Key_Z) {
        return qtKey;  // A-Z map directly
    }
    if (qtKey >= ::Qt::Key_0 && qtKey <= ::Qt::Key_9) {
        if (keypad) {
            switch (qtKey) {
                case ::Qt::Key_0: return INK_KEY_KP_0;
                case ::Qt::Key_1: return INK_KEY_KP_1;
                case ::Qt::Key_2: return INK_KEY_KP_2;
                case ::Qt::Key_3: return INK_KEY_KP_3;
                case ::Qt::Key_4: return INK_KEY_KP_4;
                case ::Qt::Key_5: return INK_KEY_KP_5;
                case ::Qt::Key_6: return INK_KEY_KP_6;
                case ::Qt::Key_7: return INK_KEY_KP_7;
                case ::Qt::Key_8: return INK_KEY_KP_8;
                case ::Qt::Key_9: return INK_KEY_KP_9;
            }
        }
        return qtKey;  // 0-9 map directly
    }

    // Keys that have distinct keypad variants
    if (keypad) {
        switch (qtKey) {
            case ::Qt::Key_Space:    return INK_KEY_KP_Space;
            case ::Qt::Key_Tab:      return INK_KEY_KP_Tab;
            case ::Qt::Key_Home:     return INK_KEY_KP_Home;
            case ::Qt::Key_Left:     return INK_KEY_KP_Left;
            case ::Qt::Key_Up:       return INK_KEY_KP_Up;
            case ::Qt::Key_Right:    return INK_KEY_KP_Right;
            case ::Qt::Key_Down:     return INK_KEY_KP_Down;
            case ::Qt::Key_PageUp:   return INK_KEY_KP_Page_Up;
            case ::Qt::Key_PageDown: return INK_KEY_KP_Page_Down;
            case ::Qt::Key_End:      return INK_KEY_KP_End;
            case ::Qt::Key_Insert:   return INK_KEY_KP_Insert;
            case ::Qt::Key_Delete:   return INK_KEY_KP_Delete;
            case ::Qt::Key_Plus:     return INK_KEY_KP_Add;
            case ::Qt::Key_Minus:    return INK_KEY_KP_Subtract;
        }
    }

    // Special keys - map to INK_KEY values
    switch (qtKey) {
        case ::Qt::Key_Escape:      return INK_KEY_Escape;
        case ::Qt::Key_Tab:         return INK_KEY_Tab;
        case ::Qt::Key_Backtab:     return INK_KEY_ISO_Left_Tab;
        case ::Qt::Key_Backspace:   return INK_KEY_BackSpace;
        case ::Qt::Key_Return:      return INK_KEY_Return;
        case ::Qt::Key_Enter:       return INK_KEY_KP_Enter;
        case ::Qt::Key_Insert:      return INK_KEY_Insert;
        case ::Qt::Key_Delete:      return INK_KEY_Delete;
        case ::Qt::Key_Pause:       return INK_KEY_Pause;
        case ::Qt::Key_Print:       return INK_KEY_Print;
        case ::Qt::Key_SysReq:      return INK_KEY_Sys_Req;
        case ::Qt::Key_Clear:       return INK_KEY_Clear;
        case ::Qt::Key_Home:        return INK_KEY_Home;
        case ::Qt::Key_End:         return INK_KEY_End;
        case ::Qt::Key_Left:        return INK_KEY_Left;
        case ::Qt::Key_Up:          return INK_KEY_Up;
        case ::Qt::Key_Right:       return INK_KEY_Right;
        case ::Qt::Key_Down:        return INK_KEY_Down;
        case ::Qt::Key_PageUp:      return INK_KEY_Page_Up;
        case ::Qt::Key_PageDown:    return INK_KEY_Page_Down;
        case ::Qt::Key_Shift:       return INK_KEY_Shift_L;
        case ::Qt::Key_Control:     return INK_KEY_Control_L;
        case ::Qt::Key_Meta:        return INK_KEY_Meta_L;
        case ::Qt::Key_Alt:         return INK_KEY_Alt_L;
        case ::Qt::Key_AltGr:       return INK_KEY_Alt_R;
        case ::Qt::Key_CapsLock:    return INK_KEY_Caps_Lock;
        case ::Qt::Key_NumLock:     return INK_KEY_Num_Lock;
        case ::Qt::Key_ScrollLock:  return INK_KEY_Scroll_Lock;
        case ::Qt::Key_F1:          return INK_KEY_F1;
        case ::Qt::Key_F2:          return INK_KEY_F2;
        case ::Qt::Key_F3:          return INK_KEY_F3;
        case ::Qt::Key_F4:          return INK_KEY_F4;
        case ::Qt::Key_F5:          return INK_KEY_F5;
        case ::Qt::Key_F6:          return INK_KEY_F6;
        case ::Qt::Key_F7:          return INK_KEY_F7;
        case ::Qt::Key_F8:          return INK_KEY_F8;
        case ::Qt::Key_F9:          return INK_KEY_F9;
        case ::Qt::Key_F10:         return INK_KEY_F10;
        case ::Qt::Key_F11:         return INK_KEY_F11;
        case ::Qt::Key_F12:         return INK_KEY_F12;
        case ::Qt::Key_Space:       return INK_KEY_space;
        case ::Qt::Key_Exclam:      return 0x0021;  // !
        case ::Qt::Key_QuoteDbl:    return 0x0022;  // "
        case ::Qt::Key_NumberSign:  return 0x0023;  // #
        case ::Qt::Key_Dollar:      return 0x0024;  // $
        case ::Qt::Key_Percent:     return 0x0025;  // %
        case ::Qt::Key_Ampersand:   return 0x0026;  // &
        case ::Qt::Key_Apostrophe:  return 0x0027;  // '
        case ::Qt::Key_ParenLeft:   return INK_KEY_parenleft;
        case ::Qt::Key_ParenRight:  return INK_KEY_parenright;
        case ::Qt::Key_Asterisk:    return 0x002A;  // *
        case ::Qt::Key_Plus:        return 0x002B;  // +
        case ::Qt::Key_Comma:       return INK_KEY_comma;
        case ::Qt::Key_Minus:       return INK_KEY_minus;
        case ::Qt::Key_Period:      return INK_KEY_period;
        case ::Qt::Key_Slash:       return 0x002F;  // /
        case ::Qt::Key_Less:        return INK_KEY_less;
        case ::Qt::Key_Greater:     return INK_KEY_greater;
        case ::Qt::Key_BracketLeft:  return INK_KEY_bracketleft;
        case ::Qt::Key_BracketRight: return INK_KEY_bracketright;
        case ::Qt::Key_BraceLeft:   return INK_KEY_braceleft;
        case ::Qt::Key_BraceRight:  return INK_KEY_braceright;
        case ::Qt::Key_Menu:        return INK_KEY_Menu;
        default: return static_cast<unsigned>(qtKey);
    }
}

} // anonymous namespace

// Public API functions

std::unique_ptr<ButtonPressEvent> convertMousePress(QMouseEvent* event, UI::Widget::Canvas* canvas) {
    auto canvasEvent = std::make_unique<ButtonPressEvent>();

    populateButtonEvent(*canvasEvent, event, canvas);

    // Add button mask to modifiers to match GTK behavior where state includes button state
    addButtonMask(canvasEvent->modifiers, event->button());

    return canvasEvent;
}

std::unique_ptr<ButtonReleaseEvent> convertMouseRelease(QMouseEvent* event, UI::Widget::Canvas* canvas) {
    auto canvasEvent = std::make_unique<ButtonReleaseEvent>();

    populateButtonEvent(*canvasEvent, event, canvas);

    return canvasEvent;
}

std::unique_ptr<MotionEvent> convertMotion(QMouseEvent* event, UI::Widget::Canvas* canvas) {
    auto canvasEvent = std::make_unique<MotionEvent>();

    populateMotionEvent(*canvasEvent, event, canvas);

    return canvasEvent;
}

std::unique_ptr<ButtonPressEvent> convertTabletPress(QTabletEvent* event, UI::Widget::Canvas* canvas) {
    auto canvasEvent = std::make_unique<ButtonPressEvent>();
    populateButtonEvent(*canvasEvent, event, canvas);
    addButtonMask(canvasEvent->modifiers, event->button());
    populateTabletInput(event, canvasEvent->extinput);
    return canvasEvent;
}

std::unique_ptr<ButtonReleaseEvent> convertTabletRelease(QTabletEvent* event, UI::Widget::Canvas* canvas) {
    auto canvasEvent = std::make_unique<ButtonReleaseEvent>();
    populateButtonEvent(*canvasEvent, event, canvas);
    return canvasEvent;
}

std::unique_ptr<MotionEvent> convertTabletMotion(QTabletEvent* event, UI::Widget::Canvas* canvas) {
    auto canvasEvent = std::make_unique<MotionEvent>();
    populateMotionEvent(*canvasEvent, event, canvas);
    populateTabletInput(event, canvasEvent->extinput);
    return canvasEvent;
}

std::unique_ptr<ScrollEvent> convertWheel(QWheelEvent* event, UI::Widget::Canvas* canvas) {
    auto canvasEvent = std::make_unique<ScrollEvent>();

    canvasEvent->modifiers = convertModifiers(event->modifiers());
    // Note: ScrollEvent doesn't have a time field in this API

    // Delta values - Qt gives angle delta in 8ths of a degree
    canvasEvent->delta = Geom::Point(event->angleDelta().x() / 8.0, event->angleDelta().y() / 8.0);

    return canvasEvent;
}

std::unique_ptr<KeyPressEvent> convertKeyPress(QKeyEvent* event) {
    auto canvasEvent = std::make_unique<KeyPressEvent>();

    canvasEvent->keyval = convertKeyval(event->key(), event->modifiers() & ::Qt::KeypadModifier);
    canvasEvent->keycode = event->nativeScanCode();
    canvasEvent->modifiers = convertModifiers(event->modifiers());
    canvasEvent->qtKey = event->key();
    canvasEvent->qtModifiers = static_cast<unsigned>(event->modifiers().toInt());
    // Note: KeyPressEvent doesn't have is_modifier field - use modifiersChange() method instead

    // Carry the printable text so tools can insert characters without needing
    // the GTK IM context. Only set for keys that actually produce output
    // (text() is empty for modifier-only keys, arrows, function keys, etc.).
    if (!event->text().isEmpty()) {
        canvasEvent->text = event->text().toStdString();
    }

    return canvasEvent;
}

std::unique_ptr<KeyReleaseEvent> convertKeyRelease(QKeyEvent* event) {
    auto canvasEvent = std::make_unique<KeyReleaseEvent>();

    canvasEvent->keyval = convertKeyval(event->key(), event->modifiers() & ::Qt::KeypadModifier);
    canvasEvent->keycode = event->nativeScanCode();
    canvasEvent->modifiers = convertModifiers(event->modifiers());
    canvasEvent->qtKey = event->key();
    canvasEvent->qtModifiers = static_cast<unsigned>(event->modifiers().toInt());
    // Note: KeyReleaseEvent doesn't have is_modifier field - use modifiersChange() method instead

    return canvasEvent;
}

bool isTextInputKey(QKeyEvent* event) {
    if (!event) return false;

    // Check if this is a text input key (letters, numbers, punctuation, etc.)
    // Exclude modifier keys, function keys, navigation keys, etc.

    if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35) {
        // skip function keys (they produce Unicode key text)
        return false;
    }

    if (!event->text().isEmpty()) {
        return true;
    }

    return event->key() == Qt::Key_Left  || event->key() == Qt::Key_Right
        || event->key() == Qt::Key_Up    || event->key() == Qt::Key_Down
        || event->key() == Qt::Key_Home  || event->key() == Qt::Key_End
        || event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete
        || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab;
}

} // namespace Linea
