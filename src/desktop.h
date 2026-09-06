// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt6 desktop adapter - minimal replacement for SPDesktop without GTK.
 *
 * This class bridges the Qt6 UI (canvas, main window) with the Inkscape
 * core (document, drawing, tools, selection). It provides the essential
 * functionality of SPDesktop without the GTK widget dependencies.
 */

#ifndef LINEA_DESKTOP_H
#define LINEA_DESKTOP_H

#include <memory>
#include <string>

#include <sigc++/connection.h>
#include <sigc++/signal.h>

#include <2geom/affine.h>
#include <2geom/parallelogram.h>
#include <2geom/point.h>
#include <2geom/rect.h>
#include <2geom/transforms.h>

#include "message-stack.h"
#include "display/rendermode.h"
#include "message-context.h"
#include "object/sp-gradient.h" // TODO refactor enums out to their own .h file
#include "util/element-properties.h"
#include "qt/util/virtual-node-type.h"

class QCursor;
class QWidget;
class LineaWindow;

// Signal accumulators for style signals
struct StopOnTrue {
  using result_type = bool;

  template<typename T_iterator>
  result_type operator()(T_iterator first, T_iterator last) const {
      for (; first != last; ++first)
          if (*first) return true;
      return false;
  }
};

struct StopOnNonZero {
  using result_type = int;

  template<typename T_iterator>
  result_type operator()(T_iterator first, T_iterator last) const {
      for (; first != last; ++first)
          if (*first) return *first;
      return 0;
  }
};

// Forward declarations (global namespace)
class SPDocument;
class SPItem;
class SPRoot;
class SPNamedView;
class SPStop;
class SPCSSAttr;
class SPStyle;

namespace Linea::UI { class SPDesktopWidget; }

namespace Inkscape {
class CanvasItem;
class CanvasItemCatchall;
class CanvasItemDrawing;
class CanvasItemGroup;
class CanvasEvent;
class DrawingItem;
class LayerManager;
class MessageStack;
class Selection;

namespace XML {
class Node;
}

namespace UI {
class ControlPointSelection;
namespace Widget {
class Canvas;
}
namespace Tools {
class ToolBase;
class TextTool;
}
}

namespace Display {
class SnapIndicator;
class TemporaryItem;
class TemporaryItemList;
class TranslucencyGroup;
}

namespace UI {
namespace Tools {
class ToolBase;
class SelectTool;
class NodeTool;
}
}

namespace QtUI {
class QtEventConverter;
}

} // Inkscape

namespace Linea {
struct PresentationState;
struct PresentationStateDelta;
struct ElementState;

namespace Props {
class SelectionStateModel;
}
}

inline constexpr double SP_DESKTOP_ZOOM_MAX = 256.00;
inline constexpr double SP_DESKTOP_ZOOM_MIN =   0.01;

/**
 * Desktop adapter for Qt6-based Inkscape
 *
 * Responsibilities:
 * - Own SPDocument and set up Drawing/CanvasItem tree
 * - Manage tools (Select, Node)
 * - Handle CanvasEvent dispatch to current tool
 * - Provide coordinate transforms
 * - Manage Selection
 * - Interface between Qt canvas and Inkscape core
 */
class SPDesktop {
public:
    SPDesktop(SPNamedView* namedview);
    ~SPDesktop(); // defined in .cpp where TemporaryItemList is complete

    // Document access
    SPDocument* doc() const { return _document; }
    SPDocument* document() const { return _document; }
    SPDocument* getDocument() const { return _document; }
    SPRoot* root() const;

    // Item lookup (delegates to SPDocument)
    SPItem* getItemFromListAtPointBottom(std::vector<SPItem*> const &list, Geom::Point const &p) const;

    // Viewport and visibility
    Geom::Parallelogram get_display_area() const;
    bool isWithinViewport(SPItem const *item) const;
    bool itemIsHidden(SPItem const *item) const;

    void set_rotation_lock(bool lock) { rotation_locked = lock; }
    bool get_rotation_lock() const { return rotation_locked; }

    void zoom_grab_focus();
    void rotate_grab_focus();

