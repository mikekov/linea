// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_SATELLITEARRAY_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_SATELLITEARRAY_H

#include <memory>
#include <vector>
#include <glibmm/ustring.h>

#include <sigc++/scoped_connection.h>
#include "live_effects/lpeobject.h"
#include "live_effects/parameter/array.h"
#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/satellite-reference.h"

class SPObject;

namespace Inkscape::LivePathEffect {

class SatelliteReference;

class SatelliteArrayParam : public ArrayParam<std::shared_ptr<SatelliteReference>>
{
public:
    SatelliteArrayParam(const Glib::ustring &label, const Glib::ustring &tip, const Glib::ustring &key,
                        Inkscape::UI::Widget::Registry *wr, Effect *effect, bool visible);
    ~SatelliteArrayParam() override;

    SatelliteArrayParam(const SatelliteArrayParam &) = delete;
    SatelliteArrayParam &operator=(const SatelliteArrayParam &) = delete;

    QWidget* param_newWidget() override;
    bool param_readSVGValue(char const * strvalue) override;
    void link(SPObject *to, size_t pos = Glib::ustring::npos);
    void unlink(std::shared_ptr<SatelliteReference> const &to);
    void unlink(SPObject *to);
    bool is_connected(){ return linked_connections.size() != 0; };
    void clear();
    void start_listening();
    void quit_listening();
    ParamType paramType() const override { return ParamType::SATELLITE_ARRAY; };

private:
    void linked_modified(SPObject *linked_obj, guint flags);
    void updatesignal();

    bool _visible{};
    std::vector<sigc::scoped_connection> linked_connections;
    std::vector<SPObject *> param_get_satellites() override;
};

} // namespace Inkscape::LivePathEffect

#endif // INKSCAPE_LIVEPATHEFFECT_PARAMETER_SATELLITEARRAY_H
