// SPDX-License-Identifier: GPL-2.0-or-later
// QT TODO: Stub for SVGViewWidget — GTK-specific Canvas API not available in Qt port.
// This provides the minimal symbol definitions required to link inkview-window.cpp.

#include "svg-view-widget.h"
#include "ui/widget/canvas.h"

namespace Inkscape::UI::View {

SVGViewWidget::SVGViewWidget(SPDocument *) {}
SVGViewWidget::~SVGViewWidget() = default;
void SVGViewWidget::setDocument(SPDocument *) {}
void SVGViewWidget::setResize(int, int) {}
void SVGViewWidget::on_size_allocate(int, int, int) {}
void SVGViewWidget::doRescale() {}

} // namespace Inkscape::UI::View