    void rotate(double angle);
    void rotate_absolute_keep_point  (Geom::Point const &c, double rotate);
    void rotate_relative_keep_point  (Geom::Point const &c, double rotate);
    void rotate_absolute_center_point(Geom::Point const &c, double rotate);
    void rotate_relative_center_point(Geom::Point const &c, double rotate);

    enum CanvasFlip {
        FLIP_NONE       = 0,
        FLIP_HORIZONTAL = 1,
        FLIP_VERTICAL   = 2
    };
    void flip_absolute_keep_point  (Geom::Point const &c, CanvasFlip flip);
    void flip_relative_keep_point  (Geom::Point const &c, CanvasFlip flip);
    void flip_absolute_center_point(Geom::Point const &c, CanvasFlip flip);
    void flip_relative_center_point(Geom::Point const &c, CanvasFlip flip);
    bool is_flipped(CanvasFlip flip);

    Geom::Rotate const &current_rotation() const { return _current_affine.getRotation(); }

    void scroll_absolute(Geom::Point const &point);
    void scroll_relative(Geom::Point const &delta);
    void scroll_relative_in_svg_coords(double dx, double dy);
    bool scroll_to_point(Geom::Point const &s_dt, double autoscrollspeed = 0);

    // Canvas access
    Inkscape::UI::Widget::Canvas* canvas() const { return _canvas.get(); }
    Inkscape::UI::Widget::Canvas* getCanvas() const { return _canvas.get(); }

    // SPDesktop compatibility stubs (for ToolBase migration)
    // These return nullptr in Qt mode since there's no GTK Canvas/DesktopWidget
    Inkscape::UI::Widget::Canvas* get_gtk_canvas() const { return nullptr; }

    // CanvasItemGroup accessors (delegate to QtCanvasWidget)
    Inkscape::CanvasItemGroup* getCanvasControls() const;
    Inkscape::CanvasItemGroup* getCanvasTemp() const;
    Inkscape::CanvasItemGroup* getCanvasGuides() const;
    Inkscape::CanvasItemCatchall* getCanvasCatchall() const;
    Inkscape::CanvasItemGroup* getCanvasSketch() const;
    Inkscape::CanvasItemGroup* getCanvasGrids() const;
    Inkscape::CanvasItemGroup* getCanvasPagesBg() const;
    Inkscape::CanvasItemGroup* getCanvasPagesFg() const;
    Inkscape::CanvasItemDrawing* getCanvasDrawing() const { return _canvasDrawing; }

    // Additional desktop methods for ToolBase compatibility
    Geom::Point current_center() const;
    bool quick_zoomed() const;
    void setToolboxFocusTo(const std::string& id);
    void setToolboxAdjustmentValue(const std::string& id, double value);
    // void applyCurrentOrToolStyle(Inkscape::XML::Node* repr, const std::string& tool_path, bool is_shape_tool);
    void quick_preview(bool active);
    void zoom_quick(bool active);
    void emit_text_cursor_moved(Inkscape::UI::Tools::TextTool* tool);
    void setToolboxAdjustmentValueInt(const std::string& id, int value);

    // temp desktop style, to be replaced by getter; used by desktop-style.cpp
    SPCSSAttr* _current = nullptr;  ///< Current style
    // current style querying
    sigc::signal<bool (SPCSSAttr const *, bool)>::accumulated<StopOnTrue> _set_style_signal;
    sigc::signal<int (SPStyle *, int)>::accumulated<StopOnNonZero> _query_style_signal;
    template <typename F> sigc::connection connectSetStyle(F &&slot) {
        return _set_style_signal.connect(std::forward<F>(slot));
    }
    template <typename F> sigc::connection connectQueryStyle(F &&slot) {
        return _query_style_signal.connect(std::forward<F>(slot));
    }
    void applyCurrentOrToolStyle(SPObject *obj, Glib::ustring const &tool_path, bool with_text, const Glib::ustring &use_current = "") const;
    void applyCurrentOrToolStyle(Inkscape::XML::Node *repr, Glib::ustring const &tool_path, bool with_text, const Glib::ustring &use_current = "") const;
    SPCSSAttr *getCurrentOrToolStyle(Glib::ustring const &tool_path, bool with_text, const Glib::ustring &use_current = "") const;
    Glib::ustring getCurrentOrToolStylePath(Glib::ustring const &tool_path);


