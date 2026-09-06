// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FontList — simple font selector built on top of VirtualTreeList.
 */

#ifndef LINEA_UI_FONT_LIST_H
#define LINEA_UI_FONT_LIST_H

#include <atomic>
#include <sigc++/scoped_connection.h>

#include <QRect>

#include "virtual-tree-list.h"
#include "util/font-discovery.h"

namespace Linea::UI {

/**
 * Simple, programmable font selector with no extra UI chrome.
 *
 * It displays a list of fonts, supports sorting, and lets the caller adjust
 * the preview size. Row heights are calculated on demand while drawing, so
 * expensive font metrics are only computed for the visible rows.
 *
 * The list is virtual: it does not own the font data and only keeps the fonts
 * that are currently displayed.
 */
class FontList : public VirtualTreeList {
    Q_OBJECT

public:
    explicit FontList(QWidget* parent = nullptr);
    ~FontList() override;

    // sorting order
    void setFontOrder(Inkscape::FontOrder order);
    Inkscape::FontOrder fontOrder() const;

    // preview text size in points
    void setPreviewSize(int size);
    int previewSize() const;

    // optional sample text rendered in the font preview; if empty, the family name is used
    void setSampleText(const QString& text);
    QString sampleText() const;

    // replace the displayed font families with a custom list; grouping and sorting are still applied
    void setFonts(const std::vector<std::vector<Inkscape::FontInfo>>& fontFamilies);

    // current fontspec of the selected font, or an empty string if none
    QString currentFontspec() const;

    // show requested font in the list and select it; in family mode it expands the family if needed
    void setCurrentFont(const QString& fontspec);

    // reload fonts from the system font discovery service
    void refresh();

    // filter fonts by matching the font name
    void filterFonts(const QString& match);

Q_SIGNALS:
    // emitted when the selected font changes
    void fontChanged(const QString& fontspec);
    // emitted when the user activates a row (double-click or Enter)
    void fontSelected(const QString& fontspec);

private:
    void loadFonts();
    void updateDisplayFonts();
    void onRowSelected(int index);
    void onRowOpened(int index);
    void drawRow(QPainter* painter, const ItemIndex& item, const QRect& rect, bool selected);
    enum DrawMode { DrawFamily, DrawStyle, DrawFullName };
    void drawFontRow(QPainter* painter, const Inkscape::FontInfo& info, DrawMode mode, int count, const QRect& rect, bool selected);
    QFontMetrics makePreviewFont(const QString& familyName, const QString& styleName, QFont& outFont) const;
    void drawSample(QPainter* painter, const QFont& font, const QFontMetrics& metrics, const QString& familyName, int x, int y, int width) const;
    int findFamilyIndex(const QString& fontspec) const;
    int findFamilyRow(int familyIndex) const;
    int findStyleRow(const QString& fontspec) const;
    const Inkscape::FontInfo* getFontInfo(const ItemIndex& index) const;
    void navigateToMatchingFont(const QString& text);

    Inkscape::FontOrder _order = Inkscape::FontOrder::ByFamily;
    int _previewSize = 0; // set by setPreviewSize() in the constructor
    QString _sampleText;
    QString _pendingFontspec;
    std::vector<Inkscape::FontInfo> _allFonts;
    std::vector<std::vector<Inkscape::FontInfo>> _fontFamilies;
    std::vector<Inkscape::FontInfo> _displayFonts;
    std::vector<QString> _displayFontspecs;
    sigc::scoped_connection _fontConnection;
    static constexpr int topMargin = 4;
    static constexpr int bottomMargin = 4;
    static constexpr int spacing = 2;
    static constexpr int leftMargin = 2;
    static constexpr int rightMargin = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_FONT_LIST_H
