// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt6 desktop adapter implementation.
 */

#include "desktop.h"

#include <QCursor>

#include "desktop-events.h"
#include "display/control/snap-indicator.h"
#include "display/translucency-group.h"
#include "document-undo.h"
#include "document.h"
#include "layer-manager.h"
#include "linea-window.h"
#include "message-stack.h"
#include "display/control/canvas-item-catchall.h"
#include "display/control/canvas-item-group.h"
#include "object/sp-root.h"      // For SPRoot, invoke_show
#include "object/sp-namedview.h"
#include "object/sp-item.h"
#include "ui/desktop/desktop-widget.h"
#include "selection.h"

#include "display/control/canvas-temporary-item.h"
#include "display/control/canvas-temporary-item-list.h"

// Note: sp-item.h must be included before selection.h because selection.h
// includes object-set.h which uses SPItem::BBoxType nested type
#include "ui/tools/select-tool.h"
#include "ui/tools/node-tool.h"
#include "ui/tools/text-tool.h"
#include "ui/tools/tool-base.h"
#include "ui/widget/events/canvas-event.h"
#include "ui/widget/canvas.h"
#include "display/control/canvas-item-drawing.h"
#include "display/drawing.h"
#include "display/drawing-item.h"
#include "page-manager.h"
#include "object/sp-page.h"
#include "ui/widget/canvas/graphics.h"
#include "props/selection-state-model.h"
#include "ui/tool-factory.h"
// #include "ui/widget/stroke-options.h"
#include "util/units.h"
#include "xml/sp-css-attr.h"

// namespace Inkscape {
// namespace QtUI {

SPDesktop::SPDesktop(SPNamedView* namedview)
    : _namedview(namedview)
    , _document(namedview->document)
{
    g_return_if_fail(_namedview != nullptr);
    g_return_if_fail(_document != nullptr);

    _layerManager = std::make_unique<Inkscape::LayerManager>(this);
    _selection = std::make_unique<Selection>(this);
    _stateModel = std::make_unique<Linea::Props::SelectionStateModel>(this);

    // Set up the text-span scope once: the lambda dynamically checks the
    // current tool, so it returns the selected spans or the span at the
    // insertion point when the text tool is active. The model re-evaluates
    // the scope on each rebuild, including cursor and tool changes.
    _stateModel->setTextScope([this]() -> std::vector<SPItem*> {
        if (auto tool = dynamic_cast<Inkscape::UI::Tools::TextTool*>(currentTool())) {
            return tool->get_subselection(true);
        }
        return {};
    });

    // Write-side scope: when the text tool has a non-empty cursor subselection,
    // return a text-range EditTarget that routes CSS through sp_te_apply_style
    // (splitting tspans to match the selection). Otherwise return nullopt so
    // the Editor falls back to the selection items. The scope is invoked per
    // edit call, so iterators are re-fetched and re-validated every time.
    _stateModel->setTargetScope([this]() -> std::optional<Linea::Props::EditTarget> {
        if (auto tool = dynamic_cast<Inkscape::UI::Tools::TextTool*>(currentTool())) {
            return tool->targetSelection();
        }
        return std::nullopt;
    });

    _message_stack = std::make_unique<MessageStack>();
    _tips_message_context = std::make_unique<MessageContext>(*_message_stack);
    _guides_message_context = std::make_unique<MessageContext>(*_message_stack);

    const auto prefs = Inkscape::Preferences::get();
    _current = prefs->getStyle("/desktop/style");

    dkey = SPItem::display_key_new(1);

    _canvas = std::make_unique<Inkscape::UI::Widget::Canvas>();
    _canvas->setDesktop(this);

    _canvas->setupCanvasItems();
    // Get catchall from canvas (now created in setupCanvasItems with correct z-order)
    _canvas_catchall = _canvas->getCanvasCatchall();
    auto root = _canvas->canvasItemContext()->root();

    // The root should never emit events. The "catchall" should get it!
    // But somehow there are still exceptions, e.g. Ctrl+scroll to zoom.
    root->connect_event(sigc::bind(&sp_desktop_root_handler, this));

    // Connect catchall to root handler for scroll events
    _canvas_catchall->connect_event(sigc::bind(&sp_desktop_root_handler, this));

    _temporary_item_list = std::make_unique<Inkscape::Display::TemporaryItemList>();
    _translucency_group = std::make_unique<Inkscape::Display::TranslucencyGroup>(dkey);
    _snapindicator = std::make_unique<Inkscape::Display::SnapIndicator>(this);

    _selection_changed_connection = _selection->connectChanged([this](auto selection) {
        _selection_changed_connection.block();
        fireStyleChanged(0);
        _selection_changed_connection.unblock();
    });

    _selection_modified_connection = _selection->connectModified([this](auto selection, auto flags) {
        _selection_modified_connection.block();
        fireStyleChanged(flags);
        _selection_modified_connection.unblock();
    });

    _attachDocument();

    setTool("Select");

    schedule_zoom_from_document();

    //todo if needed
    // apply_preferences_canvas_transform(this);
}

SPDesktop::~SPDesktop() {
    _destroy_signal.emit(this);

    // Clean up tools first (they may reference drawing items)
    _currentTool.reset();

    //TODO
    // delete_then_null(_tool);

    // Canvas
    _canvas->set_drawing(nullptr); // Ensures deactivation
    _canvas->set_desktop(nullptr); // Todo: Remove desktop dependency.

    if (_document) {
        _detachDocument();
    }

    _snapindicator.reset();
    _temporary_item_list.reset();
    _stateModel.reset();
    _selection.reset();

    _guides_message_context = nullptr;
}

