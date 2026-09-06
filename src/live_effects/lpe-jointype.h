// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 * Liam P White
 *
 * Copyright (C) 2014 Authors
 *
 * Released under GNU GPL v2+, read the file COPYING for more information
 */

#ifndef INKSCAPE_LPE_JOINTYPE_H
#define INKSCAPE_LPE_JOINTYPE_H

#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/point.h"
#include "live_effects/parameter/enum.h"

namespace Inkscape {
namespace LivePathEffect {

class LPEJoinType : public Effect {
public:
    LPEJoinType(LivePathEffectObject *lpeobject);
    ~LPEJoinType() override;

    void doOnApply(SPLPEItem const* lpeitem) override;
    void doOnRemove(SPLPEItem const* lpeitem) override;
    void transform_multiply(Geom::Affine const &postmul, bool set) override;
    Geom::PathVector doEffect_path (Geom::PathVector const & path_in) override;

private:
    LPEJoinType(const LPEJoinType&) = delete;
    LPEJoinType& operator=(const LPEJoinType&) = delete;

    ScalarParam line_width;
    EnumParam<unsigned> linecap_type;
    EnumParam<unsigned> linejoin_type;
    //ScalarParam start_lean;
    //ScalarParam end_lean;
    ScalarParam miter_limit;
    BoolParam attempt_force_join;
};

} //namespace LivePathEffect
} //namespace Inkscape

#endif
