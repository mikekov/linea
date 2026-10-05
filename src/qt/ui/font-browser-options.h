// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FontBrowserOptions — font preview options popup content for FontBrowser.
 */

#ifndef LINEA_UI_FONT_BROWSER_OPTIONS_H
#define LINEA_UI_FONT_BROWSER_OPTIONS_H

#include <QWidget>
#include <memory>

namespace Ui {
class FontBrowserOptions;
}

namespace Linea::UI {

/**
 * Options panel shown inside the font browser's options popup.
 *
 * Mirrors the Inkscape font browser's options popover: sample text with
 * presets, a "show font name" toggle and a preview size slider.
 *
 * The widget only exposes the option values and reports changes; applying
 * them to the font list and persisting them is up to the host.
 */
class FontBrowserOptions : public QWidget {
    Q_OBJECT

public:
    explicit FontBrowserOptions(QWidget* parent = nullptr);
    ~FontBrowserOptions() override;

    void setSampleText(const QString& text);
    QString sampleText() const;

    void setShowFontName(bool show);
    bool showFontName() const;

    // preview size in percent of the base preview size
    void setPreviewPercent(int percent);
    int previewPercent() const;

Q_SIGNALS:
    void sampleTextChanged(const QString& text);
    void showFontNameChanged(bool show);
    void previewPercentChanged(int percent);

private:
    std::unique_ptr<Ui::FontBrowserOptions> ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_FONT_BROWSER_OPTIONS_H
