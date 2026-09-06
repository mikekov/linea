// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Debug::Logger - debug logging facility
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2005 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_DEBUG_LOGGER_H
#define SEEN_INKSCAPE_DEBUG_LOGGER_H

#include "debug/event.h"

namespace Inkscape {

namespace Debug {

class Logger {
public:
    static void init();

    template <typename EventType, typename... Args>
    inline static void start(Args&&... args) {
        if (_enabled) {
            if (_category_mask[EventType::category()]) {
                _start(EventType(std::forward<Args>(args)...));
            } else {
                _skip();
            }
        }
    }

    inline static void finish() {
        if (_enabled) {
            _finish();
        }
    }

    template <typename EventType, typename... Args>
    inline static void write(Args&&... args) {
        start<EventType, Args...>(std::forward<Args>(args)...);
        finish();
    }

    static void shutdown();

private:
    static bool _enabled;

    static void _start(Event const &event);
    static void _skip();
    static void _finish();

    static bool _category_mask[Event::N_CATEGORIES];
};

}

}

#endif
