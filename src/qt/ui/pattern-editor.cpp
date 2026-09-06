// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PatternEditor — Qt pattern editor widget implementation.
 *
 * Copyright (C) 2022-2026 Michael Kowalski
 */

#include "pattern-editor.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QShowEvent>
#include <QSlider>
#include <QGuiApplication>
#include <QScreen>
#include <QSplitter>
#include <QPushButton>
#include <QWidgetAction>

#include "colors/color.h"
#include "document.h"
#include "object/sp-hatch.h"
#include "object/sp-paint-server.h"
#include "object/sp-pattern.h"
#include "pattern-manipulation.h"
#include "preferences.h"
#include "util-string/string-compare.h"

#include "qt/ui/number-edit.h"
#include "qt/ui/resizing-separator.h"
#include "qt/ui/simple-grid.h"

#include "ui_pattern-edit.h"

namespace Linea::UI {

namespace {

constexpr int ITEM_WIDTH = 45;

// slider position ↔ tile pixel size
int sliderToTile(int sliderValue) { return 30 + sliderValue * 5; }
int tileToSlider(int tileSize)    { return (tileSize - 30) / 5; }


void sortPatterns(std::vector<std::shared_ptr<Inkscape::UI::Widget::PatternItem>>& list) {
    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        if (!a || !b) return false;
        if (a->label == b->label) return a->id < b->id;
        return natural_compare(a->label, b->label);
    });
}

} // namespace

// ─── Constructor / destructor ────────────────────────────────────────────────

PatternEditor::PatternEditor(const char* prefs, Inkscape::PatternManager& manager, QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PatternEditor>())
    , _prefs(prefs)
    , _manager(manager)
{
    _ui->setupUi(this);

    auto* p = Inkscape::Preferences::get();
    _tileSize  = p->getIntLimited((_prefs + "/tileSize").c_str(),   ITEM_WIDTH, 30, 1000);
    _showNames = p->getBool      ((_prefs + "/showLabels").c_str(), false);

    setupCustomWidgets();
    connectSignals();
}

PatternEditor::~PatternEditor() = default;

// ─── Setup ───────────────────────────────────────────────────────────────────

