// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PdfPreviewArea — a custom QWidget that displays a PDF page preview image.
 *
 * Used by PdfImportDialog as a promoted custom widget in the .ui file.
 * The owner renders a page to a QImage and calls setImage(); this widget
 * paints it centered on its background.
 */

#ifndef LINEA_UI_DIALOG_PDF_PREVIEW_AREA_H
#define LINEA_UI_DIALOG_PDF_PREVIEW_AREA_H

#include <QImage>
#include <QWidget>

namespace Linea::UI {

class PdfPreviewArea : public QWidget {
    Q_OBJECT

public:
    explicit PdfPreviewArea(QWidget* parent = nullptr);
    ~PdfPreviewArea() override;

    /// Set the preview image and trigger a repaint. Pass a null image to clear.
    void setImage(const QImage& image);

    /// Set the preferred preview size (used for sizeHint).
    void setPreviewSize(int width, int height);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage _image;
    int _previewWidth = 200;
    int _previewHeight = 300;
};

} // namespace Linea::UI

#endif // LINEA_UI_DIALOG_PDF_PREVIEW_AREA_H
