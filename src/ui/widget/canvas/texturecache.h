// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Extremely basic gadget for re-using textures, since texture creation turns out to be quite expensive.
 * Copyright (C) 2022 PBS <pbs3141@gmail.com>
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_CANVAS_TEXTURECACHE_H
#define INKSCAPE_UI_WIDGET_CANVAS_TEXTURECACHE_H

#include "texture.h"

namespace Inkscape::UI::Widget {

class TextureCache
{
public:
    virtual ~TextureCache() = default;

    static std::unique_ptr<TextureCache> create();

    /**
     * Request a texture of at least the given dimensions.
     * The texture is bound to GL_TEXTURE_2D.
     */
    virtual Texture request(Geom::IntPoint const &dimensions) = 0;

    /**
     * Return a no-longer used texture to the pool.
     */
    virtual void finish(Texture tex) = 0;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_CANVAS_TEXTURECACHE_H
