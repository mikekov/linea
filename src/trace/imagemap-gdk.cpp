// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Conversion between BitmapView (ARGB32 premultiplied) and internal trace map types.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <cassert>
#include "imagemap-gdk.h"

namespace Inkscape {
namespace Trace {

GrayMap bitmapToGrayMap(const BitmapView& bmp) {
    assert(bmp.channels == 4);
    auto map = GrayMap(bmp.width, bmp.height);

    for (int y = 0; y < bmp.height; y++) {
        auto p = bmp.pixels + bmp.rowstride * y;
        for (int x = 0; x < bmp.width; x++) {
            // ARGB32 premultiplied on little-endian: bytes are B, G, R, A
            int b = p[0];
            int g = p[1];
            int r = p[2];
            int a = p[3];
            // Values are premultiplied, so r+g+b is already (R+G+B)*a/255.
            // Composite over white: add 3*(255-a) for the white background.
            int white = 3 * (255 - a);
            map.setPixel(x, y, (r + g + b) + white);
            p += 4;
        }
    }

    return map;
}

QImage grayMapToQImage(const GrayMap& map)
{
    QImage img(map.width, map.height, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < map.height; y++) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < map.width; x++) {
            unsigned char v = map.getPixel(x, y) / 3;
            line[x] = qRgba(v, v, v, 255); // alpha=255: premultiplication is identity
        }
    }

    return img;
}

RgbMap bitmapToRgbMap(const BitmapView& bmp) {
    assert(bmp.channels == 4);
    auto map = RgbMap(bmp.width, bmp.height);

    for (int y = 0; y < bmp.height; y++) {
        auto p = bmp.pixels + bmp.rowstride * y;
        for (int x = 0; x < bmp.width; x++) {
            int b = p[0];
            int g = p[1];
            int r = p[2];
            int a = p[3];
            // Composite premultiplied color over white
            int white = 255 - a;
            map.setPixel(x, y, {
                static_cast<unsigned char>(r + white),
                static_cast<unsigned char>(g + white),
                static_cast<unsigned char>(b + white)
            });
            p += 4;
        }
    }

    return map;
}

std::vector<unsigned char> bitmapToRgb8Packed(const BitmapView& bmp) {
    assert(bmp.channels == 4);
    std::vector<unsigned char> out(3 * bmp.width * bmp.height);
    auto q = out.data();

    for (int y = 0; y < bmp.height; y++) {
        auto p = bmp.pixels + bmp.rowstride * y;
        for (int x = 0; x < bmp.width; x++) {
            int b = p[0];
            int g = p[1];
            int r = p[2];
            int a = p[3];
            int white = 255 - a;
            *(q++) = r + white;
            *(q++) = g + white;
            *(q++) = b + white;
            p += 4;
        }
    }

    return out;
}

QImage indexedMapToQImage(const IndexedMap& map) {
    QImage img(map.width, map.height, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < map.height; y++) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < map.width; x++) {
            auto rgb = map.getPixelValue(x, y);
            line[x] = qRgba(rgb.r, rgb.g, rgb.b, 255);
        }
    }

    return img;
}

QImage argbToQImage(const uint32_t* argb, int width, int height) {
    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < height; y++) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < width; x++) {
            uint32_t px = argb[y * width + x];
            int a = (px >> 24) & 0xff;
            int r = (px >> 16) & 0xff;
            int g = (px >>  8) & 0xff;
            int b =  px        & 0xff;
            line[x] = qPremultiply(qRgba(r, g, b, a));
        }
    }

    return img;
}

QImage bitmapToQImagePreview(const BitmapView& bmp) {
    assert(bmp.channels == 4);
    QImage img(bmp.width, bmp.height, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < bmp.height; y++) {
        auto src = bmp.pixels + bmp.rowstride * y;
        auto dst = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < bmp.width; x++) {
            int b = src[0];
            int g = src[1];
            int r = src[2];
            int a = src[3];
            int white = 255 - a;
            dst[x] = qRgba(r + white, g + white, b + white, 255);
            src += 4;
        }
    }

    return img;
}

std::vector<uint32_t> bitmapToArgb(const BitmapView& bmp) {
    assert(bmp.channels == 4);
    std::vector<uint32_t> argb(bmp.width * bmp.height);

    for (int y = 0; y < bmp.height; y++) {
        auto p = bmp.pixels + bmp.rowstride * y;
        for (int x = 0; x < bmp.width; x++) {
            int b = p[0];
            int g = p[1];
            int r = p[2];
            int a = p[3];
            // Un-premultiply
            if (a > 0 && a < 255) {
                r = (r * 255 + a / 2) / a;
                g = (g * 255 + a / 2) / a;
                b = (b * 255 + a / 2) / a;
            }
            argb[y * bmp.width + x] = (a << 24) | (r << 16) | (g << 8) | b;
            p += 4;
        }
    }

    return argb;
}

std::vector<unsigned char> bitmapToRgba(const BitmapView& bmp) {
    auto argb = bitmapToArgb(bmp);
    std::vector<unsigned char> rgba(4 * bmp.width * bmp.height);

    for (int i = 0; i < bmp.width * bmp.height; i++) {
        uint32_t px = argb[i];
        rgba[4 * i + 0] = (px >> 16) & 0xff; // r
        rgba[4 * i + 1] = (px >>  8) & 0xff; // g
        rgba[4 * i + 2] =  px        & 0xff; // b
        rgba[4 * i + 3] = (px >> 24) & 0xff; // a
    }

    return rgba;
}

} // namespace Trace
} // namespace Inkscape
