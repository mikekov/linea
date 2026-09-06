// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PagesPanel — document pages list with a toolbar.
 *
 * Shows the document's pages in a VirtualTreeList and provides basic
 * page management actions.
 */

#ifndef LINEA_UI_PAGES_PANEL_H
#define LINEA_UI_PAGES_PANEL_H

#include <QImage>
#include <QWidget>
#include <cstdint>
#include <memory>
#include <tuple>
#include <boost/compute/detail/lru_cache.hpp>
#include <sigc++/scoped_connection.h>
#include <2geom/rect.h>

class SPDocument;
class SPDesktop;

QT_BEGIN_NAMESPACE
namespace Ui {
class PagesPanel;
}
QT_END_NAMESPACE

namespace Inkscape {
class Drawing;
}

namespace Linea::UI {
class VirtualTreeList;
}

namespace Linea::UI {

/**
 * Panel that lists the document pages and exposes page management actions.
 */
class PagesPanel : public QWidget {
    Q_OBJECT

public:
    explicit PagesPanel(QWidget* parent = nullptr);
    ~PagesPanel() override;

    void setDesktop(SPDesktop* desktop);
    void setDocument(SPDocument* document);
    void update(SPDocument* document);

private:
    void watchDocument();
    void onSelectionChanged(int index);
    QImage renderPagePreview(int index, qreal dpr);
    Geom::Rect getPageRect(int index) const;
    void rebuildMaxPageRect();
    void updateSelection();

    // manages the Inkscape::Drawing lifecycle for the document preview
    class PagePreviewDrawing;
    std::unique_ptr<PagePreviewDrawing> _preview;

    std::unique_ptr<Ui::PagesPanel> _ui;

    using PreviewKey =
        std::tuple<int, qreal, unsigned, unsigned, std::uint32_t, std::uint32_t, std::uint32_t, bool, bool>;
    boost::compute::detail::lru_cache<PreviewKey, QImage> _preview_cache{100};

    SPDocument* _document = nullptr;
    SPDesktop* _desktop = nullptr;
    VirtualTreeList* _list = nullptr;
    // size of page previews
    int _previewSize = 90;
    int _margin = 2;
    Geom::Rect _maxPageRect;
    sigc::scoped_connection _pagesChanged;
    sigc::scoped_connection _pageSelected;
    sigc::scoped_connection _pageModified;
};

} // namespace Linea::UI

#endif // LINEA_UI_PAGES_PANEL_H
