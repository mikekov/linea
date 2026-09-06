// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PdfImportDialog — Qt replacement for the GTK PdfImportDialog.
 *
 * This is a QWidget (not a QDialog) so it can be embedded in any container.
 * Call showDialog() to run it modally as a QDialog wrapper, or embed it
 * directly and connect to the buttonBox signals.
 *
 * Mirrors the interface of the original Inkscape::Extension::Internal::PdfImportDialog
 * but lives in the Linea::UI namespace and uses Qt widgets exclusively.
 */

#ifndef LINEA_UI_DIALOG_PDF_IMPORT_DIALOG_H
#define LINEA_UI_DIALOG_PDF_IMPORT_DIALOG_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QImage>
#include <QWidget>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "extension/internal/pdfinput/enums.h"
#include "extension/internal/pdfinput/poppler-utils.h"

#ifdef HAVE_POPPLER_CAIRO
#include <cairo.h>
#endif

class QCheckBox;
class QComboBox;
class QDialog;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSlider;
class QStandardItemModel;
class QTabWidget;
class QTreeView;
class QPushButton;

class PDFDoc;
class Page;

struct _PopplerDocument;
typedef struct _PopplerDocument PopplerDocument;

namespace Inkscape::Extension {
class Input;
}

namespace Inkscape::Async::Channel {
class Dest;
}

namespace Ui {
class PdfImportDialog;
}

namespace Linea::UI {

class PdfPreviewArea;

/// Import method selection (mirrors PdfImportType from the original).
enum class PdfImportType : unsigned char {
    PDF_IMPORT_INTERNAL = 0,
    PDF_IMPORT_CAIRO,
};

/**
 * Qt-based PDF import settings widget.
 *
 * Layout is fully defined in pdf-import-dialog.ui. This class wires up
 * the widgets, populates combo boxes, manages the font list model,
 * renders page previews, and exposes getters for the caller to read
 * the user's choices after the dialog is accepted.
 */
class PdfImportDialog : public QWidget {
    Q_OBJECT

public:
    PdfImportDialog(std::shared_ptr<PDFDoc> doc, const char* uri, Inkscape::Extension::Input* mod,
                    QWidget* parent = nullptr);
    ~PdfImportDialog() override;

    /// Run as a modal QDialog. Returns true if accepted, false if cancelled.
    bool showDialog();

    // ---- getters (call after showDialog() returns true) ----
    bool getImportPages() const;
    std::string getSelectedPages() const;
    PdfImportType getImportMethod() const;
    FontStrategies getFontStrategies() const;
    void setFontStrategies(const FontStrategies& fs);

private Q_SLOTS:
    void onPageNumberChanged();
    void onPrevPage();
    void onNextPage();
    void onMeshSliderChanged(int value);
    void onClipToChanged(int index);
    void onGroupByChanged(int index);
    void onFontRenderingChanged(int index);
    void onEmbedImagesToggled(bool checked);
    void onConvertColorsToggled(bool checked);
    void onImportPagesToggled(bool checked);

private:
    void setupCombos();
    void setupConnections();
    void setFonts(const FontList& fonts);
    void setPreviewPage(int page);
    void renderPreview();

    std::unique_ptr<Ui::PdfImportDialog> _ui;

    Inkscape::Extension::Input* _mod = nullptr;
    std::shared_ptr<PDFDoc> _pdfDoc;
    std::string _uri;

    // Font list model
    QStandardItemModel* _fontModel = nullptr;

    // Preview state
    int _totalPages = 0;
    int _previewPage = 1;
    std::string _currentPages = "all";
    bool _renderThumb = false;

    int _previewWidth = 200;
    int _previewHeight = 300;

#ifdef HAVE_POPPLER_CAIRO
    PopplerDocument* _popplerDoc = nullptr;
    bool _previewRenderingInProgress = false;
    std::unordered_map<int, std::shared_ptr<cairo_surface_t>> _cairoSurfaces;
    std::vector<Inkscape::Async::Channel::Dest> _channels;
#endif

    // Thumbnail data (non-CAIRO path)
    unsigned char* _thumbData = nullptr;
    int _thumbWidth = 0;
    int _thumbHeight = 0;
    int _thumbRowstride = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_DIALOG_PDF_IMPORT_DIALOG_H
