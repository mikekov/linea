// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *
 * Copyright (C) 2005 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_INKSCAPE_UI_WIDGET_REGISTRY_H
#define SEEN_INKSCAPE_UI_WIDGET_REGISTRY_H

class SPDesktop;

namespace Inkscape {
namespace UI {
namespace Widget {
   
class Registry {
public:
    Registry();
    ~Registry();
    
    bool               isUpdating();
    void               setUpdating (bool);

    SPDesktop *desktop() const { return _desktop; }
    void setDesktop(SPDesktop *desktop);

protected:
    bool _updating;

    SPDesktop *_desktop = nullptr;
};

} // namespace Dialog
} // namespace UI
} // namespace Widget

#endif // SEEN_INKSCAPE_UI_WIDGET_REGISTRY_H