    // Drawing/CanvasItem access
    CanvasItemDrawing* canvasDrawing() const { return _canvasDrawing; }

    // Snap indicator
    Inkscape::Display::SnapIndicator* getSnapIndicator() const { return _snapindicator.get(); }

    // Message context
    Inkscape::MessageStack* messageStack() const { return _message_stack.get(); }
    Inkscape::MessageContext* tipsMessageContext() const { return _tips_message_context.get(); }
    Inkscape::MessageContext* guidesMessageContext() const { return _guides_message_context.get(); }

    // Signal emission methods
    void emit_control_point_selected(Inkscape::UI::ControlPointSelection* selection);
    void emit_gradient_stop_selected(SPStop* stop);

    // Signal connection methods
    template <typename F> sigc::connection connectGradientStopSelected(F &&slot) {
        return signal_gradient_stop_selected.connect(std::forward<F>(slot));
    }

    template <typename F> sigc::connection connectHideSelectionBoxes(F &&slot) {
        return signal_hide_selection_boxes_changed.connect(std::forward<F>(slot));
    }

    // Connection methods for various signals (matching gtk-desktop.h interface)
    sigc::connection connect_gradient_stop_selected(sigc::slot<void (SPStop *)> const &slot);
    sigc::connection connect_control_point_selected(sigc::slot<void (Inkscape::UI::ControlPointSelection *)> const &slot);
    sigc::connection connect_text_cursor_moved(sigc::slot<void (Inkscape::UI::Tools::TextTool *)> const &slot);

    // Temporary item management (stubs)
    Inkscape::Display::TemporaryItem* add_temporary_canvasitem(Inkscape::CanvasItem* item, int lifetime_msecs, bool move_to_bottom = true);
    void remove_temporary_canvasitem(Inkscape::Display::TemporaryItem* tempitem);

    // Layer management
    Inkscape::LayerManager& layerManager() const;

    // Point query
    Geom::Point point() const;

    // Display key for rendering (stub)
    unsigned dkey = 1;

    // Tool management
    void setTool(const std::string& toolName);
    Inkscape::UI::Tools::ToolBase* currentTool() const { return _currentTool.get(); }
    // Tool access (stub)
    Inkscape::UI::Tools::ToolBase* getTool() const { return currentTool(); }
    const std::string& getActiveTool() const { return _currentToolName; }

    bool isSelectTool() const;
    bool isNodeTool() const;

    // Storage for selected dragger used by GrDrag as it's created and deleted by tools
    SPItem* gr_item = nullptr;
    GrPointType gr_point_type = POINT_LG_BEGIN;
    unsigned int gr_point_i = 0;
    Inkscape::PaintTarget gr_fill_or_stroke = Inkscape::FOR_FILL;

    // Public state variables (matching gtk-desktop.h)
    unsigned interaction_disabled_counter = 0;
    bool waiting_cursor = false;  // Alias for _waiting_cursor
    bool showing_dialogs = false;
    bool guides_active = false;
    bool _focusMode = false;  ///< Whether we're focused working or general working

    // Event handling (called from QtCanvasWidget)
    bool handleCanvasEvent(CanvasEvent* event);

    // Selection
    Selection* getSelection() const { return _selection.get(); }

    // Virtual node selection in the object tree (DocumentProps, Pages, Guides,
    // etc.). The desktop owns this just like it owns the normal element
    // selection, so it survives tab switches.
    Linea::UI::VirtualNodeType selectedVirtualNode() const { return _selectedVirtualNode; }
    void setSelectedVirtualNode(Linea::UI::VirtualNodeType type) { _selectedVirtualNode = type; }

    // Typed selection snapshot (see props/selection-state-model.h)
    Linea::Props::SelectionStateModel* stateModel() const { return _stateModel.get(); }

