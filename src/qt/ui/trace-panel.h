// SPDX-License-Identifier: GPL-2.0-or-later
// Trace panel — Qt widget for bitmap tracing settings.
//
// This is a plain QWidget with no knowledge of the document, desktop, or selection.
// It communicates entirely via signals. The caller is responsible for:
//  - Passing a BitmapView to trace/preview
//  - Running the async trace/preview operations
//  - Feeding results back via setPreview() / setTraceProgress() / setTraceComplete()

#ifndef LINEA_UI_TRACE_PANEL_H
#define LINEA_UI_TRACE_PANEL_H

#include <QImage>
#include <QWidget>
#include <memory>
#include <cairomm/surface.h>

#include "trace/trace.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QStackedWidget;
namespace Ui {
class TracePanel;
}
QT_END_NAMESPACE

namespace Linea::UI {
class SpinScale;
class TabStrip;

namespace Trace = Inkscape::Trace;

class TracePanel : public QWidget {
    Q_OBJECT

public:
    explicit TracePanel(QWidget* parent = nullptr);
    ~TracePanel() override;

    // Set the image to be traced/previewed. The panel does not own the pixel data;
    // the caller must keep it alive until trace/preview completes.
    void setImage(Trace::BitmapView bmp);

    // Set the SIOX mask surface (optional, for user-assisted tracing).
    void setSioxMask(Cairo::RefPtr<Cairo::ImageSurface> mask);

    // Whether live preview updates are enabled.
    bool livePreviewEnabled() const;

    // Take ownership of the tracing engine built from the current panel state.
    // Call this in response to traceRequested() / previewRequested().
    std::unique_ptr<Trace::TracingEngine> takeEngine();

    // Whether SIOX is enabled for the current page.
    bool sioxEnabled() const;

    // The bitmap currently set on the panel.
    Trace::BitmapView bitmap() const { return _bmp; }

    // The SIOX mask currently set on the panel (may be null).
    Cairo::RefPtr<Cairo::ImageSurface> sioxMask() const { return _sioxMask; }

Q_SIGNALS:
    // Emitted when the user clicks "Apply".
    // Call takeEngine() to get the configured tracing engine.
    void traceRequested();

    // Emitted when the user requests a preview update (live or manual).
    // Call takeEngine() to get the configured tracing engine.
    void previewRequested();

    // Emitted when the user clicks "Abort".
    void traceAborted();

    // Emitted when the live-preview checkbox toggles.
    void livePreviewChanged(bool enabled);

public Q_SLOTS:
    // Display a preview image in the preview area.
    void setPreview(QImage image);

    // Update the progress bar (0.0 to 1.0).
    void setTraceProgress(double fraction);

    // Switch the bottom bar back to the Apply/Reset state.
    void setTraceComplete();

private Q_SLOTS:
    void onApplyClicked();
    void onAbortClicked();
    void onResetClicked();
    void onTabChanged(QWidget* tab);
    void onParamsChanged();
    void adjustParamsVisible();

private:
    struct TraceConfig {
        std::unique_ptr<Trace::TracingEngine> engine;
        bool sioxEnabled;
    };
    TraceConfig buildTraceConfig() const;
    void requestPreview();

    std::unique_ptr<Ui::TracePanel> _ui;

    // State
    Trace::BitmapView _bmp{};
    Cairo::RefPtr<Cairo::ImageSurface> _sioxMask;
    bool _tracing = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_TRACE_PANEL_H