void SPDesktop::fireStyleChanged(unsigned flags) {
    Linea::PresentationState styleProps;
    Linea::ElementState elProps;
    for (auto obj : _selection->objects()) {
        if (auto item = cast<SPItem>(obj)) {
            Linea::detail::query_style_impl(styleProps, item, Linea::StyleQueryFlags::None);
            Linea::detail::merge_item_properties(elProps, item);
        }
    }
    StyleChangeArgs args{_selection.get(), styleProps, elProps, Linea::PresentationStateDelta{}, flags != 0, flags};
    _signal_style_changed.emit(args);
}

void SPDesktop::fireDesktopStyleChanged() {
    _signal_desktop_style_changed.emit(_current);
}

void SPDesktop::_attachDocument() {
    if (!_document) return;

    // Disable undo tracking during ensureUpToDate (prevents incomplete transactions)
    {
        Inkscape::DocumentUndo::ScopedInsensitive _no_undo(_document);
        _document->ensureUpToDate();
    }

    // Set up reconstruction signals
    _reconstructionStartConn = _document->connectReconstructionStart(
        [this]() { reconstruction_start(); }
    );
    _reconstructionFinishConn = _document->connectReconstructionFinish(
        [this]() { reconstruction_finish(); }
    );

    // Connect y-axis flip signal
    _y_axis_flipped = _document->get_y_axis_flipped().connect(
        [this](double yshift){ handle_y_axis_flip(yshift); }
    );

    // Show document in drawing
    setupCanvasDrawing();
    auto* drawing = _canvasDrawing ? _canvasDrawing->get_drawing() : nullptr;
    if (drawing) {
        if (auto drawing_item = _document->getRoot()->invoke_show(*drawing, dkey, SP_ITEM_SHOW_DISPLAY)) {
            drawing->root()->prependChild(drawing_item);
        }
    }

    // Get namedview and show it (adds page canvas items)
    _namedview = _document->getNamedView();
    _namedview->viewcount++;
    _namedview->show(this);  // KEY: adds page canvas items
    _namedview->setShowGrids(_namedview->getShowGrids());
    _namedview->set_desk_color(this);

    _view_number = _namedview->viewcount;

    /* Ugly hack */
    activate_guides(true);

    /*TODO
    _document_uri_set_connection = _document->connectFilenameSet([this] (auto) {
        _widget->desktopChangedTitle(this);
    });
    _saved_or_modified_conn = _document->connectSavedOrModified([this] {
        _widget->desktopChangedTitle(this);
    });*/

    // set new document before firing signal, so handlers can see new value if they query desktop
    _document_replaced_signal.emit(this, _document);

    sp_namedview_update_layers_from_document(this);
}

void SPDesktop::_detachDocument() {
    _reconstructionStartConn.disconnect();
    _reconstructionFinishConn.disconnect();
    _y_axis_flipped.disconnect();

    // Mirror the show() call in attachDocument: unregister grid/guide/page
    // canvas items so SPGrid::views is empty before the document destructs.
    if (_namedview) {
        _namedview->hide(this);
        _namedview->viewcount--;
        _namedview = nullptr;
    }

    if (_document && _document->getRoot()) {
        // Hide the drawing item
        _document->getRoot()->invoke_hide(dkey);
    }

    // Detach the canvas drawing and remove the old CanvasItemDrawing from the
    // canvas item tree. A new one will be created when a new document is set.
    if (_canvas && _canvasDrawing) {
        _canvas->set_drawing(nullptr);
        _canvasDrawing->unlink();
        _canvasDrawing = nullptr;
    }
}

void SPDesktop::setupCanvasDrawing() {
    if (!_canvas || !_document) return;

    if (_canvasDrawing) return;

    CanvasItemGroup* drawingGroup = _canvas->canvasGroupDrawing();
    if (!drawingGroup) return;

    _canvasDrawing = new CanvasItemDrawing(drawingGroup);

    _canvas->set_drawing(_canvasDrawing->get_drawing());

    // Connect drawing events to desktop handler to match GTK behavior
    _canvasDrawing->connect_drawing_event(sigc::mem_fun(*this, &SPDesktop::drawingHandler));

    _canvasDrawing->get_drawing()->update();
    // updatePageInfo();
}

void SPDesktop::updatePageInfo() {
    if (!_canvas || !_document) return;

    UI::Widget::PageInfo pi;
    for (auto page : _document->getPageManager().getPages()) {
        pi.pages.push_back(page->getDocumentRect());
    }
    if (pi.pages.empty()) {
        // Fall back to document bounds as the single page
        auto bounds = _document->preferredBounds();
        if (bounds) pi.pages.push_back(*bounds);
    }
    // _canvas->setPageInfo(std::move(pi));

    // Colours from namedview
    auto nv = _document->getNamedView();
    if (!nv) return;

    auto& pm = _document->getPageManager();

    auto bgcolor = pm.getBackgroundColor();
    // bgcolor.setOpacity(pm.isCheckerboard() ? 0.0 : 1.0);

    auto dkcolor = nv->getDeskColor();
    // dkcolor.setOpacity(nv->desk_checkerboard() ? 0.0 : 1.0);

    auto toRGBA = [](Inkscape::Colors::Color const &c) -> uint32_t {
        return c.toRGBA();
    };
    _canvas->setColours(toRGBA(bgcolor),
                        toRGBA(dkcolor),
                        toRGBA(pm.getBorderColor()));
}

void SPDesktop::setTool(const std::string& tool_name) {
    _canvas->resetIM();
    _canvas->setAttribute(Qt::WA_InputMethodEnabled, false);

    // Tool should be able to be replaced with itself. See commit 29df5ca05d
    if (_currentTool) {
        _currentTool->switching_away(tool_name);
        _currentTool.reset();
    }

    _currentToolName = tool_name;
    _currentTool.reset(ToolFactory::createObject(this, tool_name));

    if (_currentTool && _currentTool->usesInputMethod()) {
        _canvas->setAttribute(Qt::WA_InputMethodEnabled, true);
    }

    _event_context_changed_signal.emit(this, _currentTool.get());
}