void PatternEditor::setupCustomWidgets() {
    const int cellH = _tileSize + (_showNames ? 20 : 4);

    // Galleries and preview are owned by _ui (declared as custom widgets in the .ui file).
    // Configure them post-construction.
    _ui->docGallery->setCellSize(_tileSize + 4, cellH);
    _ui->docGallery->setCellStretch(false);
    _ui->docGallery->setHasFrame(false);
    _ui->docGallery->setSelectable(true);

    _ui->stockGallery->setCellSize(_tileSize + 4, cellH);
    _ui->stockGallery->setCellStretch(false);
    _ui->stockGallery->setHasFrame(false);
    _ui->stockGallery->setSelectable(true);

    _ui->preview->setDrawCallback([this](QPainter* p, const QRect& r){ drawPreview(p, r); });

    _ui->colorPicker->setTitle(tr("Pattern color"));
    _ui->colorPicker->setUseTransparency(false);

    // --- Options popup menu on the gear/options button ---
    auto* optMenu = new QMenu(this);

    auto* showNamesAct = optMenu->addAction(tr("Show names"));
    showNamesAct->setCheckable(true);
    showNamesAct->setChecked(_showNames);
    connect(showNamesAct, &QAction::toggled, this, [this](bool checked) {
        _showNames = checked;
        Inkscape::Preferences::get()->setBool((_prefs + "/showLabels").c_str(), checked);
        const int h = _tileSize + (checked ? 20 : 4);
        _ui->docGallery->setCellSize(_tileSize + 4, h);
        _ui->stockGallery->setCellSize(_tileSize + 4, h);
        rebuildGalleryCallbacks();
    });

    // Tile-size slider embedded in the menu
    auto* sliderWidget = new QWidget;
    auto* sliderLayout = new QHBoxLayout(sliderWidget);
    sliderLayout->setContentsMargins(6, 2, 6, 2);
    sliderLayout->addWidget(new QLabel(tr("Tile size:"), sliderWidget));
    auto* tileSlider = new QSlider(Qt::Horizontal, sliderWidget);
    tileSlider->setRange(0, 20);
    tileSlider->setValue(tileToSlider(_tileSize));
    sliderLayout->addWidget(tileSlider);
    auto* sliderAction = new QWidgetAction(optMenu);
    sliderAction->setDefaultWidget(sliderWidget);
    optMenu->addAction(sliderAction);
    connect(tileSlider, &QSlider::valueChanged, this, [this](int v) {
        const int newSize = sliderToTile(v);
        if (newSize == _tileSize) return;
        _tileSize = newSize;
        Inkscape::Preferences::get()->setInt((_prefs + "/tileSize").c_str(), newSize);
        const int h = newSize + (_showNames ? 20 : 4);
        _ui->docGallery->setCellSize(newSize + 4, h);
        _ui->stockGallery->setCellSize(newSize + 4, h);
        updateTileImages();
    });

    _ui->optionsButton->setMenu(optMenu);

    // --- Link-scale button (toggle) ---
    _ui->linkScaleButton->setCheckable(true);
    updateScaleLinkIcon();

    _ui->patternCombo->setProperty("class", "flat-combobox");

    const int screenH = QGuiApplication::primaryScreen()
                            ? QGuiApplication::primaryScreen()->geometry().height()
                            : 1080;
    const int maxListH = std::max(500, screenH - 400);
    _ui->separator->setOrientation(ResizingSeparator::Orientation::Vertical);
    _ui->separator->resize(_ui->splitter, QSize(-1, 120), QSize(-1, maxListH));
    // Restore splitter position
    const int listH = Inkscape::Preferences::get()->getIntLimited(
        (_prefs + "/listHeight").c_str(), 200, 100, maxListH);
    _ui->splitter->setProperty("class", "active-frame");
    _ui->splitter->setMaximumHeight(listH); // the two lists in a "spliiter" panel

    connect(_ui->separator, &ResizingSeparator::resized, this, [this](Geom::Point size) {
        Inkscape::Preferences::get()->setInt((_prefs + "/listHeight").c_str(), static_cast<int>(size.y()));
    });

    const int pos = Inkscape::Preferences::get()->getIntLimited(
        (_prefs + "/handlePos").c_str(), 120, 40, 9999);
    _ui->splitter->setSizes({pos, 200});
}

