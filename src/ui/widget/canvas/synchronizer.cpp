// SPDX-License-Identifier: GPL-2.0-or-later

#include "synchronizer.h"
#include <cassert>

namespace Inkscape::UI::Widget {

Synchronizer::Synchronizer()
{
    /* GTK-specific start
    dispatcher.connect([this] { on_dispatcher(); });
    GTK-specific end */
    /* QT-specific start */
    // qt_post_to_main must be set via set_post_to_main() before use
    /* QT-specific end */
}

void Synchronizer::signalExit() const
{
    auto lock = std::unique_lock(mutables);
    awaken();
    assert(slots.empty());
    exitposted = true;
}

void Synchronizer::runInMain(std::function<void()> const &f) const
{
    auto lock = std::unique_lock(mutables);
    awaken();
    auto s = Slot{ &f };
    slots.emplace_back(&s);
    assert(!exitposted);
    slots_cond.wait(lock, [&] { return !s.func; });
}

void Synchronizer::waitForExit() const
{
    auto lock = std::unique_lock(mutables);
    main_blocked = true;
    while (true) {
        if (!slots.empty()) {
            process_slots(lock);
        } else if (exitposted) {
            exitposted = false;
            break;
        }
        main_cond.wait(lock);
    }
    main_blocked = false;
}

sigc::connection Synchronizer::connectExit(sigc::slot<void()> const &slot)
{
    return signal_exit.connect(slot);
}

void Synchronizer::awaken() const
{
    if (exitposted || !slots.empty()) {
        return;
    }

    if (main_blocked) {
        main_cond.notify_all();
    } else {
        /* GTK-specific start
        const_cast<Glib::Dispatcher&>(dispatcher).emit(); // Glib::Dispatcher is const-incorrect.
        GTK-specific end */
        /* QT-specific start */
        // Qt: Post to main thread using the set callback
        // This should use QMetaObject::invokeMethod with Qt::QueuedConnection
        if (qt_post_to_main) {
            qt_post_to_main([this] { on_dispatcher(); });
        }
        /* QT-specific end */
    }
}

void Synchronizer::on_dispatcher() const
{
    auto lock = std::unique_lock(mutables);
    if (!slots.empty()) {
        process_slots(lock);
    } else if (exitposted) {
        exitposted = false;
        lock.unlock();
        signal_exit.emit();
    }
}

void Synchronizer::process_slots(std::unique_lock<std::mutex> &lock) const
{
    while (!slots.empty()) {
        auto slots_grabbed = std::move(slots);
        lock.unlock();
        for (auto &s : slots_grabbed) {
            (*s->func)();
        }
        lock.lock();
        for (auto &s : slots_grabbed) {
            s->func = nullptr;
        }
        slots_cond.notify_all();
    }
}

} // namespace Inkscape::UI::Widget
