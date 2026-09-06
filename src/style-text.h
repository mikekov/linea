// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Style interactions with libnrtype
 *//*
 * Copyright (C) 2018-2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "libnrtype/font-factory.h"
#include "style.h"

// User must free return value.
PangoFontDescription *ink_font_description_from_style(SPStyle const *style);
std::shared_ptr<FontInstance> ink_font_from_style(SPStyle const *style);