bool SPDesktop::isSelectTool() const {
    return _currentToolName == "Select";
}

bool SPDesktop::isNodeTool() const {
    return _currentToolName == "Node";
}

bool SPDesktop::handleCanvasEvent(CanvasEvent* event) {
    if (!event) return false;

    // Route event to current tool
    if (_currentTool) {
        // Tools expect CanvasEvent references
        return _currentTool->start_root_handler(*event);
    }

    // Default handling when no tool is active
    return false;
}

SPRoot* SPDesktop::root() const {
    return _document ? _document->getRoot() : nullptr;
}

SPItem* SPDesktop::getItemFromListAtPointBottom(std::vector<SPItem*> const &list, Geom::Point const &p) const {
    g_return_val_if_fail(_document != nullptr, nullptr);
    return SPDocument::getItemFromListAtPointBottom(dkey, root(), list, p);
}

Geom::Parallelogram SPDesktop::get_display_area() const {
    // Get the canvas widget dimensions in window coordinates
    if (!_canvas) {
        return Geom::Parallelogram(Geom::Rect(0, 0, 1, 1));
    }

    auto const world = _canvas->get_area_world();
    Geom::Rect viewbox(Geom::Point(static_cast<double>(world.min().x()), static_cast<double>(world.min().y())),
                       Geom::Point(static_cast<double>(world.max().x()), static_cast<double>(world.max().y())));
    return Geom::Parallelogram(viewbox) * w2d();
}

bool SPDesktop::isWithinViewport(SPItem const *item) const {
    auto const bbox = item->desktopVisualBounds();
    if (!bbox) {
        return false;
    }
    auto const viewport = get_display_area();
    return viewport.intersects(*bbox);
}

bool SPDesktop::itemIsHidden(SPItem const *item) const {
    return item->isHidden(dkey);
}


/// Called when document is starting to be rebuilt.
void SPDesktop::reconstruction_start()
{
    auto layer = layerManager().currentLayer();
    _reconstruction_old_layer_id = layer->getId() ? layer->getId() : "";
    layerManager().reset();

    getSelection()->clear();
}

/// Called when document rebuild is finished.
void SPDesktop::reconstruction_finish()
{
    g_debug("Desktop, finishing reconstruction\n");
    if (!_reconstruction_old_layer_id.empty()) {
        if (auto const newLayer = getNamedView()->document->getObjectById(_reconstruction_old_layer_id)) {
            layerManager().setCurrentLayer(newLayer);
        }

        _reconstruction_old_layer_id.clear();
    }
    g_debug("Desktop, finishing reconstruction end\n");
}

void SPDesktop::handle_y_axis_flip(double yshift) {
    // selection is repainted in a wrong location, so clearing it for now
    if (!_selection->isEmpty()) {
        _selection->clear();
    }

    auto offset = _current_affine.getOffset();
    auto zoom = _current_affine.getZoom();
    _current_affine.setScale(Geom::Scale(zoom, yaxisdir() * zoom));
    _current_affine.setOffset(Geom::Point(offset.x(), offset.y() + zoom * yshift));
    set_display_area(false);
}

SPNamedView* SPDesktop::namedView() const {
    return _document ? _document->getNamedView() : nullptr;
}

Geom::Affine const &SPDesktop::doc2dt() const
{
    assert(_document);
    return _document->doc2dt();
}

Geom::Affine const &SPDesktop::dt2doc() const
{
    assert(_document);
    return _document->dt2doc();
}

void SPDesktop::setZoom(double zoom) {
    // if (_canvas) {
    //     _canvas->setZoom(zoom);
    //     signal_zoom_changed.emit(zoom);
    // }
}

void SPDesktop::zoomIn() {
    auto w = Geom::Rect(_canvas->get_area_world()).midpoint();
    zoom_relative(w2d(w), M_SQRT2, true);
    // if (_canvas) {
    //     _canvas->zoomIn();
    //     signal_zoom_changed.emit(currentZoom());
    // }
}

void SPDesktop::zoomOut() {
    auto w = Geom::Rect(_canvas->get_area_world()).midpoint();
    zoom_relative(w2d(w), 1.0 / M_SQRT2, true);
    // if (_canvas) {
    //     _canvas->zoomOut();
    //     signal_zoom_changed.emit(currentZoom());
    // }
}

void SPDesktop::zoomFit() {
    // if (_canvas) {
    //     _canvas->zoomFit();
    //     signal_zoom_changed.emit(currentZoom());
    // }
}

void SPDesktop::emit_control_point_selected(Inkscape::UI::ControlPointSelection* selection) {
    signal_control_point_selected.emit(selection);
}

void SPDesktop::emit_gradient_stop_selected(SPStop* stop) {
    signal_gradient_stop_selected.emit(stop);
}

Geom::Point SPDesktop::current_center() const {
    if (!_canvas) return Geom::Point();
    return Geom::Rect(_canvas->get_area_world()).midpoint() * _current_affine.w2d();
}

Geom::Point SPDesktop::point() const {
    if (!_canvas) {
        return Geom::Point();
    }

    const auto mouse = _canvas->get_last_mouse();
    auto pt = mouse ? *mouse : Geom::Point(_canvas->get_dimensions()) / 2.0;
    auto dt_pos = w2d(_canvas->canvas_to_world(pt));

    if (const auto document = doc()) {
        if (const auto unit = document->getDisplayUnit()) {
            double x = Inkscape::Util::Quantity(dt_pos.x(), "px").value(unit);
            double y = Inkscape::Util::Quantity(dt_pos.y(), "px").value(unit);
            x = std::round(x);
            y = std::round(y);
            return Geom::Point(
                Inkscape::Util::Quantity(x, unit).value("px"),
                Inkscape::Util::Quantity(y, unit).value("px")
            );
        }
    }

    return dt_pos;
}

