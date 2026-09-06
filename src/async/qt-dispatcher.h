// SPDX-License-Identifier: GPL-2.0-or-later
/** \file Qt Dispatcher
 * Thread-safe dispatcher using Qt's event loop for async operations.
 * Replaces Glib::Dispatcher for Qt-based applications.
 */
#ifndef INKSCAPE_ASYNC_QT_DISPATCHER_H
#define INKSCAPE_ASYNC_QT_DISPATCHER_H

#include <functional>
#include <memory>
#include <QObject>
#include <QMutex>
#include <QMetaObject>

namespace Inkscape {
namespace Async {

/**
 * Qt-based dispatcher for thread-safe communication from worker threads to main thread.
 * Functions queued from any thread will be executed in the thread where the dispatcher was created.
 */
class QtDispatcher : public QObject, public std::enable_shared_from_this<QtDispatcher> {
    Q_OBJECT

public:
    using Func = std::function<void()>;
    using Ptr = std::shared_ptr<QtDispatcher>;

    static Ptr create(QObject* parent = nullptr) {
        return Ptr(new QtDispatcher(parent));
    }

    ~QtDispatcher() override = default;

    /**
     * Queue a function to be executed in the dispatcher's thread.
     * Thread-safe: can be called from any thread.
     */
    void emit_signal();

    /**
     * Connect a callback to be invoked when emit_signal is called.
     * The callback will be executed in the dispatcher's thread.
     */
    void connect(Func callback);

private:
    QtDispatcher(QObject* parent = nullptr);

    mutable QMutex _mutex;
    Func _callback;
};

} // namespace Async
} // namespace Inkscape

#endif // INKSCAPE_ASYNC_QT_DISPATCHER_H