void PatternEditor::connectSignals() {
    // Search box filters both galleries
    connect(_ui->searchBox, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (_update.pending()) return;
        _filterText = text;
        // Re-filter doc gallery using existing items
        applyFilter();
        // Stock filter applied by applyFilter() below; rebuild doc list to apply filter there too
        updateDocPatternList(_currentDocument);
    });

    // Category combo
    connect(_ui->patternCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (_update.pending()) return;
        const auto& categories = _manager.get_categories();
        if (index >= 0 && index < (int)categories.size()) {
            setStockPatterns(categories[index]->patterns);
        }
        Inkscape::Preferences::get()->setInt((_prefs + "/currentSet").c_str(), index);
    });

    connect(_ui->previousButton, &QPushButton::clicked, this, [this] {
        const int prev = _ui->patternCombo->currentIndex() - 1;
        if (prev >= 0) _ui->patternCombo->setCurrentIndex(prev);
    });
    connect(_ui->nextButton, &QPushButton::clicked, this, [this] {
        const int next = _ui->patternCombo->currentIndex() + 1;
        if (next < _ui->patternCombo->count()) _ui->patternCombo->setCurrentIndex(next);
    });

    // Scale link toggle
    connect(_ui->linkScaleButton, &QPushButton::clicked, this, [this] {
        if (_update.pending()) return;
        auto scoped(_update.block());
        _scaleLinked = !_scaleLinked;
        if (_scaleLinked) {
            // match Y to X (mirrors GTK behaviour)
            _ui->scaleX->setValue(_ui->scaleY->value());
        }
        updateScaleLinkIcon();
        if (_uniformSupported) Q_EMIT changed();
    });

    // Scale spins — enforce uniform scaling when linked
    connect(_ui->scaleX, &NumberEdit::valueChanged, this, [this](double value) {
        if (_update.pending()) return;
        if (_scaleLinked) {
            auto scoped(_update.block());
            _ui->scaleY->setValue(value);
        }
        Q_EMIT changed();
    });
    connect(_ui->scaleY, &NumberEdit::valueChanged, this, [this](double value) {
        if (_update.pending()) return;
        if (_scaleLinked) {
            auto scoped(_update.block());
            _ui->scaleX->setValue(value);
        }
        Q_EMIT changed();
    });

    // Offset, gap, pitch, stroke, angle — emit changed when not blocked/insensitive
    auto emitIfActive = [this](NumberEdit* spin) {
        connect(spin, &NumberEdit::valueChanged, this, [this, spin](double) {
            if (_update.pending() || !spin->isEnabled()) return;
            Q_EMIT changed();
        });
    };
    emitIfActive(_ui->offsetX);
    emitIfActive(_ui->offsetY);
    emitIfActive(_ui->gapXSpin);
    emitIfActive(_ui->gapYSpin);
    emitIfActive(_ui->pitchSpin);
    emitIfActive(_ui->strokeSpin);
    emitIfActive(_ui->angleSpin);

    // Pattern name
    connect(_ui->patternName, &QLineEdit::textChanged, this, [this] {
        if (!_update.pending()) Q_EMIT changed();
    });

    // Color picker
    _ui->colorPicker->connectChanged([this](const Inkscape::Colors::Color& color) {
        if (!_update.pending()) Q_EMIT colorChanged(color);
    });

    // Edit button
    connect(_ui->editButton, &QPushButton::clicked, this, [this] { Q_EMIT editRequested(); });

    // Gallery selections
    connect(_ui->docGallery, &SimpleGrid::cellSelected, this, [this](int index) {
        if (_update.pending()) return;
        auto scoped(_update.block());
        _docSelected   = index;
        _stockSelected = -1;
        _ui->stockGallery->invalidate();
        if (index >= 0 && index < (int)_docItems.size()) {
            updateWidgetsFromPattern(_docItems[index]);
        }
        Q_EMIT changed();
    });
    connect(_ui->stockGallery, &SimpleGrid::cellSelected, this, [this](int index) {
        if (_update.pending()) return;
        auto scoped(_update.block());
        _stockSelected = index;
        _docSelected   = -1;
        _ui->docGallery->invalidate();
        if (index >= 0 && index < (int)_stockItemsFiltered.size()) {
            updateWidgetsFromPattern(_stockItemsFiltered[index]);
        }
        Q_EMIT changed();
    });

    // Save splitter position
    connect(_ui->splitter, &QSplitter::splitterMoved, this, [this](int pos, int) {
        Inkscape::Preferences::get()->setInt((_prefs + "/handlePos").c_str(), pos);
    });
}

// ─── Lazy initialisation ─────────────────────────────────────────────────────

void PatternEditor::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    initialSelect();
}

// Populate the category combo and load the saved category's patterns.
// Safe to call multiple times; only acts on the first call.
void PatternEditor::initialSelect() {
    if (_initialSelectionDone) return;

    const auto& categories = _manager.get_categories();
    const int catCount = static_cast<int>(categories.size());

    {
        QSignalBlocker blocker(_ui->patternCombo);
        _ui->patternCombo->clear();
        for (const auto& cat : categories) {
            _ui->patternCombo->addItem(QString::fromStdString(cat->name));
        }
    }

    const int saved = Inkscape::Preferences::get()->getIntLimited(
        (_prefs + "/currentSet").c_str(), 0, 0, std::max(0, catCount - 1));
    _ui->patternCombo->setCurrentIndex(saved);
    // Trigger population of stock patterns for this index
    if (saved >= 0 && saved < catCount) {
        setStockPatterns(categories[saved]->patterns);
    }

    _initialSelectionDone = true;
}

// ─── Gallery draw callbacks ───────────────────────────────────────────────────

