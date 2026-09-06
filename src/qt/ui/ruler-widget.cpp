// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * RulerWidget implementation — Qt port of ink-ruler.cpp.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ruler-widget.h"

#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

#include <algorithm>
#include <vector>

#include "preferences.h"
#include "theme.h"
#include "ui/pixel-alignment.h"
#include "util/units.h"

namespace Linea::UI {

// Half width of pointer triangle.
constexpr double half_width = 4.0;
constexpr int ruler_size = 20; // just a default

RulerWidget::RulerWidget(Qt::Orientation orientation, QWidget* parent)
    : QWidget(parent)
    , _orientation(orientation) {
    setObjectName("RulerWidget");
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    if (_orientation == Qt::Horizontal) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(_ruler_size);
        // Dragging from the horizontal ruler pulls out a vertical guide.
        setCursor(Qt::SizeVerCursor);
    } else {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setFixedWidth(_ruler_size);
        // Dragging from the vertical ruler pulls out a horizontal guide.
        setCursor(Qt::SizeHorCursor);
    }

    auto font = this->font();
    font.setPointSize(10);
    font.setWeight(QFont::DemiBold);
    setFont(font);

    createContextMenu();
}

RulerWidget::~RulerWidget() = default;

void RulerWidget::setUnit(const Inkscape::Util::Unit* unit) {
    if (!unit) return;

    if (_unit != unit) {
        _unit = unit;
        update();
    }

    const auto abbr = QString::fromStdString(_unit->abbr);
    for (auto action : _unit_action_group->actions()) {
        action->setChecked(action->data().toString() == abbr);
    }
}

void RulerWidget::setRange(double lower, double upper) {
    if (_lower != lower || _upper != upper) {
        _lower = lower;
        _upper = upper;
        _max_size = _upper - _lower;
        if (_max_size == 0) {
            _max_size = 1;
        }
        update();
    }
}

void RulerWidget::setPage(double lower, double upper) {
    if (_page_lower != lower || _page_upper != upper) {
        _page_lower = lower;
        _page_upper = upper;
        update();
    }
}

void RulerWidget::setSelection(double lower, double upper) {
    if (_sel_lower != lower || _sel_upper != upper) {
        _sel_lower = lower;
        _sel_upper = upper;
        update();
    }
}

void RulerWidget::setCursorPosition(double position) {
    if (position == _position) return;

    _position = position;
    update();
}

void RulerWidget::setRulerSize(int size) {
    if (_ruler_size == size) return;

    _ruler_size = size;
    if (_orientation == Qt::Horizontal) {
        setFixedHeight(size);
    } else {
        setFixedWidth(size);
    }
    update();
}

void RulerWidget::setMarkerVisible(bool visible) {
    if (_marker_visible == visible) return;

    _marker_visible = visible;
    update();
}

QSize RulerWidget::sizeHint() const {
    if (_orientation == Qt::Horizontal) {
        return {100, ruler_size};
    }
    return {ruler_size, 100};
}

static double safe_frac(double x) {
    return x - std::floor(x);
}

