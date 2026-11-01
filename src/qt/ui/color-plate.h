// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPlate — color plate widget (rectangular, wheel or ring color picker).
 */

#ifndef LINEA_UI_COLOR_PLATE_H
#define LINEA_UI_COLOR_PLATE_H

#include <QPointF>
#include <QPointer>
#include <QWidget>
#include <optional>
#include <vector>
#include <sigc++/signal.h>

#include "colors/color.h"
#include "colors/spaces/enum.h"
#include "colors/spaces/gamut.h"

namespace Linea::UI {

class IndicatorOverlay;

// Shapes of color plate widget; persisted in preferences, keep values stable
enum class ColorPlateType { None, Rect, Wheel, Ring };

// Gamut inputs for plates drawn in unbounded spaces (OkLch rect plate)
struct PlateGamut {
    Inkscape::Colors::Gamut gamut = Inkscape::Colors::Gamut::sRGB; // colors shown only within this gamut
    bool srgb_line = false; // outline sRGB when a wider gamut is drawn
    bool p3_line = false;   // outline Display P3 when Rec2020 is drawn
    // space of the window surface the plate's pixels are read in:
    // DisplayP3 when the surface is tagged Display P3
    Inkscape::Colors::Gamut surface = Inkscape::Colors::Gamut::sRGB;
};

/**
 * A color plate widget: rectangular plane, hue wheel or hue ring with a
 * tone triangle. The user can click/drag to pick a color.
 */
class ColorPlate : public QWidget {
    Q_OBJECT
public:
    ~ColorPlate() override;

    // null if 'source' has no plate or shape is None
    static ColorPlate* create(Inkscape::Colors::Space::Type source, ColorPlateType shape,
                              const PlateGamut& gamut, QWidget* parent = nullptr);
    // true if 'source' has a plate space
    static bool supports(Inkscape::Colors::Space::Type source);

    // set current color, in any space; ignored while the user is dragging
    void setColor(const Inkscape::Colors::Color& color);
    // connect a callback invoked when the user picks a color; emits colors in the source space
    sigc::connection connectColorChanged(sigc::slot<void(const Inkscape::Colors::Color&)> cb);

    QSize sizeHint() const override { return {215, 215}; }

protected:
    ColorPlate(Inkscape::Colors::Space::Type source, Inkscape::Colors::Space::Type space,
               int fixed_channel, Inkscape::Colors::Gamut surface, QWidget* parent);

    // subclass hooks, all in plate-space colors
    virtual void colorChanged() = 0;                         // _color changed: invalidate images, recompute cached local marker positions
    virtual void paintPlate(QPainter& p) = 0;              // images, clip, borders
    virtual std::vector<QPointF> markers() const = 0;      // marker centers in widget coords, from cached local positions
    virtual void beginPick(const QPointF& pos) {}          // drag start (ring: band vs triangle)
    virtual Inkscape::Colors::Color colorAt(const QPointF& pos) const = 0; // plate color at a widget position, clamped onto the pickable area

    const Inkscape::Colors::Color& color() const { return _color; }
    Inkscape::Colors::Space::Type plateSpace() const { return _space; }
    Inkscape::Colors::Gamut surface() const { return _surface; }
    // convert a built image's bytes into the surface color space when they
    // differ — call after building/tagging an image, never per paint
    void toSurface(QImage& img) const;

    QRectF area() const;
    QRectF activeArea() const;
    // |a-b|, wrapping around at 1 for the space's hue channel — shared by
    // subclasses' image rebuild thresholds
    static double channelDelta(Inkscape::Colors::Space::Type space, int channel, double a, double b);

    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
    void hideEvent(QHideEvent*) override;
    void moveEvent(QMoveEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    // pick the plate color nearest a widget position and emit
    void pickAt(QPointF pos);
    // convert _color to the source space and emit it
    void emitColor();
    // show/move/hide the indicator overlays to track markers()
    void syncOverlays();
    // the plate space's hue channel; unset when the space has none
    std::optional<unsigned> hueChannel() const;
    // Hue is meaningless at zero saturation and at black.
    bool achromatic(const Inkscape::Colors::Color& plate) const;
    // true when type's hue convention matches the plate space's
    bool sharesHue(Inkscape::Colors::Space::Type type) const;

    Inkscape::Colors::Space::Type _source; // space colors are emitted in
    Inkscape::Colors::Space::Type _space;  // space the plate is drawn/picked in
    int _fixed_channel;                    // channel the plate holds constant (driven by its slider)
    Inkscape::Colors::Gamut _surface;      // space the window surface reads the plate's pixels in
    Inkscape::Colors::Color _color;        // current color in plate space, opacity 1
    double _last_hue = -1.0;               // last hue rendered on the plate; -1 before the first setColor
    // true while the user is dragging; the widget suppresses incoming
    // setColor() updates then, the same way edit controls block signal
    // feedback while the user is typing
    bool _drag = false;
    // children of the top-level window, not of the plate: an indicator may
    // extend past the plate's edges, which a regular child cannot draw into
    std::vector<QPointer<IndicatorOverlay>> _overlays;

    sigc::signal<void(const Inkscape::Colors::Color&)> _signal_color_changed;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PLATE_H
