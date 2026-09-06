// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Auto-save
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Re-write of code formerly in inkscape.cpp and originally written by Jon Cruz and others.
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INKSCAPE_AUTOSAVE_H
#define INKSCAPE_AUTOSAVE_H

class LineaApplication;

namespace Inkscape {

class AutoSave final {
private:
    AutoSave() = default;

public:
    AutoSave(const AutoSave &) = delete;
    AutoSave &operator=(const AutoSave &) = delete;
    AutoSave(AutoSave &&) = delete;
    AutoSave &operator=(AutoSave &&) = delete;

    static AutoSave &getInstance()
    {
        static AutoSave theInstance;
        return theInstance;
    }

    static void restart();
    void init(LineaApplication *app);
    void start(); // Includes restarting.
    bool save();

private:
    LineaApplication* _app = nullptr;
};

} // namespace Inkscape

#endif // INKSCAPE_AUTOSAVE_H