void PatternEditor::drawGalleryCell(QPainter* painter,
                                    const std::vector<PatternItemPtr>& items,
                                    uint32_t index,
                                    const Geom::IntRect& rect,
                                    bool selected) {
    if (index >= items.size()) return;
    const auto& item = items[index];

    const QRect qr(rect.left(), rect.top(), rect.width(), rect.height());

    // Tile image
    if (!item->pix.isNull()) {
        const QRect pixRect = qr.adjusted(2, 2, -2, _showNames ? -18 : -2);
        painter->drawImage(pixRect, item->pix);
    }

    // Optional name label — small font, single line, elide at right
    if (_showNames) {
        const QRect textRect = qr.adjusted(2, qr.height() - 16, -2, -2);
        QFont f = painter->font();
        f.setPointSizeF(f.pointSizeF() * 0.8);
        painter->save();
        painter->setFont(f);
        const QString label = QString::fromStdString(item->label);
        const QString elided = painter->fontMetrics().elidedText(
            label, Qt::ElideRight, textRect.width());
        painter->drawText(textRect, Qt::AlignCenter | Qt::TextSingleLine, elided);
        painter->restore();
    }

    // Selection highlight
    if (selected) {
        painter->save();
        painter->setPen(QPen(palette().highlight().color(), 2));
        painter->drawRect(qr.adjusted(1, 1, -1, -1));
        painter->restore();
    }
}

void PatternEditor::rebuildGalleryCallbacks() {
    _ui->docGallery->setDrawFunc([this](QPainter* p, uint32_t idx, const Geom::IntRect& r, bool sel) {
        drawGalleryCell(p, _docItems, idx, r, sel);
    });
    _ui->docGallery->setTooltipFunc([this](int idx) -> QString {
        if (idx < 0 || idx >= (int)_docItems.size()) return {};
        return QString::fromStdString(_docItems[idx]->label);
    });

    _ui->stockGallery->setDrawFunc([this](QPainter* p, uint32_t idx, const Geom::IntRect& r, bool sel) {
        drawGalleryCell(p, _stockItemsFiltered, idx, r, sel);
    });
    _ui->stockGallery->setTooltipFunc([this](int idx) -> QString {
        if (idx < 0 || idx >= (int)_stockItemsFiltered.size()) return {};
        return QString::fromStdString(_stockItemsFiltered[idx]->label);
    });

    _ui->docGallery->invalidate();
    _ui->stockGallery->invalidate();
}

// ─── Gallery content management ───────────────────────────────────────────────

// Populate the doc-gallery from the document, reusing cached tile images.
std::vector<PatternEditor::PatternItemPtr>
PatternEditor::updateDocPatternList(SPDocument* document) {
    auto psList   = sp_get_pattern_list(document);
    auto hatchList = sp_get_hatch_list(document);
    psList.insert(psList.begin(), hatchList.begin(), hatchList.end());

    const double ds = devicePixelRatioF();

    // Build items without generating preview images (cheap)
    std::vector<PatternItemPtr> patterns;
    patterns.reserve(psList.size());
    for (auto* ps : psList) {
        if (auto item = _manager.get_item(ps)) {
            patterns.push_back(std::move(item));
        }
    }

    for (auto& item : patterns) {
        auto it = _cachedItems.find(item->id);
        if (it != _cachedItems.end()) {
            // Reuse cached preview image
            if (item->pix.isNull()) item->pix = it->second->pix;
        } else {
            if (item->pix.isNull()) {
                // Generate preview for a newly added pattern
                if (document) {
                    auto* ps = cast<SPPaintServer>(document->getObjectById(item->id));
                    item->pix = _manager.get_image(ps, _tileSize, _tileSize, ds);
                }
            }
            _cachedItems[item->id] = item;
        }
    }

    _docItems = patterns;

    _ui->docGallery->setCellCount(_docItems.size());
    rebuildGalleryCallbacks();

    return patterns;
}

void PatternEditor::setStockPatterns(const std::vector<SPPaintServer*>& patterns) {
    const double ds = devicePixelRatioF();

    _stockItems.clear();
    _stockItems.reserve(patterns.size());
    for (auto* ps : patterns) {
        if (auto item = _manager.get_item(ps)) {
            item->pix = _manager.get_image(ps, _tileSize, _tileSize, ds);
            _stockItems.push_back(std::move(item));
        }
    }
    sortPatterns(_stockItems);

    applyFilter();
}

