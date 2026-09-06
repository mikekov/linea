// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PdfPreviewArea implementation.
 */

#include "pdf-preview-area.h"

#include <QPaintEvent>
#include <QPainter>

namespace Linea::UI {

PdfPreviewArea::PdfPreviewArea(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumSize(100, 100);
}

PdfPreviewArea::~PdfPreviewArea() = default;

void PdfPreviewArea::setImage(const QImage& image) {
    _image = image;
    if (!_image.isNull()) {
        setPreviewSize(_image.width(), _image.height());
    }
    update();
}

void PdfPreviewArea::setPreviewSize(int width, int height) {
    _previewWidth = width;
    _previewHeight = height;
    setFixedSize(width, height);
    updateGeometry();
}

QSize PdfPreviewArea::sizeHint() const {
    return {_previewWidth, _previewHeight};
}

void PdfPreviewArea::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);

    if (_image.isNull()) return;

    // Center the image in the widget
    int x = (width() - _image.width()) / 2;
    int y = (height() - _image.height()) / 2;
    p.drawImage(x, y, _image);
}

} // namespace Linea::UI