    /// Transformation from window to desktop coordinates (zoom/rotate).
    Geom::Affine const &w2d() const { return _current_affine.w2d(); }
    Geom::Point w2d(Geom::Point const &p) const { return p * _current_affine.w2d(); }
    /// Transformation from desktop to window coordinates
    Geom::Affine const &d2w() const { return _current_affine.d2w(); }
    Geom::Point d2w(Geom::Point const &p) const { return p * _current_affine.d2w(); }
    Geom::Affine const &doc2dt() const;
    Geom::Affine const &dt2doc() const;
    Geom::Point doc2dt(Geom::Point const &p) const { return p * doc2dt(); }
    Geom::Point dt2doc(Geom::Point const &p) const { return p * dt2doc(); }

    // Y-axis direction
    double yaxisdir() const { return doc2dt()[3]; }  // Y scaling factor from doc2dt transform
    bool yaxisdown() const { return yaxisdir() > 0; }

    // NamedView access
    SPNamedView* namedView() const;
    SPNamedView* getNamedView() const { return namedView(); }

    // Zoom and view control (delegated to canvas)
    double current_zoom() const { return _current_affine.getZoom(); }

    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoom_relative(Geom::Point const &c, double zoom, bool keep_point = true);
    void zoom_absolute(Geom::Point const &center, double zoom, bool keep_point = true);
    void zoom_realworld(Geom::Point const &c, double ratio);

    // Pinch zoom
    void on_zoom_begin();
    void on_zoom_scale(double scale);
    void on_zoom_end();

    // Display area control
    void set_display_area(Geom::Rect const &r, Geom::Coord border, bool log = true);
    void set_display_area(Geom::Point const &c, Geom::Point const &w, bool log = true);
    void set_display_area(bool log);
    void set_display_width(Geom::Rect const &rect, Geom::Coord border);
    void set_display_center(Geom::Rect const &a);

    // Transform history
    void prev_transform();
    void next_transform();
    void clear_transform_history();

    // Zoom methods
    void zoom_drawing();
    void zoom_selection();
    void schedule_zoom_from_document();

    // Selection boxes
    void setHideSelectionBoxes(bool hide);
    bool getHideSelectionBoxes() const;

    // Guide and document management
    void activate_guides(bool activate);
    void change_document(SPDocument* document);
    void setDocument(SPDocument* doc);

    // Render/color modes
    void setRenderMode(Inkscape::RenderMode mode);
    void setColorMode(Inkscape::ColorMode mode);

    // UI toggles (stubs for Qt)
    void toggleCommandPalette();
    void toggleRulers();
    void toggleScrollbars();

    // Window state queries
    bool isMinimised() const;
    bool is_darktheme() const;
    bool is_maximized() const;
    bool is_fullscreen() const;
    bool is_focusMode() const;

    // Focus mode
    void focusMode(bool mode = true);

    // Toolbar/guides toggles
    void toggleLockGuides();
    void toggleToolbar(const char* toolbar_name);
    Glib::ustring get_toolbar_by_name(Glib::ustring const &name);

    // Item picking (find item at point)
    SPItem* itemAtPoint(const Geom::Point& point, bool intoGroups = false) const;
    SPItem* getItemAtPoint(Geom::Point const& p, bool into_groups, SPItem* upto = nullptr) const;
    SPItem* getGroupAtPoint(Geom::Point const& p) const;
    std::vector<SPItem*> getItemsAtPoints(std::vector<Geom::Point> points, bool all_layers = true, bool topmost_only = true, size_t limit = 0, bool active_only = true) const;
    DrawingItem* drawingItemAtPoint(const Geom::Point& point) const;

    // Canvas event handling for tools (matches SPDesktop::drawing_handler)
    bool drawingHandler(CanvasEvent const& event, DrawingItem* drawingItem);

    // Cursor management
    void setCursor(const QCursor& cursor);
    void setDefaultCursor();
    void setWaitingCursor();
    void clearWaitingCursor();
    bool isWaitingCursor() const { return _waiting_cursor; }

    // Viewport/geometry

    void updatePageInfo();

    void set_coordinate_status(const Geom::Point& p);

    // warning/info UI
    void showInfoDialog(Glib::ustring const &message);
    bool warnDialog(Glib::ustring const &text);
    void showNotice(Glib::ustring const &msg, int timeout = 0);
    // main window
    void presentWindow();
    Geom::IntPoint getWindowSize() const;
    void setWindowSize(Geom::IntPoint const &size);
    void setWindowTransient(QWidget& window, int transient_policy = 1);
    LineaWindow* getLineaWindow() const;
    LineaWindow* getLineaWindow();
    Linea::UI::SPDesktopWidget* getDesktopWidget() const { return _widget; }
    void setDesktopWidget(Linea::UI::SPDesktopWidget* widget) { _widget = widget; }