void PatternEditor::applyFilter() {
    if (_filterText.isEmpty()) {
        _stockItemsFiltered = _stockItems;
    } else {
        const auto expr = _filterText.toLower();
        _stockItemsFiltered.clear();
        for (const auto& item : _stockItems) {
            if (QString::fromStdString(item->label).toLower().contains(expr)) {
                _stockItemsFiltered.push_back(item);
            }
        }
    }

    _ui->stockGallery->setCellCount(_stockItemsFiltered.size());
    rebuildGalleryCallbacks();
}

void PatternEditor::updateTileImages() {
    const double ds = devicePixelRatioF();

    auto regenerate = [&](std::vector<PatternItemPtr>& items, SPDocument* doc) {
        for (auto& item : items) {
            auto* d   = item->collection ? item->collection : doc;
            auto* ps  = d ? cast<SPPaintServer>(d->getObjectById(item->id)) : nullptr;
            if (!ps) continue;
            item->pix = _manager.get_image(ps, _tileSize, _tileSize, ds);
        }
    };

    regenerate(_docItems,   _currentDocument);
    regenerate(_stockItems, nullptr);

    // Propagate regenerated stock images into the filtered subset
    for (auto& filtered : _stockItemsFiltered) {
        for (const auto& full : _stockItems) {
            if (full->id == filtered->id && full->collection == filtered->collection) {
                filtered->pix = full->pix;
                break;
            }
        }
    }

    rebuildGalleryCallbacks();
}

// ─── Document / selection ────────────────────────────────────────────────────

void PatternEditor::setDocument(SPDocument* document) {
    _currentDocument = document;
    _cachedItems.clear();
    updateDocPatternList(document);
    setInitialSelection();
}

void PatternEditor::setSelected(SPPattern* pattern) {
    const auto offset = pattern ? pattern->getTransform().translation() : Geom::Point();
    setSelectedInternal(pattern, pattern ? pattern->rootPattern() : nullptr, offset);
}

void PatternEditor::setSelected(SPHatch* hatch) {
    // Hatch has dedicated x/y attributes; no need to preserve the transform offset
    setSelectedInternal(hatch, hatch ? hatch->rootHatch() : nullptr, Geom::Point());
}

void PatternEditor::setSelectedInternal(SPPaintServer* linkPaint,
                                        SPPaintServer* rootPaint,
                                        Geom::Point    offset) {
    auto scoped(_update.block());

    // Clear any stock-gallery selection
    _stockSelected = -1;
    _ui->stockGallery->invalidate();

    if (rootPaint && rootPaint != linkPaint) {
        _currentPattern.id     = rootPaint->getId();
        _currentPattern.linkId = linkPaint->getId();
        _currentPattern.offset = offset;
    } else {
        _currentPattern.id.clear();
        _currentPattern.linkId.clear();
        _currentPattern.offset = {};
    }

    auto item = _manager.get_item(linkPaint);
    updateWidgetsFromPattern(item);

    auto list = updateDocPatternList(rootPaint ? rootPaint->document : nullptr);

    if (rootPaint) {
        // Patch the tile image for the root pattern — other attributes (e.g. color) may have
        // changed on the root pattern directly, so regenerate it.
        const double ds = devicePixelRatioF();
        for (auto& patItem : list) {
            if (patItem->id == (item ? item->id : "") && patItem->collection == nullptr) {
                patItem->pix = _manager.get_image(rootPaint, _tileSize, _tileSize, ds);
                if (item) item->pix = patItem->pix;
                break;
            }
        }
    }

    setActiveDocItem(item);
    _ui->preview->update();
}

// Select the saved pattern in the galleries, used after set_document.
void PatternEditor::setInitialSelection() {
    auto [id, doc] = getSelected();
    if (id.empty()) return;

    auto scoped(_update.block());
    auto* d       = doc ? doc : _currentDocument;
    auto* element = d ? d->getObjectById(id) : nullptr;
    if (auto* ps = cast<SPPaintServer>(element)) {
        auto item = _manager.get_item(ps);
        updateWidgetsFromPattern(item);
    }
}

