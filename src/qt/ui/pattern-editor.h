// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PatternEditor — Qt pattern editor widget for Fill and Stroke panel.
 *
 * Copyright (C) 2022-2026 Michael Kowalski
 */

#ifndef LINEA_UI_PATTERN_EDITOR_H
#define LINEA_UI_PATTERN_EDITOR_H

#include <QWidget>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "2geom/affine.h"
#include "2geom/point.h"
#include "2geom/transforms.h"
#include "colors/color.h"
#include "pattern-manager.h"
#include "ui/operation-blocker.h"
#include "ui/widget/pattern-store.h"

class SPDocument;
class SPPattern;
class SPHatch;
class SPPaintServer;

QT_BEGIN_NAMESPACE
namespace Ui { class PatternEditor; }
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * Pattern editor widget providing pattern selection (stock and document),
 * tile preview, and property editing (scale, rotation, offset, gap/pitch/stroke).
 */
class PatternEditor : public QWidget {
    Q_OBJECT

public:
    explicit PatternEditor(const char* prefs, Inkscape::PatternManager& manager, QWidget* parent = nullptr);
    ~PatternEditor() override;

    // pass current document to extract patterns from
    void setDocument(SPDocument* document);

    // set the selected pattern / hatch
    void setSelected(SPPattern* pattern);
    void setSelected(SPHatch* hatch);

    // query selected pattern info
    std::pair<std::string, SPDocument*> getSelected();
    std::string getSelectedDocPattern();
    std::pair<std::string, SPDocument*> getSelectedStockPattern();
    std::optional<Inkscape::Colors::Color> getSelectedColor();
    Geom::Affine getSelectedTransform();
    Geom::Point getSelectedOffset() const;
    Geom::Scale getSelectedGap() const;
    bool isSelectedScaleUniform() const;
    std::string getLabel() const;

    // hatch-specific attributes
    double getSelectedRotation() const;
    double getSelectedPitch() const;
    double getSelectedThickness() const;

Q_SIGNALS:
    void changed();
    void colorChanged(Inkscape::Colors::Color color);
    void editRequested();

protected:
    void showEvent(QShowEvent* event) override;

private:
    using PatternItem    = Inkscape::UI::Widget::PatternItem;
    using PatternItemPtr = std::shared_ptr<PatternItem>;

    void setupCustomWidgets();
    void connectSignals();

    // Lazy category population; safe to call repeatedly.
    void initialSelect();

    // Gallery management
    void setStockPatterns(const std::vector<SPPaintServer*>& patterns);
    void applyFilter();
    std::vector<PatternItemPtr> updateDocPatternList(SPDocument* document);
    void updateTileImages();

    // Selection helpers
    PatternItemPtr activeDocItem() const;
    PatternItemPtr activeStockItem() const;
    std::pair<PatternItemPtr, SPDocument*> activeItem();
    void setActiveDocItem(const PatternItemPtr& item);
    void setActiveStockItem(const PatternItemPtr& item);

    void setSelectedInternal(SPPaintServer* linkPaint, SPPaintServer* rootPaint, Geom::Point offset);
    void setInitialSelection();
    void updateWidgetsFromPattern(const PatternItemPtr& pattern);
    void updateScaleLinkIcon();

    // Preview painting
    void drawPreview(QPainter* painter, const QRect& rect);

    // Draw one cell of a gallery
    void drawGalleryCell(QPainter* painter, const std::vector<PatternItemPtr>& items,
                         uint32_t index, const Geom::IntRect& rect, bool selected);

    // Build/refresh the draw callbacks on both galleries
    void rebuildGalleryCallbacks();

    // UI from .ui file
    std::unique_ptr<Ui::PatternEditor> _ui;

    // All custom widgets owned by _ui (declared in .ui file)

    // State
    std::string _prefs;
    Inkscape::PatternManager& _manager;
    OperationBlocker _update;

    bool _scaleLinked      = true;
    bool _uniformSupported = true;
    bool _showNames        = false;
    int  _tileSize         = 45;
    QString _filterText;

    SPDocument* _currentDocument = nullptr;

    // The pattern being edited: root id, link id, and preserved translation offset.
    struct { std::string id; std::string linkId; Geom::Point offset; } _currentPattern;

    bool _initialSelectionDone = false;

    // Flat item lists driving the two galleries
    std::vector<PatternItemPtr> _docItems;
    std::vector<PatternItemPtr> _stockItems;
    std::vector<PatternItemPtr> _stockItemsFiltered;

    // Cached current-document pattern items (key = pattern id)
    std::unordered_map<std::string, PatternItemPtr> _cachedItems;

    int _docSelected   = -1;
    int _stockSelected = -1;
};

} // namespace Linea::UI

#endif // LINEA_UI_PATTERN_EDITOR_H
