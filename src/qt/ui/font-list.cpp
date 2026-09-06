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

namespace {

// construct font name from Pango face and family;
// return font name as it is recorded in the font itself, as far as Pango allows it
QString get_full_name(const Inkscape::FontInfo& font_info) {
    return QString::fromUtf8(Inkscape::get_full_font_name(font_info.ff, font_info.face).raw());
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
        int rowHeight = topMargin + nameMetrics.height() + spacing + sampleMetrics.height() + bottomMargin;
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
        // filterFonts(text);
        navigateToMatchingFont(text);
    });

    // connect(this, &SimpleList::rowSelected, this, &FontList::onRowSelected);
    // connect(this, &SimpleList::rowOpened, this, &FontList::onRowOpened);

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
        updateDisplayFonts();
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

void FontList::setFonts(const std::vector<std::vector<Inkscape::FontInfo>>& fontFamilies) {
    _fontFamilies = fontFamilies;
    _allFonts.clear();
    for (auto& family : _fontFamilies) {
        if (!family.empty()) {
            _allFonts.insert(_allFonts.end(), family.begin(), family.end());
        }
    }
    // _expandedFamilies.clear();
    Inkscape::sort_font_families(_fontFamilies, true);
    updateDisplayFonts();
}

QString FontList::currentFontspec() const {
    int row = -1;// selectedRow();
    if (row < 0 || row >= static_cast<int>(_displayFontspecs.size())) {
        return QString();
    }
    return _displayFontspecs[row];
}

void FontList::setCurrentFont(const QString& fontspec) {
    /*
    if (fontspec.isEmpty()) {
        setSelectedRow(-1);
        return;
    }
    if (_displayFontspecs.empty()) {
        _pendingFontspec = fontspec;
        return;
    }

    auto row = findStyleRow(fontspec);
    if (row >= 0) {
        setSelectedRow(row);
        return;
    }

    if (_order == Inkscape::FontOrder::ByFamily) {
        auto familyIndex = findFamilyIndex(fontspec);
        if (familyIndex < 0) return;
        if (_fontFamilies[familyIndex].size() <= 1) {
            auto familyRow = findFamilyRow(familyIndex);
            if (familyRow >= 0) {
                setSelectedRow(familyRow);
            }
            return;
        }
        _expandedFamilies.insert(familyIndex);
        updateDisplayFonts();

        row = findStyleRow(fontspec);
        if (row >= 0) {
            setSelectedRow(row);
            return;
        }
    }
    */
}

void FontList::refresh() {
    loadFonts();
}

void FontList::loadFonts() {
    printf("loading fonts...\n");
    _fontConnection = Inkscape::FontDiscovery::get().connect_to_fonts(
        [this](const Inkscape::FontDiscovery::MessageType& msg) {
            if (auto result = Inkscape::Async::Msg::get_result(msg)) {
                _fontFamilies.clear();
                _allFonts.clear();
                for (auto& family : **result) {
                    if (!family.empty()) {
                        _fontFamilies.push_back(family);
                        _allFonts.insert(_allFonts.end(), family.begin(), family.end());
                    }
                }
                // _expandedFamilies.clear();
                Inkscape::sort_font_families(_fontFamilies, true);
    printf("fnt loaded: %ld\n", _fontFamilies.size());
                updateDisplayFonts();
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
            if (QString::fromUtf8(spec.raw()) == fontspec) {
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

void FontList::onRowSelected(int index) {
    if (index >= 0 && index < static_cast<int>(_displayFontspecs.size())) {
        Q_EMIT fontChanged(_displayFontspecs[index]);
    }
}

void FontList::onRowOpened(int index) {
    if (index >= 0 && index < static_cast<int>(_displayFontspecs.size())) {
        Q_EMIT fontSelected(_displayFontspecs[index]);
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

void FontList::filterFonts(const QString& match) {
    // TODO: Implement filtering logic
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
