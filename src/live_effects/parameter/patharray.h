// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_ORIGINALPATHARRAY_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_ORIGINALPATHARRAY_H

#include <vector>

#include <glibmm/ustring.h>

#include <2geom/pathvector.h>

#include "live_effects/effect-enum.h"          // for ParamType
#include "live_effects/parameter/parameter.h"  // for Parameter
#include "object/uri-references.h"             // for URIReference

class SPObject;

namespace Inkscape::LivePathEffect {

class PathAndDirectionAndVisible {
public:
    PathAndDirectionAndVisible(SPObject *owner)
        : ref(owner)
    {
    }

    Glib::ustring href;
    URIReference ref;
    Geom::PathVector _pathvector = {};
    bool reversed = false;
    bool visibled = true;
    
    sigc::connection linked_changed_connection;
    sigc::connection linked_release_connection;
    sigc::connection linked_modified_connection;
};

class PathArrayParam : public Parameter
{
public:
    PathArrayParam(const Glib::ustring &label, const Glib::ustring &tip, const Glib::ustring &key,
                   Inkscape::UI::Widget::Registry *wr, Effect *effect);
    ~PathArrayParam() override;

    PathArrayParam(const PathArrayParam &) = delete;
    PathArrayParam &operator=(const PathArrayParam &) = delete;

    QWidget* param_newWidget() override;
    std::vector<SPObject *> param_get_satellites() override;
    bool param_readSVGValue(char const * strvalue) override;
    Glib::ustring param_getSVGValue() const override;
    Glib::ustring param_getDefaultSVGValue() const override;
    void param_set_default() override;
    void param_update_default(char const * default_value) override{};
    /** Disable the canvas indicators of parent class by overriding this method */
    void param_editOncanvas(SPItem * /*item*/, SPDesktop * /*dt*/) override {};
    /** Disable the canvas indicators of parent class by overriding this method */
    void addCanvasIndicators(SPLPEItem const* /*lpeitem*/, std::vector<Geom::PathVector> & /*hp_vec*/) override {};
    void setFromOriginalD(bool from_original_d){ _from_original_d = from_original_d; update();};
    void allowOnlyBsplineSpiro(bool allow_only_bspline_spiro){ _allow_only_bspline_spiro = allow_only_bspline_spiro; update();};
    std::vector<PathAndDirectionAndVisible*> _vector;
    ParamType paramType() const override { return ParamType::PATH_ARRAY; };

private:
    friend class LPEFillBetweenMany;

    void unlink(PathAndDirectionAndVisible* to);
    void start_listening();
    void setPathVector(SPObject *linked_obj, guint flags, PathAndDirectionAndVisible* to);

    void linked_changed(SPObject *old_obj, SPObject *new_obj, PathAndDirectionAndVisible* to);
    void linked_modified(SPObject *linked_obj, guint flags, PathAndDirectionAndVisible* to);
    void linked_release(SPObject *release, PathAndDirectionAndVisible* to);

    bool _from_original_d = false;
    bool _allow_only_bspline_spiro = false;

    void update();
};

} // namespace Inkscape::LivePathEffect

#endif // INKSCAPE_LIVEPATHEFFECT_PARAMETER_ORIGINALPATHARRAY_H
