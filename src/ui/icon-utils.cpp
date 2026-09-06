// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Icon utilities
 *
 * Copyright (C) 2025
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "icon-utils.h"

#include <cairo.h>
#include <glibmm/miscutils.h>
#include <giomm/file.h>
#include <string>
#include <QImage>

#include "display/cairo-utils.h"
#include "document.h"
#include "helper/pixbuf-ops.h"
#include "io/file.h"
#include "io/resource.h"
#include "libnrtype/font-factory.h"
#include "object/sp-root.h"
#include "ui/util.h"
#include "util/units.h"

using Inkscape::IO::Resource::SYSTEM;
using Inkscape::IO::Resource::ICONS;

namespace Inkscape {
namespace {

struct IconDocCache : public Util::EnableSingleton<IconDocCache, Util::Depends<FontFactory>> {
    std::unordered_map<std::string, std::unique_ptr<SPDocument>> map;
};

struct IconRenderResult
{
    Cairo::RefPtr<Cairo::Surface> surface;
    Geom::IntPoint size;
};

/**
 * Renders an SVG icon from the specified file name.
 *
 * Returns rendered icon surface and size (or empty result if we could not load the icon).
 */
IconRenderResult render_svg_icon(int icon_size, double scale, const std::string& file_name, const std::string& theme_name) {
    // cache icon SVG documents
    auto& icon_docs = IconDocCache::get().map;
    SPRoot* root = nullptr;

    if (auto it = icon_docs.find(file_name); it != end(icon_docs)) {
        root = it->second->getRoot();
    }

    if (!root) {
        Glib::RefPtr<Gio::File> file;
        std::string full_file_path;
        auto icons_path = Inkscape::IO::Resource::get_path_string(IO::Resource::SYSTEM, IO::Resource::ICONS);
        full_file_path = Glib::build_filename(icons_path, theme_name, file_name + "-symbolic.svg");
        file = Gio::File::create_for_path(full_file_path);

        if (!file->query_exists()) {
            std::cerr << "load_svg_icon: Cannot locate icon file: " << full_file_path << std::endl;
            return {};
        }

        auto document = ink_file_open(file).first;

        if (!document) {
            std::cerr << "load_svg_icon: Could not open document: " << full_file_path << std::endl;
            return {};
        }

        root = document->getRoot();
        if (!root) {
            std::cerr << "load_svg_icon: Could not find SVG element: " << full_file_path << std::endl;
            return {};
        }

        icon_docs[file_name] = std::move(document);
    }

    if (!root) {
        return {};
    }

    int w = root->document->getWidth().value("px");
    int h = root->document->getHeight().value("px");

    Geom::Rect area(0, 0, w, h);
    int dpi = 96 * scale;

    // render document into internal bitmap; returns null on failure
    auto ink_pixbuf = std::unique_ptr<Inkscape::Pixbuf>(sp_generate_internal_bitmap(root->document, area, dpi));
    if (!ink_pixbuf) {
        std::cerr << "load_svg_icon: failed to create pixbuf for: " << file_name << std::endl;
        return {};
    }

    auto icon = IconRenderResult{
        .surface = ink_pixbuf->getSurface(),
        .size = {w, h}
    };

    cairo_surface_set_device_scale(icon.surface->cobj(), scale, scale);

    return icon;
}

} // namespace

QIcon load_svg_icon(const std::string& file_name, int icon_size, double device_pixel_ratio) {
    auto res = render_svg_icon(icon_size, device_pixel_ratio, file_name, "Dash/symbolic/actions");

    if (!res.surface) {
        return QIcon();
    }

    // Convert to QImage
    QImage image = UI::cairo_surface_to_qimage(res.surface->cobj());
    image.setDevicePixelRatio(device_pixel_ratio);
    // Convert to QPixmap
    QPixmap pixmap = QPixmap::fromImage(image);
    // Create QIcon from pixmap
    return QIcon(pixmap);
}

} // namespace Inkscape
