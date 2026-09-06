// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_RANDOM_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_RANDOM_H

#include "live_effects/parameter/parameter.h"

namespace Glib {
class ustring;
} // namespace Glib

namespace Inkscape::LivePathEffect {

class RandomParam : public Parameter {
public:
    RandomParam(const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key, 
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect,
                gdouble default_value = 1.0,
                long default_seed = 0,
                bool randomsign = false);

    bool param_readSVGValue(const gchar * strvalue) override;
    Glib::ustring param_getSVGValue() const override;
    Glib::ustring param_getDefaultSVGValue() const override;
    void param_set_default() override;
    void param_set_randomsign(bool randomsign) {_randomsign = randomsign;};
    QWidget* param_newWidget() override;
    double param_get_random_number() { return rand(); };
    void param_set_value(gdouble val, long newseed);
    void param_make_integer(bool yes = true);
    void param_set_range(gdouble min, gdouble max);
    void param_update_default(gdouble default_value);
    void param_update_default(const gchar * default_value) override;
    void resetRandomizer();
    void randomize();
    operator gdouble();
    inline gdouble get_value() { return value; } ;
    ParamType paramType() const override { return ParamType::RANDOM; };

protected:
    long startseed;
    long seed;
    long defseed;

    gdouble value;
    gdouble min;
    gdouble max;
    bool integer;
    bool _randomsign;
    gdouble defvalue;

private:
    void on_value_changed();
    long setup_seed(long);
    gdouble rand();
    Linea::UI::NumberEdit* _spin = nullptr;

    RandomParam(const RandomParam&) = delete;
    RandomParam& operator=(const RandomParam&) = delete;
};

} // namespace Inkscape::LivePathEffect

#endif // INKSCAPE_LIVEPATHEFFECT_PARAMETER_RANDOM_H