bool SPDesktop::quick_zoomed() const {
    return _quick_zoom_enabled;
}

void SPDesktop::setToolboxFocusTo(const std::string& id) {
    // TODO: Implement toolbox focus
    (void)id;
}

void SPDesktop::setToolboxAdjustmentValue(const std::string& id, double value) {
    // TODO: Implement toolbox adjustment
    (void)id;
    (void)value;
}

void SPDesktop::quick_preview(bool active) {
    // TODO: Implement quick preview
    (void)active;
}

void SPDesktop::zoom_quick(bool active) {
    // TODO: Implement zoom quick
    (void)active;
}

void SPDesktop::emit_text_cursor_moved(Inkscape::UI::Tools::TextTool* tool) {
    signal_text_cursor_moved.emit(tool);
}

/*
 * pinch zoom
 */

void SPDesktop::on_zoom_begin() {
    _begin_zoom = current_zoom();
}

void SPDesktop::on_zoom_scale(double scale) {
    auto widget_point = _canvas->get_last_mouse().value_or(_canvas->get_dimensions() / 2);
    auto world_point = _canvas->canvas_to_world(widget_point);
    double new_zoom = current_zoom() * scale;
    zoom_absolute(w2d(world_point), new_zoom, true);
}

void SPDesktop::on_zoom_end() {
    _begin_zoom.reset();
}

void SPDesktop::setToolboxAdjustmentValueInt(const std::string& id, int value) {
    // TODO: Implement toolbox adjustment (int version)
    (void)id;
    (void)value;
}

SPItem* SPDesktop::itemAtPoint(const Geom::Point& point, bool intoGroups) const {
    // Use picking on the Drawing tree
    DrawingItem* ditem = drawingItemAtPoint(point);
    if (ditem) {
        return ditem->getItem();
    }
    return nullptr;
}

DrawingItem* SPDesktop::drawingItemAtPoint(const Geom::Point& point) const {
    if (!_canvasDrawing) return nullptr;

    // Pick on the Drawing root
    // This uses Inkscape's picking system
    auto* drawing = _canvasDrawing->get_drawing();
    if (!drawing) return nullptr;

    auto* root = drawing->root();
    if (!root) return nullptr;

    // Pick with small delta for precise selection
    return drawing->pick(point, 2.0, 0);
}

bool SPDesktop::drawingHandler(CanvasEvent const& event, DrawingItem* drawingItem) {
    // Route event to current tool
    if (!_currentTool) return false;

    if (drawingItem) {
        SPItem* item = drawingItem->getItem();
        return _currentTool->start_item_handler(item, event);
    } else {
        return _currentTool->start_root_handler(event);
    }
}

void SPDesktop::setCursor(const QCursor& cursor) {
    if (_canvas) {
        _canvas->setCursor(cursor);
        _waiting_cursor = false;
    }
}

void SPDesktop::setDefaultCursor() {
    if (_canvas) {
        _canvas->unsetCursor();
    }
}

Glib::ustring SPDesktop::get_toolbar_by_name(Glib::ustring const &name) {
    // TODO: Implement toolbar lookup
    (void)name;
    return "";
}

void SPDesktop::setWaitingCursor() {
    if (_canvas) {
        _canvas->setCursor(QCursor(Qt::WaitCursor));
        _waiting_cursor = true;
    }
}

void SPDesktop::clearWaitingCursor() {
    if (_waiting_cursor && _currentTool) {
        _currentTool->use_tool_cursor();
    }
}

/**
 * Zoom to the given absolute zoom level
 *
 * @param center - Point we want to zoom in on
 * @param zoom - Absolute amount of zoom (1.0 is 100%)
 * @param keep_point - Keep center fixed in the desktop window.
 */
void SPDesktop::zoom_absolute(const Geom::Point& center, double zoom, bool keep_point) {
    Geom::Point w = d2w(center); // Must be before zoom changed.
    if(!keep_point) {
        w = Geom::Rect(_canvas->get_area_world()).midpoint();
    }
    zoom = CLAMP (zoom, SP_DESKTOP_ZOOM_MIN, SP_DESKTOP_ZOOM_MAX);
    _current_affine.setScale( Geom::Scale(zoom, yaxisdir() * zoom) );
    set_display_area( center, w );
}

void SPDesktop::zoom_relative(const Geom::Point& center, double zoom, bool keep_point) {
    double new_zoom = _current_affine.getZoom() * zoom;
    this->zoom_absolute(center, new_zoom, keep_point);
    // double new_zoom = _current_zoom * zoom;
    // setZoom(new_zoom);
}

/**
 * Zoom in to an absolute realworld ratio, e.g. 1:1 physical screen units
 *
 * @param center - Point we want to zoom in on.
 * @param ratio - Absolute physical zoom ratio.
 */
void SPDesktop::zoom_realworld(Geom::Point const &center, double ratio) {
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    double correction = prefs->getDouble("/options/zoomcorrection/value", 1.0);
    this->zoom_absolute(center, ratio * correction, false);
}

/**
 * Does all the dirty work in setting the display area.
 * _current_affine must already be full updated (including offset).
 * log: if true, save transform in transform stack for reuse.
 */
void SPDesktop::set_display_area(bool log)
{
    // Save the transform
    if (log) {
        transforms_past.push_front(_current_affine);
        // if we do a logged transform, our transform-forward list is invalidated, so delete it
        transforms_future.clear();
    }

    // Scroll
    _canvas->set_pos(_current_affine.getOffset());
    _canvas->set_affine(_current_affine.d2w()); // For CanvasItems.

    // Update perspective lines if we are in the 3D box tool (so that infinite ones are shown correctly).
    // if (auto const boxtool = dynamic_cast<Inkscape::UI::Tools::Box3dTool*>(_tool.get())) {
        // boxtool->_vpdrag->updateLines();
    // }

    // Update GUI (TODO: should be handled by CanvasGrid).
    const auto zoom = _current_affine.getZoom();
    const auto rotation = Geom::deg_from_rad(current_rotation().angle());
    _widget->updateRulers();
    /* QT TODO
    // _widget->get_canvas_grid()->updateScrollbars(_current_affine.getZoom());
    */
    _widget->updateZoom(zoom);
    _widget->updateRotation(rotation);
    //*/
    signal_zoom_changed.emit(_current_affine.getZoom());  // Observed by path-manipulator to update arrows.
}