    struct StyleChangeArgs {
        Selection* selection;
        const Linea::PresentationState& presentation;
        const Linea::ElementState& elements;
        const Linea::PresentationStateDelta& delta;
        bool modified;
        unsigned flags;
    };

private:
    bool rotation_locked = false;

    void _attachDocument();
    void _detachDocument();
    void setupCanvasDrawing();

    SPNamedView* _namedview = nullptr;
    int _view_number = 0;
    SPDocument* _document = nullptr;
    std::unique_ptr<Inkscape::UI::Widget::Canvas> _canvas;

    // The CanvasItemDrawing that wraps the Drawing render tree
    CanvasItemDrawing* _canvasDrawing = nullptr;

    // Canvas item catchall (for unclaimed events)
    Inkscape::CanvasItemCatchall* _canvas_catchall = nullptr;

    // Tools
    std::unique_ptr<Inkscape::UI::Tools::ToolBase> _currentTool;
    std::string _currentToolName;

    // Selection
    std::unique_ptr<Selection> _selection;
    std::unique_ptr<Linea::Props::SelectionStateModel> _stateModel;
    sigc::scoped_connection _selection_changed_connection;
    sigc::scoped_connection _selection_modified_connection;

    // Virtual node selection in the object tree (None when a normal element
    // selection is active).
    Linea::UI::VirtualNodeType _selectedVirtualNode = Linea::UI::VirtualNodeType::None;

    // This simple class ensures that _w2d is always in sync with _rotation and _scale
    // We keep rotation and scale separate to avoid having to extract them from the affine.
    // With offset, this describes fully how to map the drawing to the window.
    // Future: merge offset as a translation in w2d.
    class DesktopAffine {
    public:
        Geom::Affine const &w2d() const { return _w2d; };
        Geom::Affine const &d2w() const { return _d2w; };

        void setScale(Geom::Scale scale) {
            _scale = scale;
            _update();
        }
        void addScale(Geom::Scale scale) {
            _scale *= scale;
            _update();
        }

        void setRotate(Geom::Rotate rotate) {
            _rotate = rotate;
            _update();
        }
        void setRotate(double rotate) {
            setRotate(Geom::Rotate{rotate});
        }
        void addRotate(Geom::Rotate rotate) {
            _rotate *= rotate;
            _update();
        }
        void addRotate(double rotate) {
            addRotate(Geom::Rotate{rotate});
        }

        void setFlip(CanvasFlip flip) {
            _flip = Geom::Scale();
            addFlip( flip );
        }

        bool isFlipped(CanvasFlip flip) {
            if ((flip & FLIP_HORIZONTAL) && Geom::are_near(_flip[0], -1)) {
                return true;
            }
            if ((flip & FLIP_VERTICAL) && Geom::are_near(_flip[1], -1)) {
                return true;
            }
            return false;
        }

        void addFlip(CanvasFlip flip) {
            if (flip & FLIP_HORIZONTAL) {
                _flip *= Geom::Scale(-1.0, 1.0);
            }
            if (flip & FLIP_VERTICAL) {
                _flip *= Geom::Scale(1.0, -1.0);
            }
            _update();
        }
        double getZoom() const {
            return _d2w.descrim();
        }
        Geom::Rotate const &getRotation() const {
            return _rotate;
        }
        void setOffset(Geom::Point offset) {
            _offset = offset;
        }
        void addOffset(Geom::Point offset) {
            _offset += offset;
        }
        Geom::Point const &getOffset() {
            return _offset;
        }

    private:
        void _update() {
            _d2w = _scale * _rotate * _flip;
            _w2d = _d2w.inverse();
        }
        Geom::Affine _w2d;      // Window to desktop
        Geom::Affine _d2w;      // Desktop to window
        Geom::Rotate _rotate;   // Rotate part of _w2d
        Geom::Scale  _scale;    // Scale part of _w2d, holds y-axis direction
        Geom::Scale  _flip;     // Flip part of _w2d
        Geom::Point  _offset;   // Point on canvas to align to (0,0) of window
    };