// ─── Selection helpers ────────────────────────────────────────────────────────

PatternEditor::PatternItemPtr PatternEditor::activeDocItem() const {
    if (_docSelected >= 0 && _docSelected < (int)_docItems.size()) {
        return _docItems[_docSelected];
    }
    return {};
}

PatternEditor::PatternItemPtr PatternEditor::activeStockItem() const {
    if (_stockSelected >= 0 && _stockSelected < (int)_stockItemsFiltered.size()) {
        return _stockItemsFiltered[_stockSelected];
    }
    return {};
}

std::pair<PatternEditor::PatternItemPtr, SPDocument*> PatternEditor::activeItem() {
    if (auto doc = activeDocItem()) return {doc, nullptr};
    if (auto stock = activeStockItem()) return {stock, stock->collection};
    return {};
}

void PatternEditor::setActiveDocItem(const PatternItemPtr& item) {
    _docSelected = -1;
    if (item) {
        for (int i = 0; i < (int)_docItems.size(); ++i) {
            if (_docItems[i]->id == item->id && _docItems[i]->collection == item->collection) {
                _docSelected = i;
                break;
            }
        }
    }
    _ui->docGallery->invalidate();
}

void PatternEditor::setActiveStockItem(const PatternItemPtr& item) {
    _stockSelected = -1;
    if (item) {
        for (int i = 0; i < (int)_stockItemsFiltered.size(); ++i) {
            if (_stockItemsFiltered[i]->id == item->id &&
                _stockItemsFiltered[i]->collection == item->collection) {
                _stockSelected = i;
                break;
            }
        }
    }
    _ui->stockGallery->invalidate();
}

// ─── Widget update ────────────────────────────────────────────────────────────

void PatternEditor::updateWidgetsFromPattern(const PatternItemPtr& pattern) {
    // Enable/disable the whole properties area
    _ui->propsGrid->setEnabled(!!pattern);

    static const auto empty = PatternItem::create();
    const auto& item = pattern ? *pattern : *empty;

    auto scoped(_update.block());

    // Name
    if (_ui->patternName->text().toStdString() != item.label) {
        _ui->patternName->setText(QString::fromStdString(item.label));
    }

    // Scale
    const double sx = item.transform.xAxis().length();
    const double sy = item.transform.yAxis().length();
    _ui->scaleX->setValue(sx);
    _ui->scaleY->setValue(sy);

    _scaleLinked      = item.uniform_scale.value_or(Geom::are_near(sx, sy));
    _uniformSupported = item.uniform_scale.has_value();
    updateScaleLinkIcon();

    // Offset
    _ui->offsetX->setValue(item.offset.x());
    _ui->offsetY->setValue(item.offset.y());

    // Rotation (in degrees)
    const double deg = item.rotation.has_value()
        ? *item.rotation
        : 180.0 / M_PI * Geom::atan2(item.transform.xAxis());
    _ui->angleSpin->setValue(deg);

    // Gap or pitch (mutually exclusive)
    const bool hasPitch = item.pitch.has_value();
    if (hasPitch) {
        _ui->pitchSpin->setValue(*item.pitch);
    } else {
        _ui->gapXSpin->setValue(item.gap[Geom::X]);
        _ui->gapYSpin->setValue(item.gap[Geom::Y]);
    }
    _ui->pitchSpin->setVisible( hasPitch);
    _ui->pitchLabel->setVisible(hasPitch);
    _ui->gapXSpin->setVisible(  !hasPitch);
    _ui->gapYSpin->setVisible(  !hasPitch);
    _ui->gapLabel->setVisible(  !hasPitch);

    // Stroke
    const bool hasStroke = item.stroke.has_value();
    _ui->strokeSpin->setValue(item.stroke.value_or(0.0));
    _ui->strokeSpin->setVisible( hasStroke);
    _ui->strokeLabel->setVisible(hasStroke);

    // Color picker
    if (item.color.has_value()) {
        _ui->colorPicker->setColor(*item.color);
        _ui->colorPicker->setEnabled(true);
    } else {
        _ui->colorPicker->setColor(Inkscape::Colors::Color(0x0));
        _ui->colorPicker->setEnabled(false);
        _ui->colorPicker->close();
    }

    // Edit button
    _ui->editButton->setEnabled(item.editable);
}

