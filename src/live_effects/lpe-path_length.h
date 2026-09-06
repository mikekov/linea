// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LPE_PATH_LENGTH_H
#define INKSCAPE_LPE_PATH_LENGTH_H

/** \file
 * LPE <path_length> implementation.
 */

/*
 * Authors:
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *
 * Copyright (C) 2007-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/effect.h"
#include "live_effects/parameter/text.h"
#include "live_effects/parameter/unit.h"
#include "live_effects/parameter/bool.h"

namespace Inkscape {
namespace LivePathEffect {

class LPEPathLength : public Effect {
public:
    LPEPathLength(LivePathEffectObject *lpeobject);
    ~LPEPathLength() override;

    Geom::Piecewise<Geom::D2<Geom::SBasis> > doEffect_pwd2 (Geom::Piecewise<Geom::D2<Geom::SBasis> > const & pwd2_in) override;

private:
    LPEPathLength(const LPEPathLength&) = delete;
    LPEPathLength& operator=(const LPEPathLength&) = delete;
    ScalarParam scale;
    TextParamInternal info_text;
    UnitParam unit;
    BoolParam display_unit;
};

} //namespace LivePathEffect
} //namespace Inkscape

#endif
