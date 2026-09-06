// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Debug::SimpleEvent - trivial implementation of Debug::Event
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2007 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */


#include <glib.h>
#include <glibmm/ustring.h>
#include <memory>
#include <string>
#include "debug/simple-event.h"
#include "timestamp.h"

namespace Inkscape {

namespace Debug {

std::shared_ptr<std::string> timestamp() {
    gint64 micr = g_get_monotonic_time();
    gchar *value = g_strdup_printf("%.6f", (gdouble)micr / 1000000.0);
    std::shared_ptr<std::string> result = std::make_shared<std::string>(value);
    g_free(value);
    return result;
}

}

}
