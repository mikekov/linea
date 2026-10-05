// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FontList — simple font selector built on top of VirtualTreeList.
 */
/*
 * Authors:
 *   Michael Kowalski
 */

#include "font-list.h"

#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPolygon>
#include <QTextOption>
#include "util/font-discovery.h"
#include "util/font-tags.h"

namespace {

// construct font name from Pango face and family;
// return font name as it is recorded in the font itself, as far as Pango allows it
QString get_full_name(const Inkscape::FontInfo& font_info) {
    return QString::fromUtf8(Inkscape::get_full_font_name(font_info.ff, font_info.face).raw());
}

// compare fontspecs ignoring their variable-font variations suffix
bool same_font(const QString& lhs, const QString& rhs) {
    return Inkscape::get_fontspec_without_variants(Glib::ustring(lhs.toUtf8().constData())) ==
           Inkscape::get_fontspec_without_variants(Glib::ustring(rhs.toUtf8().constData()));
}

} // namespace

namespace Linea::UI {

FontList::FontList(QWidget* parent)
    : VirtualTreeList(parent)
{
    setPreviewSize(20);
    setRowGap(1);
    // setShowGap(true);

    setDrawItemFunc([this](QPainter* painter, const ItemIndex& item, const QRect& rect, bool selected) {
        drawRow(painter, item, rect, selected);
    });

    setGetRowHeightFunc([this](const ItemIndex& row){
        auto info = getFontInfo(row);
        if (!info) return 0;

        // Sample text to test
        QString sampleText = "ABZagf123À";
        // Set font for drawing using helper function
        auto familyName = QString::fromUtf8(info->ff->get_name().raw());
        auto styleName = info->face ? QString::fromUtf8(info->face->get_name().raw()) : QString();
        QFont nameFont = QApplication::font("FontList");
        nameFont.setPointSize(10);
        QFontMetrics nameMetrics(nameFont);
        QFont sampleFont;
        QFontMetrics sampleMetrics = makePreviewFont(familyName, styleName, sampleFont);
        int rowHeight = topMargin + sampleMetrics.height() + bottomMargin;
        if (_showFontName) {
            rowHeight += nameMetrics.height() + spacing;
        }
        return rowHeight;
    });

    setGetSubitemCountFunc([this](int index){
        if (_order == Inkscape::FontOrder::ByFamily) {
            int count = size_t(index) < _fontFamilies.size() ? _fontFamilies[index].size() : 0;
            // don't expand single-font families
            return count > 1 ? count : 0;
        }
        return 0;
    });

    // Connect alphanumeric input for filtering
    connect(this, &VirtualTreeList::alphanumericInput, this, [this](const QString& text) {
        navigateToMatchingFont(text);
    });

    connect(this, &VirtualTreeList::selectionChanged, this, &FontList::onItemSelected);
    connect(this, &VirtualTreeList::itemActivated, this, &FontList::onItemActivated);

    refresh();
}

FontList::~FontList() = default;

const Inkscape::FontInfo* FontList::getFontInfo(const ItemIndex& index) const {
    if (_order != Inkscape::FontOrder::ByFamily) {
        if (size_t(index.itemIndex) >= _displayFonts.size()) return nullptr;

        return &_displayFonts[index.itemIndex];
    }
    else if (index.isTopLevel()) {
        if (size_t(index.itemIndex) >= _fontFamilies.size()) return nullptr;

        const auto& family = _fontFamilies[index.itemIndex];
        auto idx = Inkscape::get_family_font_index(family);
        return &family[idx];
    }
    else {
        if (size_t(index.parentIndex) >= _fontFamilies.size()) return nullptr;

        const auto& family = _fontFamilies[index.parentIndex];
        if (size_t(index.itemIndex) >= family.size()) return nullptr;

        const auto& style = family[index.itemIndex];
        return &style;
    }
    return nullptr;
}

void FontList::setFontOrder(Inkscape::FontOrder order) {
    if (_order != order) {
        _order = order;
        // text filtering is order-dependent (family vs full font names)
        rebuildFontList();
    }
}

Inkscape::FontOrder FontList::fontOrder() const {
    return _order;
}

void FontList::setPreviewSize(int size) {
    size = std::max(1, size);
    if (_previewSize != size) {
        _previewSize = size;
        invalidate();
    }
}

int FontList::previewSize() const {
    return _previewSize;
}

void FontList::setSampleText(const QString& text) {
    if (_sampleText != text) {
        _sampleText = text;
        invalidate();
    }
}

QString FontList::sampleText() const {
    return _sampleText;
}

void FontList::setShowFontName(bool show) {
    if (_showFontName != show) {
        _showFontName = show;
        invalidate();
    }
}

bool FontList::showFontName() const {
    return _showFontName;
}

void FontList::setFonts(const std::vector<std::vector<Inkscape::FontInfo>>& fontFamilies) {
    _sourceFamilies = fontFamilies;
    tagFontFaces();
    rebuildFontList();
}

QString FontList::currentFontspec() const {
    auto info = getFontInfo(selectedItem());
    return info ? fontspecOf(*info) : QString();
}

void FontList::setCurrentFont(const QString& fontspec) {
    if (fontspec.isEmpty()) {
        setSelectedItem(ItemIndex());
        return;
    }
    if (_fontFamilies.empty()) {
        _pendingFontspec = fontspec;
        return;
    }

    if (_order == Inkscape::FontOrder::ByFamily) {
        int familyIndex = findFamilyIndex(fontspec);
        if (familyIndex < 0) return;

        const auto& family = _fontFamilies[familyIndex];
        if (family.size() > 1) {
            for (int j = 0; j < static_cast<int>(family.size()); ++j) {
                if (same_font(fontspecOf(family[j]), fontspec)) {
                    setItemExpanded(familyIndex, true);
                    setSelectedItem(ItemIndex(j, familyIndex));
                    return;
                }
            }
        }
        setSelectedItem(ItemIndex(familyIndex));
        return;
    }

    for (int i = 0; i < static_cast<int>(_displayFonts.size()); ++i) {
        if (same_font(fontspecOf(_displayFonts[i]), fontspec)) {
            setSelectedItem(ItemIndex(i));
            return;
        }
    }
}

void FontList::refresh() {
    loadFonts();
}

void FontList::loadFonts() {
    _fontConnection = Inkscape::FontDiscovery::get().connect_to_fonts(
        [this](const Inkscape::FontDiscovery::MessageType& msg) {
            if (auto result = Inkscape::Async::Msg::get_result(msg)) {
                _sourceFamilies.clear();
                for (auto& family : **result) {
                    if (!family.empty()) {
                        _sourceFamilies.push_back(family);
                    }
                }
                tagFontFaces();
                rebuildFontList();
            }
        });
}

void FontList::updateDisplayFonts() {
    _displayFonts.clear();
    _displayFontspecs.clear();
//TODO

    if (_order == Inkscape::FontOrder::ByFamily) {
        setExpanderEnabled(true);
        setItemCount(_fontFamilies.size());
    }
    else {
        _displayFonts = _allFonts;
        Inkscape::sort_fonts(_displayFonts, _order, true);
        // _displayItems.resize(_displayFonts.size(), {-1, -1});
        setExpanderEnabled(false);
        setItemCount(_displayFonts.size());
    }

    // _displayFontspecs.reserve(_displayFonts.size());
    // for (auto& info : _displayFonts) {
    //     auto spec = Inkscape::get_inkscape_fontspec(info.ff, info.face, info.variations);
    //     _displayFontspecs.push_back(QString::fromUtf8(spec.raw()));
    // }

    // _expanderRects.assign(_displayItems.size(), QRect());

    // setItemCount(_fontFamilies.size());

    if (!_pendingFontspec.isEmpty()) {
        auto pending = _pendingFontspec;
        _pendingFontspec.clear();
        setCurrentFont(pending);
    }
}

int FontList::findFamilyIndex(const QString& fontspec) const {
    for (int i = 0; i < static_cast<int>(_fontFamilies.size()); ++i) {
        for (auto& info : _fontFamilies[i]) {
            auto spec = Inkscape::get_inkscape_fontspec(info.ff, info.face, info.variations);
            if (same_font(QString::fromUtf8(spec.raw()), fontspec)) {
                return i;
            }
        }
    }
    return -1;
}

int FontList::findFamilyRow(int familyIndex) const {
    // if (familyIndex < 0 || familyIndex >= static_cast<int>(_fontFamilies.size())) return -1;
    // for (int i = 0; i < static_cast<int>(_displayItems.size()); ++i) {
    //     if (_displayItems[i].isFamily() && _displayItems[i].familyIndex == familyIndex) {
    //         return i;
    //     }
    // }
    return -1;
}

int FontList::findStyleRow(const QString& fontspec) const {
    for (int i = 0; i < static_cast<int>(_displayFontspecs.size()); ++i) {
        if (_displayFontspecs[i] == fontspec) {
            return i;
        }
    }
    return -1;
}

QString FontList::fontspecOf(const Inkscape::FontInfo& info) const {
    return QString::fromUtf8(Inkscape::get_inkscape_fontspec(info.ff, info.face, info.variations).raw());
}

void FontList::onItemSelected(const ItemIndex& index) {
    if (auto info = getFontInfo(index)) {
        Q_EMIT fontChanged(fontspecOf(*info));
    }
}

void FontList::onItemActivated(const ItemIndex& index) {
    if (auto info = getFontInfo(index)) {
        Q_EMIT fontSelected(fontspecOf(*info));
    }
}

void FontList::drawRow(QPainter* painter, const ItemIndex& row, const QRect& rect, bool selected) {
    auto info = getFontInfo(row);
    if (!info) return;

    if (_order != Inkscape::FontOrder::ByFamily) {
        auto indented = rect.adjusted(10, 0, 0, 0);
        drawFontRow(painter, *info, DrawFullName, 0, indented, selected);
    } else if (row.isTopLevel()) {
        const auto& family = _fontFamilies[row.itemIndex];
        auto familyFont = Inkscape::get_family_font(family);
        auto count = static_cast<int>(family.size());
        drawFontRow(painter, familyFont, DrawFamily, count > 1 ? count : 0, rect, selected);
    } else {
        drawFontRow(painter, *info, DrawStyle, 0, rect, selected);
    }
}

QFontMetrics FontList::makePreviewFont(const QString& familyName, const QString& styleName, QFont& outFont) const {
    outFont = QFont(familyName, _previewSize);
    if (!styleName.isEmpty()) {
        outFont.setStyleName(styleName);
    }
    return QFontMetrics(outFont);
}

void FontList::drawSample(QPainter* painter, const QFont& font, const QFontMetrics& metrics, const QString& sample, int x, int y, int width) const {
    painter->setFont(font);

    int textWidth = metrics.horizontalAdvance(sample);

    if (textWidth <= width) {
        // Text fits, draw normally
        painter->drawText(QPoint(x, y), sample);
    } else {
        // Text is too long, draw with fade-out effect
        painter->save();

        // Get current text color
        QColor textColor = painter->pen().color();

        // Create gradient from text color to transparent
        double fadeWidth = 20.0;
        QLinearGradient gradient(x, 0, x + width, 0);
        gradient.setColorAt(0.0, textColor);
        gradient.setColorAt(1.0 - (fadeWidth / width), textColor);
        gradient.setColorAt(1.0, Qt::transparent);

        // Set the gradient as the pen
        painter->setPen(QPen(gradient, 0));

        // Draw the text with the gradient
        painter->drawText(QPoint(x, y), sample);

        painter->restore();
    }
}

void FontList::drawFontRow(QPainter* painter, const Inkscape::FontInfo& info, DrawMode mode, int count, const QRect& rect, bool selected) {
    auto familyName = QString::fromUtf8(info.ff->get_name().raw());
    auto styleName = info.face ? QString::fromUtf8(info.face->get_name().raw()) : QString();
    auto fontName = mode == DrawFullName ? get_full_name(info) : (mode == DrawFamily ? familyName : styleName);

    auto sample = _sampleText.isEmpty() ? fontName : _sampleText;

    QFont nameFont = painter->font();
    nameFont.setPointSize(10);
    QFontMetrics nameMetrics(nameFont);

    QFont sampleFont;
    QFontMetrics sampleMetrics = makePreviewFont(familyName, styleName, sampleFont);

    QMargins margins(leftMargin, topMargin, rightMargin, bottomMargin);
    auto area = rect.marginsRemoved(margins);
    if (area.isEmpty()) return;

    painter->save();

    auto textPen = selected ? palette().color(QPalette::HighlightedText) : palette().color(QPalette::Text);
    painter->setPen(textPen);

    // sample rendered in the selected font
    int sampleBaseline = area.top() + sampleMetrics.ascent();
    drawSample(painter, sampleFont, sampleMetrics, sample, area.left(), sampleBaseline, area.width());

    if (!_showFontName) {
        painter->restore();
        return;
    }

    // font name
    painter->setFont(nameFont);
    int nameBaseline = area.bottom();
    QString elidedName = nameMetrics.elidedText(fontName, Qt::ElideRight, area.width());
    painter->drawText(QPoint(area.left(), nameBaseline), elidedName);

    if (count > 0) {
        // count badge
        auto badge = QString::number(count);
        int badgeWidth = nameMetrics.horizontalAdvance(badge) + 8;
        int badgeHeight = nameMetrics.height();
        int badgeX = area.left() + nameMetrics.horizontalAdvance(elidedName) + 8;
        // Position badge so text appears centered when drawn at same baseline as font name
        int ascent = nameMetrics.ascent();
        int descent = nameMetrics.descent();
        int badgeY = area.bottom() - (ascent - descent) / 2 - badgeHeight / 2;
        QRect badgeRect(badgeX, badgeY, badgeWidth, badgeHeight);

        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette().color(QPalette::Mid));
        painter->drawRoundedRect(badgeRect, badgeHeight/2, badgeHeight/2);
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(textPen);
        painter->drawText(badgeRect, Qt::AlignCenter, badge);
    }