/**
 * Map the drawing to the window so that 'c' lies at 'w' where where 'c'
 * is a point on the canvas and 'w' is position in window in screen pixels.
 */
void SPDesktop::set_display_area(Geom::Point const &c, Geom::Point const &w, bool log)
{
    // The relative offset needed to keep c at w.
    Geom::Point offset = d2w(c) - w;
    _current_affine.addOffset(offset);
    set_display_area(log);
}

/**
 * Map the center of rectangle 'r' (which specifies a non-rotated region of the
 * drawing) to lie at the center of the window. The zoom factor is calculated such that
 * the edges of 'r' closest to 'w' are 'border' length inside of the window (if
 * there is no rotation). 'r' is in document pixel units, 'border' is in screen pixels.
 */
void SPDesktop::set_display_area(Geom::Rect const &r, Geom::Coord border, bool log) {
    if (!_canvas) return;

    // Get window dimensions
    QRect widgetRect = QRect(QPoint(0, 0), _canvas->size());
    Geom::Rect window_rect(Geom::Point(0, 0), Geom::Point(widgetRect.width(), widgetRect.height()));

    // Shrink window to account for border padding
    window_rect.expandBy(-border);

    // Calculate zoom to fit rectangle a in window
    double zoom = 1.0;
    if (r.width() * window_rect.height() > r.height() * window_rect.width()) {
        zoom = window_rect.width() / r.width();
    } else {
        zoom = window_rect.height() / r.height();
    }

    // Clamp zoom
    zoom = std::clamp(zoom, SP_DESKTOP_ZOOM_MIN, SP_DESKTOP_ZOOM_MAX);
    _current_affine.setScale( Geom::Scale(zoom, yaxisdir() * zoom) );
    // Zero offset, actual offset calculated later.
    _current_affine.setOffset( Geom::Point( 0, 0 ) );

    set_display_area( r.midpoint(), window_rect.midpoint(), log );
}

/**
 * Tell widget to let zoom widget grab keyboard focus.
 */
void
SPDesktop::zoom_grab_focus()
{
    //TODO
    // _widget->letZoomGrabFocus();
}

/**
 * Tell widget to let rotate widget grab keyboard focus.
 */
void
SPDesktop::rotate_grab_focus()
{
    // TODO
    // _widget->letRotateGrabFocus();
}

void SPDesktop::rotate(double angle) {
    const Geom::Rect canvas = _canvas->get_area_world();
    Geom::Point midpoint = w2d(canvas.midpoint()); // Midpoint of drawing on canvas.
    rotate_absolute_center_point(midpoint, Geom::rad_from_deg(angle));
}

/**
 * Set new rotation, keeping the point 'c' fixed in the desktop window.
 *
 * @param c Point in desktop coordinates
 * @param rotate Angle in clockwise direction
 */
void SPDesktop::rotate_absolute_keep_point(Geom::Point const &c, double rotate)
{
    auto const w = d2w(c); // Must be before rotate changed.
    _current_affine.setRotate(rotate);
    set_display_area(c, w);
}

/**
 * Rotate keeping the point 'c' fixed in the desktop window.
 *
 * @param c Point in desktop coordinates
 * @param rotate Angle in clockwise direction
 */
void SPDesktop::rotate_relative_keep_point(Geom::Point const &c, double rotate)
{
    auto const w = d2w(c); // Must be before rotate changed.
    _current_affine.addRotate(rotate);
    set_display_area(c, w);
}

/**
 * Set new rotation, aligning the point 'c' to the center of desktop window.
 *
 * @param c Point in desktop coordinates
 * @param rotate Angle in clockwise direction
 */
void SPDesktop::rotate_absolute_center_point(Geom::Point const &c, double rotate)
{
    _current_affine.setRotate(rotate);
    auto const viewbox = _canvas->get_area_world();
    set_display_area(c, viewbox.midpoint());
}

/**
 * Rotate aligning the point 'c' to the center of desktop window.
 *
 * @param c Point in desktop coordinates
 * @param rotate Angle in clockwise direction
 */
void SPDesktop::rotate_relative_center_point(Geom::Point const &c, double rotate)
{
    _current_affine.addRotate(rotate);
    auto const viewbox = _canvas->get_area_world();
    set_display_area(c, viewbox.midpoint());
}

/**
 * Set new flip direction, keeping the point 'c' fixed in the desktop window.
 *
 * @param c Point in desktop coordinates
 * @param flip Direction the canvas will be set as.
 */
void
SPDesktop::flip_absolute_keep_point (Geom::Point const &c, CanvasFlip flip)
{
    Geom::Point w = d2w(c); // Must be before flip.
    _current_affine.setFlip(flip);
    set_display_area(c, w);
}

/**
 * Flip direction, keeping the point 'c' fixed in the desktop window.
 *
 * @param c Point in desktop coordinates
 * @param flip Direction to flip canvas
 */
void
SPDesktop::flip_relative_keep_point (Geom::Point const &c, CanvasFlip flip)
{
    Geom::Point w = d2w(c); // Must be before flip.
    _current_affine.addFlip(flip);
    set_display_area(c, w);
}

/**
 * Set new flip direction, aligning the point 'c' to the center of desktop window.
 *
 * @param c Point in desktop coordinates
 * @param flip Direction the canvas will be set as.
 */
