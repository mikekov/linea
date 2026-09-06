// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LPE_CIRCLE_3PTS_H
#define INKSCAPE_LPE_CIRCLE_3PTS_H

/** \file
 * LPE "Circle through 3 points" implementation
 */

/*
 * Authors:
 *   Maximilian Albert
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 * Copyright (C) Maximilian Albert 2008 <maximilian.albert@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/point.h"

namespace Inkscape {
namespace LivePathEffect {

class LPECircle3Pts : public Effect {
public:
    LPECircle3Pts(LivePathEffectObject *lpeobject);
    ~LPECircle3Pts() override;

    Geom::PathVector doEffect_path (Geom::PathVector const & path_in) override;

private:
    LPECircle3Pts(const LPECircle3Pts&) = delete;
    LPECircle3Pts& operator=(const LPECircle3Pts&) = delete;
};

} //namespace LivePathEffect
} //namespace Inkscape

#endif