    painter->restore();
}

void FontList::rebuildFontList() {
    _fontFamilies.clear();
    _allFonts.clear();

    for (auto& family : _sourceFamilies) {
        std::vector<Inkscape::FontInfo> faces;
        for (auto& info : family) {
            if (_faceFilter && !_faceFilter(info)) {
                continue;
            }
            // in flat mode the text filter applies to each font's full name;
            // family grouping checks the representative font instead (see below)
            if (_order != Inkscape::FontOrder::ByFamily && !_textFilter.isEmpty() &&
                !get_full_name(info).contains(_textFilter, Qt::CaseInsensitive)) {
                continue;
            }
            faces.push_back(info);
        }
        if (faces.empty()) continue;

        if (_order == Inkscape::FontOrder::ByFamily && !_textFilter.isEmpty()) {
            int rep = Inkscape::get_family_font_index(faces);
            if (rep < 0 || !get_full_name(faces[rep]).contains(_textFilter, Qt::CaseInsensitive)) {
                continue;
            }
        }

        _allFonts.insert(_allFonts.end(), faces.begin(), faces.end());
        _fontFamilies.push_back(std::move(faces));
    }

    Inkscape::sort_font_families(_fontFamilies, true);
    updateDisplayFonts();
    Q_EMIT fontCountChanged(fontCount(), totalFontCount());
}

