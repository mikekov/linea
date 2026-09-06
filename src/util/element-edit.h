// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#ifndef LINEA_ELEMENT_EDIT_H
#define LINEA_ELEMENT_EDIT_H

#include "util/paint-item-ops.h"

class SPDesktop;
class SPPaintServer;

namespace Linea::Util {

// PaintEditDelegate implementation for editing paint properties of document elements.
class ElementEdit : public PaintEditDelegate {
public:
    ElementEdit(unsigned int tag);
    ~ElementEdit() override = default;

    void set_desktop(SPDesktop* desktop) override;
    SPPaintServer* apply(const Op& op) override;

private:
    SPDesktop* _desktop = nullptr;
    unsigned int _tag = 0;
};

} // namespace Linea::Util

#endif // LINEA_ELEMENT_EDIT_H