void PatternEditor::updateScaleLinkIcon() {
    _ui->linkScaleButton->setChecked(_scaleLinked);
    _ui->linkScaleButton->setIcon(QIcon(
        _scaleLinked ? ":/icons/entries-linked" : ":/icons/entries-unlinked"));
}

// ─── Preview ─────────────────────────────────────────────────────────────────

void PatternEditor::drawPreview(QPainter* painter, const QRect& rect) {
    if (_currentPattern.linkId.empty() || !_currentDocument) return;

    auto* linkPattern = cast<SPPaintServer>(
        _currentDocument->getObjectById(_currentPattern.linkId));
    if (!linkPattern) return;

    const double ds = devicePixelRatioF();
    // White background — most stock patterns are black
    constexpr unsigned int bg = 0xffffffff;
    const QImage img = _manager.get_preview(linkPattern, rect.width(), rect.height(), bg, ds);
    if (!img.isNull()) painter->drawImage(rect, img);
}

// ─── Public accessors ─────────────────────────────────────────────────────────

std::pair<std::string, SPDocument*> PatternEditor::getSelected() {
    const auto id = getSelectedDocPattern();
    if (!id.empty()) return {id, nullptr};
    return getSelectedStockPattern();
}

std::string PatternEditor::getSelectedDocPattern() {
    initialSelect();
    auto sel = activeDocItem();
    if (!sel) return {};
    // If the selection hasn't changed, return the link-pattern id so its transform/offset
    // can be modified in-place (mirrors GTK get_selected_doc_pattern logic).
    if (sel->id == _currentPattern.id) return _currentPattern.linkId;
    return sel->id;
}

std::pair<std::string, SPDocument*> PatternEditor::getSelectedStockPattern() {
    initialSelect();
    if (auto sel = activeStockItem()) {
        return {sel->id, sel->collection};
    }
    // Nothing selected: fall back to the first stock pattern so pattern-fill transitions work.
    if (!_stockItemsFiltered.empty()) {
        const auto& first = _stockItemsFiltered.front();
        return {first->id, first->collection};
    }
    return {{}, nullptr};
}

std::optional<Inkscape::Colors::Color> PatternEditor::getSelectedColor() {
    auto [item, _] = activeItem();
    if (item && item->color.has_value()) return _ui->colorPicker->getCurrentColor();
    return {};
}

Geom::Affine PatternEditor::getSelectedTransform() {
    Geom::Affine matrix;
    matrix *= Geom::Scale(_ui->scaleX->value(), _ui->scaleY->value());
    auto [item, _] = activeItem();
    if (item && !item->rotation.has_value()) {
        // Bake rotation into transform unless the item has a dedicated rotation attribute (hatch)
        matrix *= Geom::Rotate(_ui->angleSpin->value() / 180.0 * M_PI);
    }
    matrix.setTranslation(_currentPattern.offset);
    return matrix;
}

Geom::Point PatternEditor::getSelectedOffset() const {
    return Geom::Point(_ui->offsetX->value(), _ui->offsetY->value());
}

Geom::Scale PatternEditor::getSelectedGap() const {
    return Geom::Scale(_ui->gapXSpin->value(), _ui->gapYSpin->value());
}

bool PatternEditor::isSelectedScaleUniform() const { return _scaleLinked; }

std::string PatternEditor::getLabel() const {
    return _ui->patternName->text().toStdString();
}

double PatternEditor::getSelectedRotation()  const { return _ui->angleSpin->value();  }
double PatternEditor::getSelectedPitch()     const { return _ui->pitchSpin->value();  }
double PatternEditor::getSelectedThickness() const { return _ui->strokeSpin->value(); }

} // namespace Linea::UI