void FontList::tagFontFaces() {
    auto& tags = Inkscape::FontTags::get();
    for (auto& family : _sourceFamilies) {
        for (auto& font : family) {
            auto kind = font.family_kind >> 8;
            if (kind == 10) {
                tags.tag_font(font.face, "script");
            } else if (kind >= 1 && kind <= 5) {
                tags.tag_font(font.face, "serif");
            } else if (kind == 8) {
                tags.tag_font(font.face, "sans");
            } else if (kind == 12) {
                tags.tag_font(font.face, "symbols");
            }
            if (font.monospaced) tags.tag_font(font.face, "monospace");
            if (font.variable_font) tags.tag_font(font.face, "variable");
            if (font.oblique) tags.tag_font(font.face, "oblique");
        }
    }
}

void FontList::filterFonts(const QString& match) {
    if (_textFilter == match) return;
    _textFilter = match;
    rebuildFontList();
}

void FontList::setFontFilter(FontFilter filter) {
    _faceFilter = std::move(filter);
    rebuildFontList();
}

int FontList::fontCount() const {
    return static_cast<int>(_allFonts.size());
}

int FontList::totalFontCount() const {
    int total = 0;
    for (auto& family : _sourceFamilies) {
        total += static_cast<int>(family.size());
    }
    return total;
}

void FontList::navigateToMatchingFont(const QString& text) {
    int parent = -1;
    int style = -1;

    if (_order == Inkscape::FontOrder::ByFamily) {
        // find the first font that starts with the given text
        for (int i = 0; i < _fontFamilies.size(); ++i) {
            const auto& family = _fontFamilies[i];
            int idx = Inkscape::get_family_font_index(family);
            if (idx < 0) continue;

            if (get_full_name(family[idx]).startsWith(text, Qt::CaseInsensitive)) {
                parent = i;
                break;
            }
            for (int j = 0; j < family.size(); ++j) {
                if (get_full_name(family[j]).startsWith(text, Qt::CaseInsensitive)) {
                    parent = i;
                    style = j;
                    break;
                }
            }
        }
    }
    else {
        for (int i = 0; i < _displayFonts.size(); ++i) {
            if (get_full_name(_displayFonts[i]).startsWith(text, Qt::CaseInsensitive)) {
                parent = i;
                break;
            }
        }
    }

    if (parent >= 0) {
        auto item = style >= 0 ? ItemIndex(style, parent) : ItemIndex(parent);
        setSelectedItem(item);
    }
}

} // namespace Linea::UI
