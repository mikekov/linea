// SPDX-License-Identifier: GPL-2.0-or-later
/** \file Qt Dispatcher implementation
 */

#include "qt-dispatcher.h"

namespace Inkscape {
namespace Async {

QtDispatcher::QtDispatcher(QObject* parent)
    : QObject(parent) {}

void QtDispatcher::emit_signal() {
    // Capture shared_ptr to keep dispatcher alive until callback executes
    auto self = shared_from_this();

    // Queue the execution of the single callback in the dispatcher's thread
    QMetaObject::invokeMethod(
        this,
        [self]() {
            QMutexLocker locker(&self->_mutex);
            if (self->_callback) {
                self->_callback();
            }
        },
        Qt::QueuedConnection);
}

void QtDispatcher::connect(Func callback) {
    QMutexLocker locker(&_mutex);
    _callback = std::move(callback);
}

} // namespace Async
} // namespace Inkscape