void
SPDesktop::flip_absolute_center_point (Geom::Point const &c, CanvasFlip flip)
{
    _current_affine.setFlip(flip);
    Geom::Rect viewbox = _canvas->get_area_world();
    set_display_area(c, viewbox.midpoint());
}

/**
 * Flip direction, aligning the point 'c' to the center of desktop window.
 *
 * @param c Point in desktop coordinates
 * @param flip Direction to flip canvas
 */
void
SPDesktop::flip_relative_center_point (Geom::Point const &c, CanvasFlip flip)
{
    _current_affine.addFlip(flip);
    Geom::Rect viewbox = _canvas->get_area_world();
    set_display_area(c, viewbox.midpoint());
}

bool
SPDesktop::is_flipped (CanvasFlip flip)
{
    return _current_affine.isFlipped(flip);
}

/**
 * Scroll canvas by to a particular point (window coordinates).
 */
void
SPDesktop::scroll_absolute (Geom::Point const &point)
{
    _canvas->set_pos(point);
    _current_affine.setOffset( point );

    /*  update perspective lines if we are in the 3D box tool (so that infinite ones are shown correctly) */
    // if (auto const boxtool = dynamic_cast<Inkscape::UI::Tools::Box3dTool*>(_tool.get())) {
        // boxtool->_vpdrag->updateLines();
    // }

    _widget->updateRulers();
    /* QT TODO
    _widget->get_canvas_grid()->updateScrollbars(_current_affine.getZoom());
    */
}

/**
 * Scroll canvas by specific coordinate amount (window coordinates).
 */
void
SPDesktop::scroll_relative (Geom::Point const &delta)
{
    Geom::Rect const viewbox = _canvas->get_area_world();
    scroll_absolute( viewbox.min() - delta );
}

/**
 * Scroll canvas by specific coordinate amount in svg coordinates.
 */
void
SPDesktop::scroll_relative_in_svg_coords (double dx, double dy)
{
    double scale = _current_affine.getZoom();
    scroll_relative(Geom::Point(dx*scale, dy*scale));
}

/**
 * Scroll screen so as to keep point 'p' visible in window.
 * (Used, for example, during spellcheck.)
 * 'p': The point in desktop coordinates.
 */
// Todo: Eliminate second argument and return value.
bool SPDesktop::scroll_to_point(Geom::Point const &p, double)
{
    auto prefs = Inkscape::Preferences::get();

    // autoscrolldistance is in screen pixels.
    double const autoscrolldistance = prefs->getIntLimited("/options/autoscrolldistance/value", 0, -1000, 10000);

    auto w = Geom::Rect(_canvas->get_area_world()); // Window in screen coordinates.
    w.expandBy(-autoscrolldistance);  // Shrink window

    auto const c = d2w(p);  // Point 'p' in screen coordinates.
    if (!w.contains(c)) {
        auto const c2 = w.clamp(c); // Constrain c to window.
        scroll_relative(c2 - c);
        return true;
    }

    return false;
}

/**
 * Set display area in only the width dimension.
 */
void SPDesktop::set_display_width(Geom::Rect const &rect, Geom::Coord border) {
    if (rect.width() < 1.0)
        return;
    auto const center_y = current_center().y();
    set_display_area(Geom::Rect(
        Geom::Point(rect.left(), center_y),
        Geom::Point(rect.width(), center_y)), border);
}

/**
 * Centre Rect, without zooming
 */
void SPDesktop::set_display_center(Geom::Rect const &rect) {
    zoom_absolute(rect.midpoint(), this->current_zoom(), false);
}

// CanvasItemGroup accessors - delegate to Canvas
Inkscape::CanvasItemGroup* SPDesktop::getCanvasControls() const {
    return _canvas ? _canvas->canvasGroupControls() : nullptr;
}

Inkscape::CanvasItemGroup* SPDesktop::getCanvasTemp() const {
    return _canvas ? _canvas->canvasGroupTemp() : nullptr;
}

Inkscape::CanvasItemGroup* SPDesktop::getCanvasGuides() const {
    return _canvas ? _canvas->canvasGroupGuides() : nullptr;
}

Inkscape::CanvasItemCatchall* SPDesktop::getCanvasCatchall() const {
    return _canvas_catchall;
}

Inkscape::CanvasItemGroup* SPDesktop::getCanvasSketch() const {
    return _canvas ? _canvas->canvasGroupSketch() : nullptr;
}

/* These methods help for temporarily showing things on-canvas.
 * The *only* valid use of the TemporaryItem* that you get from add_temporary_canvasitem
 * is when you want to prematurely remove the item from the canvas, by calling
 * desktop->remove_temporary_canvasitem(tempitem).
 */
/**
 * One should *not* keep a reference to the SPCanvasItem, the temporary item code will
 * delete the object for you and the reference will become invalid without you knowing it.
 * It is perfectly safe to ignore the returned pointer: the object is deleted by itself, so don't delete it elsewhere!
 * The *only* valid use of the returned TemporaryItem* is as argument for SPDesktop::remove_temporary_canvasitem,
 * because the object might be deleted already without you knowing it.
 * move_to_bottom = true by default so the item does not interfere with handling of other items on the canvas like nodes.
 */
Display::TemporaryItem* SPDesktop::add_temporary_canvasitem(CanvasItem* item, int lifetime_msecs, bool move_to_bottom) {
    if (move_to_bottom) {
        item->lower_to_bottom();
    }

    return _temporary_item_list->add_item(item, lifetime_msecs);
}

/** It is perfectly safe to call this function while the object has already been deleted due to a timeout.
*/
// Note: This function may free the wrong temporary item if it is called on a freed pointer that
// has had another TemporaryItem reallocated in its place.
void SPDesktop::remove_temporary_canvasitem(Inkscape::Display::TemporaryItem *tempitem)
{
    // check for non-null temporary_item_list, because during destruction of desktop, some destructor might try to access this list!
    if (tempitem && _temporary_item_list) {
        _temporary_item_list->delete_item(tempitem);
    }
}