    DesktopAffine _current_affine;
    std::list<DesktopAffine> transforms_past;
    std::list<DesktopAffine> transforms_future;
    bool _quick_zoom_enabled = false; ///< Signifies that currently we're in quick zoom mode
    DesktopAffine _quick_zoom_affine; ///< The transform of the screen before quick zoom

    // Message stack (for status messages)
    std::unique_ptr<Inkscape::MessageStack> _message_stack;
    std::unique_ptr<Inkscape::MessageContext> _tips_message_context;
    std::unique_ptr<Inkscape::MessageContext> _guides_message_context;

    // Layer manager (lazy init)
    mutable std::unique_ptr<Inkscape::LayerManager> _layerManager;

    std::unique_ptr<Inkscape::Display::TranslucencyGroup> _translucency_group;
    std::unique_ptr<Inkscape::Display::SnapIndicator> _snapindicator;
    std::unique_ptr<Inkscape::Display::TemporaryItemList> _temporary_item_list;

    // Cursor state
    bool _waiting_cursor = false;

    // Rotation and zoom state
    // double _current_zoom = 1.0;
    double _current_rotation = 0.0; // in radians
    bool _rotation_lock = false;

    // Pinch zoom state
    std::optional<double> _begin_zoom;

    // Document connections (for updates)
    // Using sigc++ connections
    sigc::connection _reconstructionStartConn;
    sigc::connection _reconstructionFinishConn;

    // Signals (for compatibility with SPDesktop interface)
    sigc::signal<void (SPDesktop*)> _destroy_signal;
    sigc::signal<void (SPDesktop*, SPDocument*)> _document_replaced_signal;
    sigc::signal<void (SPDesktop*, Inkscape::UI::Tools::ToolBase*)> _event_context_changed_signal;

    // Style signals
    sigc::signal<void (SPStop *)> signal_gradient_stop_selected;
    sigc::signal<void (bool)> signal_hide_selection_boxes_changed;
    sigc::signal<void (Inkscape::UI::Tools::TextTool *)> signal_text_cursor_moved;

    // Selection boxes visibility
    bool _hide_selection_boxes = false;

    Linea::UI::SPDesktopWidget* _widget = nullptr;

    // An id attribute is not allowed to be the empty string.
    Glib::ustring _reconstruction_old_layer_id;

    sigc::scoped_connection _y_axis_flipped;

    void reconstruction_start();
    void reconstruction_finish();
    void handle_y_axis_flip(double yshift);

    sigc::signal<void (StyleChangeArgs&)> _signal_style_changed;
    sigc::signal<void (SPCSSAttr*)> _signal_desktop_style_changed;

    void fireStyleChanged(unsigned flags);

public:
    sigc::signal<void (double)> signal_zoom_changed;
    sigc::signal<void (Inkscape::UI::ControlPointSelection *)> signal_control_point_selected;

    template <typename F> sigc::connection connectDestroy(F &&slot) {
        return _destroy_signal.connect(std::forward<F>(slot));
    }

    template <typename F> sigc::connection connectDocumentReplaced(F &&slot) {
        return _document_replaced_signal.connect(std::forward<F>(slot));
    }

    template <typename F> sigc::connection connectEventContextChanged(F &&slot) {
        return _event_context_changed_signal.connect(std::forward<F>(slot));
    }

    template <typename F> sigc::connection connectZoomChanged(F &&slot) {
        return signal_zoom_changed.connect(std::forward<F>(slot));
    }

    // style of selected object(s) has changed (b/c selection has changed or has been modified)
    template <typename F> sigc::connection connectSelectionStyleChanged(F&& slot) {
        return _signal_style_changed.connect(std::forward<F>(slot));
    }

    // desktop style has changed
    template <typename F> sigc::connection connectDesktopStyleChanged(F&& slot) {
        return _signal_desktop_style_changed.connect(std::forward<F>(slot));
    }
    void fireDesktopStyleChanged();
};

#endif // LINEA_DESKTOP_H
