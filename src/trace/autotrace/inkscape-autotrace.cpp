// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * This is the C++ glue between Inkscape and Autotrace
 *//*
 *
 * Authors:
 *   Marc Jeanmougin
 *
 * Copyright (C) 2018 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 */
#include <2geom/path-sink.h>
#include <glibmm/i18n.h>

#include "inkscape-autotrace.h"
#include "async/progress.h"
#include "trace/imagemap-gdk.h"
#include "util/safe-printf.h"

extern "C" {
#include <autotrace/autotrace.h>
#include <autotrace/output.h>
#include <autotrace/spline.h>
}

namespace Inkscape::Trace::Autotrace {
namespace {

struct at_splines_deleter { void operator()(at_splines_type *p) { at_splines_free(p); }; };
using at_splines_uniqptr = std::unique_ptr<at_splines_type, at_splines_deleter>;

} // namespace

AutotraceTracingEngine::AutotraceTracingEngine()
{
    // Create options struct, automatically filled with defaults.
    opts = at_fitting_opts_new();
    opts->background_color = at_color_new(255, 255, 255);
    autotrace_init();
}

AutotraceTracingEngine::~AutotraceTracingEngine()
{
    at_fitting_opts_free(opts);
}

QImage AutotraceTracingEngine::preview(const BitmapView& bmp)
{
    // Todo: Actually generate a meaningful preview.
    return bitmapToQImagePreview(bmp);
}

TraceResult AutotraceTracingEngine::trace(const BitmapView& bmp, Async::Progress<double>& progress)
{
    auto rgb = bitmapToRgb8Packed(bmp);

    at_bitmap bitmap;
    bitmap.height = bmp.height;
    bitmap.width  = bmp.width;
    bitmap.bitmap = rgb.data();
    bitmap.np     = 3;

    auto throttled = Async::ProgressStepThrottler(progress, 0.02);
    auto sub_trace = Async::SubProgress(throttled, 0.0, 0.8);
    
    auto splines = at_splines_uniqptr(at_splines_new_full(
        &bitmap, opts,
        nullptr, nullptr,
        [] (gfloat frac, gpointer data) { reinterpret_cast<decltype(sub_trace)*>(data)->report(frac); }, &sub_trace,
        [] (gpointer data) -> gboolean { return !reinterpret_cast<decltype(sub_trace)*>(data)->keepgoing(); }, &sub_trace
    ));
    // at_output_write_func wfunc = at_output_get_handler_by_suffix("svg");
    // at_spline_writer *wfunc = at_output_get_handler_by_suffix("svg");
    // at_splines_write(wfunc, stdout, "", NULL, splines, NULL, NULL);

    sub_trace.report_or_throw(1.0);
    auto sub_convert = Async::SubProgress(throttled, 0.8, 0.2);

    int height = splines->height;
    at_spline_list_type list;
    at_color last_color = { 0, 0, 0 };

    std::string style;
    Geom::PathBuilder pathbuilder;
    TraceResult res;

    auto get_style = [&] {
        char color[10];
        safeprintf(color, "#%02x%02x%02x;", list.color.r, list.color.g, list.color.b);

        std::stringstream ss;
        ss << (splines->centerline || list.open ? "stroke:" : "fill:") << color
           << (splines->centerline || list.open ? "fill:" : "stroke:") << "none";

        return ss.str();
    };

    auto to_geom = [=] (at_real_coord const &c) {
        return Geom::Point(c.x, height - c.y);
    };

    int const num_splines = SPLINE_LIST_ARRAY_LENGTH(*splines);
    for (int list_i = 0; list_i < num_splines; list_i++) {
        sub_convert.report_or_throw((double)list_i / num_splines);

        list = SPLINE_LIST_ARRAY_ELT(*splines, list_i);

        if (list_i == 0 || !at_color_equal(&list.color, &last_color)) {
            if (list_i > 0) {
                if (!(splines->centerline || list.open)) {
                    pathbuilder.closePath();
                } else {
                    pathbuilder.flush();
                }
                res.emplace_back(std::move(style), pathbuilder.peek());
                pathbuilder.clear();
            }

            style = get_style();
        }

        auto const first = SPLINE_LIST_ELT(list, 0);
        pathbuilder.moveTo(to_geom(START_POINT(first)));

        for (int spline_i = 0; spline_i < SPLINE_LIST_LENGTH(list); spline_i++) {
            auto const spline = SPLINE_LIST_ELT(list, spline_i);

            if (SPLINE_DEGREE(spline) == AT_LINEARTYPE) {
                pathbuilder.lineTo(to_geom(END_POINT(spline)));
            } else {
                pathbuilder.curveTo(to_geom(CONTROL1(spline)), to_geom(CONTROL2(spline)), to_geom(END_POINT(spline)));
            }

            last_color = list.color;
        }
    }

    if (SPLINE_LIST_ARRAY_LENGTH(*splines) > 0) {
        if (!(splines->centerline || list.open)) {
            pathbuilder.closePath();
        } else {
            pathbuilder.flush();
        }
        res.emplace_back(std::move(style), pathbuilder.peek());
    }

    return res;
}

void AutotraceTracingEngine::setColorCount(unsigned color_count)
{
    opts->color_count = color_count;
}

void AutotraceTracingEngine::setCenterLine(bool centerline)
{
    opts->centerline = centerline;
}

void AutotraceTracingEngine::setPreserveWidth(bool preserve_width)
{
    opts->preserve_width = preserve_width;
}

void AutotraceTracingEngine::setFilterIterations(unsigned filter_iterations)
{
    opts->filter_iterations = filter_iterations;
}

void AutotraceTracingEngine::setErrorThreshold(float error_threshold)
{
    opts->error_threshold = error_threshold;
}

} // namespace Inkscape::Trace::Autotrace