// Layer manager access
Inkscape::LayerManager& SPDesktop::layerManager() const {
    if (!_layerManager) {
        _layerManager = std::make_unique<Inkscape::LayerManager>(const_cast<SPDesktop*>(this));
    }
    return *_layerManager;
}

// Item picking (delegates to document)
SPItem* SPDesktop::getItemAtPoint(Geom::Point const& p, bool into_groups, SPItem* upto) const {
    if (!_document) return nullptr;
    return _document->getItemAtPoint(dkey, p, into_groups, upto);
}

SPItem* SPDesktop::getGroupAtPoint(Geom::Point const& p) const {
    if (!_document) return nullptr;
    return _document->getGroupAtPoint(dkey, p);
}

std::vector<SPItem*> SPDesktop::getItemsAtPoints(std::vector<Geom::Point> points, bool all_layers, bool topmost_only, size_t limit, bool active_only) const {
    if (!_document) return {};
    return _document->getItemsAtPoints(dkey, points, all_layers, topmost_only, limit, active_only);
}

void SPDesktop::set_coordinate_status(const Geom::Point& p) {
    // TODO: Implement coordinate status display for Qt
    (void)p;
}

// Desktop style methods
/**
  * Apply the desktop's current style or the tool style to the object.
  */
void SPDesktop::applyCurrentOrToolStyle(SPObject *obj, Glib::ustring const &tool_path, bool with_text, const Glib::ustring &use_current) const
{
    applyCurrentOrToolStyle(obj->getRepr(), tool_path, with_text, use_current);
}
void SPDesktop::applyCurrentOrToolStyle(Inkscape::XML::Node *repr, Glib::ustring const &tool_path, bool with_text, const Glib::ustring &use_current) const
{
    if (SPCSSAttr *css = getCurrentOrToolStyle(tool_path, with_text, use_current)) {
        sp_repr_css_set(repr, css, "style");
        sp_repr_css_attr_unref(css);
    }
}

SPCSSAttr *
SPDesktop::getCurrentOrToolStyle(Glib::ustring const &tool_path, bool with_text, const Glib::ustring &use_current_arg) const
{
    // use_current = "": Read tool_path/usecurrent preference to decide which style to fetch.
    // Or, force one of the options with a non-empty string (used by 3dbox to specify faces):
    // "0": Use tools/tool_path/style (Tool's own style)
    // "1": Use desktop/style (Last used style)
    // "itemtype": Use desktop/itemtype/style (Last used style of same object type)
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    SPCSSAttr *css = sp_repr_css_attr_new();
    Glib::ustring use_current_pref;
    const Glib::ustring *use_current = &use_current_arg;
    if (use_current_arg.empty()) {
        use_current_pref = prefs->getString(tool_path + "/usecurrent");
        use_current = &use_current_pref;
    }

    // Start with per-tool style, then apply current style on top if required
    if (SPCSSAttr *css_tool = prefs->getInheritedStyle(tool_path + "/style")) {
        sp_repr_css_merge(css, css_tool);
        sp_repr_css_attr_unref(css_tool);
    }
    if (!use_current->empty() && *use_current != "0") { // use_current should never be empty, but treat empty as "0"
        if (*use_current == "1") {
            sp_repr_css_merge(css, _current);
        }
        else {
            // Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            // auto *css_new = prefs->getStyle(Glib::ustring("/desktop/") + *use_current + "/style"); // getStyle never returns nullptr
            // sp_repr_css_merge(css, css_new);
            // sp_repr_css_attr_unref(css_new);
        }
    }
    if (css->attributeList().empty()) {
        sp_repr_css_attr_unref(css);
        return nullptr;
    }

    // Remove unwanted attributes
    sp_css_attr_unset_blacklist(css);
    sp_css_attr_unset_uris(css);
    if (!with_text) {
        sp_css_attr_unset_text(css);
    }

    return css; // Caller is responsible for sp_repr_css_attr_unref(css)
}

Glib::ustring
SPDesktop::getCurrentOrToolStylePath(Glib::ustring const &tool_path)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (auto use_current = prefs->getString(tool_path + "/usecurrent"); !use_current.empty() && use_current != "0") {
        if (use_current == "1") {
            return "/desktop/style";
        } else {
            return tool_path + "/style";
            // return Glib::ustring("/desktop/") + use_current + "/style";
        }
    } else {
        return tool_path + "/style";
    }
}

// dialogs TODO
void SPDesktop::showInfoDialog(Glib::ustring const &message) {}

bool SPDesktop::warnDialog(Glib::ustring const &text) { return false; }

void SPDesktop::showNotice(Glib::ustring const &msg, int timeout) {
    if (_widget) {
    }

    //  (void)msg; (void)timeout;
}

// window
Geom::IntPoint SPDesktop::getWindowSize() const {
    return _widget->getWindowSize();
}

void SPDesktop::setWindowSize(Geom::IntPoint const &size) {
    // _widget->setWindowSize(size);
}

void SPDesktop::setWindowTransient(QWidget& window, int transient_policy) {
    // _widget->setWindowTransient(window, transient_policy);
}

LineaWindow* SPDesktop::getLineaWindow() const {
    return _widget ? _widget->get_window() : nullptr;
}

LineaWindow* SPDesktop::getLineaWindow() {
    return _widget ? _widget->get_window() : nullptr;
}

void SPDesktop::presentWindow() {
    // _widget->presentWindow();
}

// Transform history methods
void SPDesktop::prev_transform() {
    if (transforms_past.empty()) return;

    // Save current to future
    transforms_future.push_front(_current_affine);

    // Restore past
    _current_affine = transforms_past.front();
    transforms_past.pop_front();

    // Apply the transform
    // TODO: Update canvas with the restored transform
}

