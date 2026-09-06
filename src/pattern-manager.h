// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_PATTERN_MANAGER_H
#define INKSCAPE_PATTERN_MANAGER_H

#include <QImage>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <cairomm/surface.h>

#include "helper/stock-items.h"
#include "ui/widget/pattern-store.h"
#include "util/statics.h"
#include "style.h"

class SPPattern;
class SPDocument;

namespace Inkscape {

class PatternManager
    : public Util::EnableSingleton<PatternManager, Util::Depends<StockPaintDocuments>>
{
public:
    struct Category {
        const std::string name;
        const std::vector<SPPaintServer*> patterns;
        const bool all;
    };

    // get all stock pattern categories
    const std::vector<std::shared_ptr<Category>>& get_categories();

    // get pattern description item
    std::shared_ptr<Inkscape::UI::Widget::PatternItem> get_item(SPPaintServer* pattern);

    // get pattern image on a solid background for use in UI lists
    QImage get_image(SPPaintServer* pattern, int width, int height, double device_scale);

    // get pattern image on a checkerboard background for use as a larger preview
    QImage get_preview(SPPaintServer* pattern, int width, int height, unsigned int rgba_background, double device_scale);

    Cairo::RefPtr<Cairo::Surface> get_image_surface(SPPaintServer* pattern, int width, int
height, double device_scale);

protected:
    PatternManager();

private:
    void init();
    std::vector<std::shared_ptr<Category>> _categories;
    std::unordered_map<SPPaintServer*, std::shared_ptr<Inkscape::UI::Widget::PatternItem>> _cache;
    std::unique_ptr<SPDocument> _preview_doc;
    std::unique_ptr<SPDocument> _big_preview_doc;
    bool _initialized = false;
};

} // namespace

#endif
