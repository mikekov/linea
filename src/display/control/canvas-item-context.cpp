// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * The context in which a single CanvasItem tree exists. Holds the root node and common state.
 */

#include "canvas-item-context.h"

#include "canvas-item-group.h"
#include "ctrl-handle-manager.h"
#include "ui/widget/canvas.h"

namespace Inkscape {

CanvasItemContext::CanvasItemContext(UI::Widget::Canvas *canvas)
    : _canvas(canvas)
    , _root(new CanvasItemGroup(this))
{
    auto &m = Handles::Manager::get();
    _handles_css = m.getCss();
    _css_updated_conn = m.connectCssUpdated([this] {
        defer([this] {
            _handles_css = Handles::Manager::get().getCss();
            _root->_invalidate_ctrl_handles();
        });
    });
    /*QT TODO
    _device_scale_conn = canvas->property_scale_factor().signal_changed().connect(
        [this] { defer([this] { _root->_invalidate_ctrl_handles(); }); });
    */
}

CanvasItemContext::~CanvasItemContext()
{
    _device_scale_conn.disconnect(); // not using scoped connection to ensure disconnect happens before delete
    delete _root;
}

void CanvasItemContext::snapshot()
{
    assert(!_snapshotted);
    _snapshotted = true;
}

void CanvasItemContext::unsnapshot()
{
    assert(_snapshotted);
    _snapshotted = false;
    _funclog();
}

} // namespace Inkscape