void RulerWidget::drawRuler(QPainter& painter) {
    painter.setRenderHint(QPainter::Antialiasing, false);

    const auto dark = isDarkTheme();
    const auto horiz = _orientation == Qt::Horizontal;
    const auto w = width();
    const auto h = height();
    // const auto w = horiz ? width() : width() - 1;
    // const auto h = horiz ? height() - 1 : height();
    const int pixel_scale = static_cast<int>(devicePixelRatioF());

    // aparallel is the longer dimension of the ruler; aperp shorter.
    const auto [aparallel, aperp] = _orientation == Qt::Horizontal ? std::pair{w, h} : std::pair{h, w};

    // Most colors used by drawRuler
    auto dark_color  = palette().color(QPalette::Dark);
    dark_color.setAlphaF(dark_color.alphaF() * (dark ? 0.7 : 0.5));
    auto text_color  = dark_color;
    auto tick_color  = dark_color;
    auto minor_color = tick_color;
    auto page_fill   = palette().color(QPalette::Base);
    if (dark) { page_fill = page_fill.darker(130); }
    auto selection_highlight = palette().color(QPalette::Highlight);
    auto sel_bgnd    = selection_highlight;
    sel_bgnd.setAlphaF(sel_bgnd.alphaF() * 0.4);
    auto sel_stroke  = selection_highlight;
    auto sel_text = palette().color(QPalette::Accent);

    // Helper to convert (parallel, perpendicular) coords to widget rect.
    auto to_rect = [&](double par0, double perp0, double par1, double perp1) {
        if (horiz) {
            return QRectF(par0, perp0, par1 - par0, perp1 - perp0);
        }
        return QRectF(perp0, par0, perp1 - perp0, par1 - par0);
    };

    // Color in page indication box
    auto aligned_page = Inkscape::pixel_align(Geom::Interval(_page_lower, _page_upper),
                                              Inkscape::RectLineAlignment::CenterInside, 0, pixel_scale);
    if (const auto interval = aligned_page & Geom::Interval(0, aparallel)) {
        painter.fillRect(to_rect(interval.value().min(), 0, interval.value().max(), aperp), page_fill);
    }

    // Draw a selection bar
    if (_sel_lower != _sel_upper) {
        constexpr double line_width = 2.0;
        const auto delta = _sel_upper - _sel_lower;
        if (std::abs(delta) >= 1) {
            auto sel_range = Inkscape::pixel_align(Geom::Interval(_sel_lower, _sel_upper),
                                                   Inkscape::RectLineAlignment::CenterOutside, 0, pixel_scale);
            double sy0 = sel_range.min();
            double sy1 = sel_range.max();
            const double x = aperp - line_width;
            painter.fillRect(to_rect(sy0, 0, sy1, x), sel_bgnd);
            painter.fillRect(to_rect(sy0, x, sy1, aperp), sel_stroke);
        }
    }

    const double abs_size = std::abs(_max_size);
    const int sign = _max_size >= 0 ? 1 : -1;

    // Figure out scale. Largest ticks must be far enough apart to fit largest text.
    int scale = std::ceil(abs_size);
    const auto scale_text = std::to_string(scale);
    const int digits = static_cast<int>(scale_text.length()) + 1;
    const int font_size = fontMetrics().height();
    const int minimum = digits * font_size * 2;

    const double pixels_per_unit = aparallel / abs_size;

    if (!_unit) {
        return;
    }

    auto ruler_metric = _unit->getUnitMetric();
    if (!ruler_metric) {
        return;
    }

    unsigned scale_index;
    for (scale_index = 0; scale_index < ruler_metric->ruler_scale.size() - 1; ++scale_index) {
        if (ruler_metric->ruler_scale[scale_index] * pixels_per_unit > minimum) {
            break;
        }
    }

    unsigned divide_index;
    for (divide_index = 0; divide_index < ruler_metric->subdivide.size() - 1; ++divide_index) {
        if (ruler_metric->ruler_scale[scale_index] * pixels_per_unit < 5 * ruler_metric->subdivide[divide_index + 1]) {
            break;
        }
    }

    const int subdivisions = ruler_metric->subdivide[divide_index];
    const double units_per_major = ruler_metric->ruler_scale[scale_index];
    const double pixels_per_major = pixels_per_unit * units_per_major;
    const double pixels_per_tick = pixels_per_major / subdivisions;

    // Compute the shift so that ticks align to the lower bound.
    const double shift =
        -std::round(safe_frac(_lower * sign / units_per_major) * pixels_per_major * pixel_scale) / pixel_scale;

    painter.save();
    if (horiz) {
        painter.translate(shift, 0);
    } else {
        painter.translate(0, shift);
    }

    // Draw ticks: iterate over major tick positions, then sub-ticks within each.
    const int start = static_cast<int>(std::floor(_lower * sign / units_per_major));
    const int end = static_cast<int>(std::floor(_upper * sign / units_per_major));

    for (int major_i = start; major_i <= end; ++major_i) {
        const double major_pos = (major_i - start) * pixels_per_major;

        // Draw sub-ticks for this major interval
        for (int i = 0; i < subdivisions; ++i) {
            const int tick_width = 1;
            double position = Inkscape::pixel_align_line(i * pixels_per_tick, tick_width * pixel_scale, pixel_scale) -
                              0.5 * tick_width;
            double abs_pos = major_pos + position;

            // Height of tick
            int size = aperp - 8;
            bool major = true;
            for (int j = static_cast<int>(divide_index); j > 0; --j) {
                if (i % ruler_metric->subdivide[j] == 0) {
                    break;
                }
                size = size / 2 + 1;
                major = false;
            }
            if (i == 0) {
                major = true;
                size = size / 2 + 3;
            } else if (major) {
                size = size / 2 + 3;
            }

            // Skip ticks beyond visible range
            if (abs_pos < -pixels_per_major || abs_pos > aparallel + pixels_per_major) {
                continue;
            }

            const QColor& color = major ? tick_color : minor_color;
            painter.fillRect(to_rect(abs_pos, aperp - size, abs_pos + tick_width, aperp), color);
        }
    }

    struct Label {
        QString text;
        double start;
        double end;
        QColor color;
    };

    // zero-trimming formatter
    const auto format_selection_value = [](double value) {
        auto text = QString::number(value, 'f', 3);
        while (text.endsWith('0')) {
            text.chop(1);
        }
        if (text.endsWith('.')) {
            text.chop(1);
        }
        return text == "-0" ? QStringLiteral("0") : text;
    };

    const auto label_width = [this](const QString& text) {
        return static_cast<double>(fontMetrics().horizontalAdvance(text));
    };

    constexpr double gap = 3;
    std::vector<Label> selection_labels;
    if (_sel_lower != _sel_upper) {
        const double lower_position = _sel_lower - shift;
        const double upper_position = _sel_upper - shift;

        auto make_selection_label = [&](double position, double other_position, double value) {
            const auto text = format_selection_value(value);
            const double width = label_width(text);
            double start, end;
            if (position <= other_position) {
                start = position - gap - width;
                end = position - gap;
            } else {
                start = position + gap;
                end = position + gap + width;
            }
            return Label{text, start, end, sel_text};
        };

        selection_labels.push_back(make_selection_label(
            lower_position, upper_position, _lower + _sel_lower * sign / pixels_per_unit));
        selection_labels.push_back(make_selection_label(
            upper_position, lower_position, _lower + _sel_upper * sign / pixels_per_unit));
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(font());

    const auto draw_label = [&](const Label& label) {
        const int text_height = fontMetrics().ascent();
        painter.save();
        painter.setPen(label.color);
        if (horiz) {
            painter.translate(label.start, text_height + 2);
        } else {
            painter.translate(text_height + 2, label.start + label_width(label.text));
            painter.rotate(-90);
        }
        painter.drawText(0, 0, label.text);
        painter.restore();
    };

    for (const auto& label : selection_labels) {
        draw_label(label);
    }

    constexpr double selection_label_clearance = 10.0;
    for (int i = start; i <= end; ++i) {
        const int label_value = static_cast<int>(std::round(i * units_per_major * sign));
        const double position = std::round((i - start) * pixels_per_major) + gap;
        const auto text = QString::number(label_value);
        const double width = label_width(text);

        const Label candidate{text, position, position + width, text_color};
        const auto too_close_to_selection = [&, clearance = selection_label_clearance](const Label& selection) {
            return candidate.start < selection.end + clearance && candidate.end > selection.start - clearance;
        };
        if (std::none_of(selection_labels.begin(), selection_labels.end(), too_close_to_selection)) {
            draw_label(candidate);
        }
    }

    painter.restore();
#if 0
    // Draw edge line at the canvas-facing side (bottom for horizontal, right for vertical)
    // as a 1px pixel-grid-aligned filled rectangle.
    painter.setRenderHint(QPainter::Antialiasing, false);
    const int edge_thickness = 1;
    if (horiz) {
        double y = Inkscape::pixel_align_line(h, edge_thickness * pixel_scale, pixel_scale);
        painter.fillRect(QRectF(0, y, w, edge_thickness), tick_color);
    } else {
        double x = Inkscape::pixel_align_line(w, edge_thickness * pixel_scale, pixel_scale);
        painter.fillRect(QRectF(x, 0, edge_thickness, h), tick_color);
    }
#endif
}

void RulerWidget::drawMarker(QPainter& painter) {
    const auto horiz = _orientation == Qt::Horizontal;
    const auto w = width() - 1;
    const auto h = height() - 1;

    QPointF pos;
    if (horiz) {
        pos = {_position, static_cast<double>(h)};
    } else {
        pos = {static_cast<double>(w), _position};
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(pos);
    if (!horiz) {
        painter.rotate(-90);
    }

    const auto fg = palette().color(QPalette::Dark);
    painter.setBrush(fg);
    painter.setPen(Qt::NoPen);

    QPolygonF triangle;
    triangle << QPointF(0, 0) << QPointF(-half_width, -half_width) << QPointF(half_width, -half_width);
    painter.drawPolygon(triangle);

    painter.restore();
}

void RulerWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    QColor bkgnd = isDarkTheme() ? palette().color(QPalette::Window) : palette().color(QPalette::Base).darker(110);
    painter.fillRect(rect(), bkgnd);
    drawRuler(painter);
    if (_marker_visible) {
        drawMarker(painter);
    }
}

void RulerWidget::contextMenuEvent(QContextMenuEvent* event) {
    if (_menu) {
        _menu->popup(event->globalPos());
    }
}

void RulerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        _pressed = true;
        _dragged = false;
        _press_pos = event->position();
    }
    QWidget::mousePressEvent(event);
}

void RulerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (_pressed && !_dragged) {
        // Discard small movements without starting a drag, matching the GTK
        // ruler's use of /options/dragtolerance/value (see canvas-grid.cpp).
        auto prefs = Inkscape::Preferences::get();
        const int tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);
        if ((event->position() - _press_pos).manhattanLength() > tolerance) {
            _dragged = true;
            const double pos = _orientation == Qt::Horizontal
                ? static_cast<double>(event->position().x())
                : static_cast<double>(event->position().y());
            Q_EMIT rulerDragStarted(this, pos);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void RulerWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && _pressed) {
        if (!_dragged) {
            Q_EMIT rulerClicked();
        }
        _pressed = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void RulerWidget::createContextMenu() {
    _menu = new QMenu(this);
    _unit_action_group = new QActionGroup(_menu);
    _unit_action_group->setExclusive(true);

    for (auto unit_ptr : Inkscape::Util::UnitTable::get().units(Inkscape::Util::UNIT_TYPE_LINEAR)) {
        auto abbr = QString::fromStdString(unit_ptr->abbr);
        auto action = _menu->addAction(abbr);
        action->setCheckable(true);
        action->setData(abbr);
        _unit_action_group->addAction(action);
        connect(action, &QAction::triggered, this, [this, abbr] { Q_EMIT unitChanged(abbr); });
    }
}

} // namespace Linea::UI