void SPDesktop::next_transform() {
    if (transforms_future.empty()) return;

    // Save current to past
    transforms_past.push_front(_current_affine);

    // Restore future
    _current_affine = transforms_future.front();
    transforms_future.pop_front();

    // TODO: Update canvas with the restored transform
}

void SPDesktop::clear_transform_history() {
    transforms_past.clear();
    transforms_future.clear();
}

// Zoom methods
void SPDesktop::zoom_drawing() {
    g_return_if_fail (doc() != nullptr);
    SPItem *docitem = doc()->getRoot();
    g_return_if_fail (docitem != nullptr);

    docitem->bbox_valid = FALSE;
    Geom::OptRect d = docitem->desktopVisualBounds();

    /* Note that the second condition here indicates that
    ** there are no items in the drawing.
    */
    if (!d || d->minExtent() < 0.1) {
        return;
    }

    set_display_area(*d, 10);
}

void SPDesktop::zoom_selection() {
    const Geom::OptRect d = _selection->visualBounds();

    if (!d || d->minExtent() < 0.1) {
        return;
    }

    set_display_area(*d, 10);
}

void SPDesktop::schedule_zoom_from_document() {
    QTimer::singleShot(0, _canvas.get(), [this]() {
        if (!_canvas || !_document) return;

        auto dim = _canvas->get_dimensions();
        if (dim.x() <= 0 || dim.y() <= 0 || !_widget) {
            schedule_zoom_from_document(); // canvas not laid out yet, retry
            return;
        }
        sp_namedview_zoom_and_view_from_document(this);
    });
}

// Selection boxes
void SPDesktop::setHideSelectionBoxes(bool hide) {
    if (_hide_selection_boxes != hide) {
        _hide_selection_boxes = hide;
        signal_hide_selection_boxes_changed.emit(hide);
    }
}

bool SPDesktop::getHideSelectionBoxes() const {
    return _hide_selection_boxes;
}

// Guide and document management
void SPDesktop::activate_guides(bool activate) {
    guides_active = activate;
    // TODO: Update guides visibility on canvas
}

void SPDesktop::setDocument(SPDocument* doc) {
    if (_document) {
        _detachDocument();
    }

    _selection->setDocument(doc);
    _document = doc;

    if (_document) {
        _attachDocument();
    }
}

void SPDesktop::change_document(SPDocument* document) {
    g_return_if_fail(document);

    if (document == _document) return;

    // Unselect everything before switching documents.
    _selection->clear();

    // Reset any tool actions currently in progress.
    if (_currentTool) {
        setTool(std::string{_currentTool->get_name()});
    }

    setDocument(document);

    // Notify the window and update the UI for the new document.
    if (_widget) {
        if (auto wnd = getLineaWindow()) {
            wnd->change_document(document);
        }
    }

    sp_namedview_zoom_and_view_from_document(this);
}

// Render/color modes
void SPDesktop::setRenderMode(Inkscape::RenderMode mode) {
    if (_canvas) {
        _canvas->set_render_mode(mode);
    }
}

void SPDesktop::setColorMode(Inkscape::ColorMode mode) {
    if (_canvas) {
        _canvas->set_color_mode(mode);
    }
}

// UI toggles (stubs for Qt)
void SPDesktop::toggleCommandPalette() {
    // TODO: Implement command palette toggle for Qt
}

void SPDesktop::toggleRulers() {
    if (_widget) _widget->toggleRulers();
}

void SPDesktop::toggleScrollbars() {
    // TODO: Implement scrollbar toggle for Qt
}

// Window state queries
bool SPDesktop::isMinimised() const {
    // TODO: Check if window is minimized
    return false;
}

bool SPDesktop::is_darktheme() const {
    // TODO: Check if dark theme is active
    return false;
}

bool SPDesktop::is_maximized() const {
    // TODO: Check if window is maximized
    return false;
}

bool SPDesktop::is_fullscreen() const {
    // TODO: Check if window is fullscreen
    return false;
}

bool SPDesktop::is_focusMode() const {
    return _focusMode;
}

// Focus mode
void SPDesktop::focusMode(bool mode) {
    _focusMode = mode;
    // TODO: Implement focus mode UI changes
}

// Toolbar/guides toggles
void SPDesktop::toggleLockGuides() {
    // TODO: Implement guide lock toggle
}

void SPDesktop::toggleToolbar(const char* toolbar_name) {
    // TODO: Implement toolbar toggle for Qt
    (void)toolbar_name;
}

// Signal connection implementations
sigc::connection SPDesktop::connect_gradient_stop_selected(sigc::slot<void (SPStop *)> const &slot) {
    return signal_gradient_stop_selected.connect(slot);
}

sigc::connection SPDesktop::connect_control_point_selected(sigc::slot<void (Inkscape::UI::ControlPointSelection *)> const &slot) {
    return signal_control_point_selected.connect(slot);
}

sigc::connection SPDesktop::connect_text_cursor_moved(sigc::slot<void (Inkscape::UI::Tools::TextTool *)> const &slot) {
    return signal_text_cursor_moved.connect(slot);
}

// CanvasItemGroup accessor implementations
Inkscape::CanvasItemGroup* SPDesktop::getCanvasGrids() const {
    return _canvas ? _canvas->getCanvasGrids() : nullptr;
}

Inkscape::CanvasItemGroup* SPDesktop::getCanvasPagesBg() const {
    return _canvas ? _canvas->getCanvasPagesBg() : nullptr;
}

Inkscape::CanvasItemGroup* SPDesktop::getCanvasPagesFg() const {
    return _canvas ? _canvas->getCanvasPagesFg() : nullptr;
}

// } // namespace QtUI
// } // namespace Inkscape
