// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ImageWidget - Widget for editing image properties.
 *
 */

#ifndef LINEA_UI_IMAGE_WIDGET_H
#define LINEA_UI_IMAGE_WIDGET_H

#include <memory>
#include <QWidget>

#include "display/cairo-utils.h"
#include "trace/trace.h"
#include "ui/operation-blocker.h"

class SPImage;

namespace Ui {
class ImageWidget;
}

namespace Linea::UI { class TracePanel; }

namespace Linea::Props {
class Binder;
}

namespace Linea::UI {

/**
 * Widget for editing image properties.
 *
 * Uses a grid layout with labels above their content (like NodeWidget).
 * - Row 0: preview + info
 * - Row 1: "URL" label
 * - Row 2: URL content (line edit + change button)
 * - Row 3: action buttons
 * - Row 4: "Aspect ratio" label
 * - Row 5: aspect ratio radio buttons
 * - Row 6: "Rendering" label
 * - Row 7: rendering combo + DPI edit
 */
class ImageWidget : public QWidget {
    Q_OBJECT

public:
    explicit ImageWidget(QWidget* parent = nullptr);
    ~ImageWidget() override;

    void bind(Linea::Props::Binder& binder);

private Q_SLOTS:
    void onChangeImage();
    void onExportImage();
    void onEmbed();
    void onTrace();

private:
    void setImage(SPImage* image);
    void update(std::shared_ptr<const Inkscape::Pixbuf> pixbuf, double opacity = 1.0);
    void resetPreview();
    void rebuildPreview();

    SPDesktop* _desktop = nullptr;
    SPImage* _image = nullptr;
    std::shared_ptr<const Inkscape::Pixbuf> _pixbuf;
    double _imageOpacity = 1.0;
    std::unique_ptr<Ui::ImageWidget> _ui;
    OperationBlocker _update;
    bool             _hasPreview = false;
    TracePanel* _tracePanel = nullptr;
    Inkscape::Trace::TraceFuture _traceFuture;
    Inkscape::Trace::TraceFuture _previewFuture;
};

} // namespace Linea::UI

#endif // LINEA_UI_IMAGE_WIDGET_H
