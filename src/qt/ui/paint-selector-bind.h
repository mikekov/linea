// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Shared wiring between a PaintSelector widget and a Props::Editor.
 * Used by StylePanel's paint strips and by the toolbar PaintIndicator popup,
 * so the signal → editor->apply() mapping lives in one place.
 */

#ifndef LINEA_UI_PAINT_SELECTOR_BIND_H
#define LINEA_UI_PAINT_SELECTOR_BIND_H

#include <QMetaObject>
#include <vector>
#include <boost/intrusive_ptr.hpp>

#include "util/style-utils.h"

class QObject;
class SPCSSAttr;

namespace Inkscape::Colors {
class Color;
}
namespace Linea::Props {
class Editor;
}

namespace Linea::UI {

class PaintSelector;

/// CSS attribute fragment setting fill/stroke to a flat color.
boost::intrusive_ptr<SPCSSAttr> makePaintCss(bool fill, const Inkscape::Colors::Color& color);

/// Sync a PaintSelector's UI from the current paint properties:
/// placeholder on mixed values, paint mode + fill-rule icon otherwise.
void syncPaintSelector(PaintSelector* selector, const Linea::mixed_property<Linea::PaintProp>& paint,
                       const Linea::mixed_property<SPWindRule>& fillRule, bool fill);

/// Connect all PaintSelector change signals to editor->apply() calls for
/// `fill` (true) or stroke (false). `context` anchors connection lifetime —
/// destroying it severs the links. The returned connections can be severed
/// earlier, e.g. via Props::Binder::track().
std::vector<QMetaObject::Connection> bindPaintSelectorToEditor(PaintSelector* selector, Linea::Props::Editor* editor,
                                                               bool fill, QObject* context);

} // namespace Linea::UI

#endif // LINEA_UI_PAINT_SELECTOR_BIND_H
