// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * MarkerComboBox — button with popup for selecting stroke markers
 */

#include "marker-combo-box.h"
#include "theme.h"
#include "ui_marker-combo-box.h"

#include <cmath>

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>

#include "document.h"
#include "helper/stock-items.h"
#include "io/resource.h"
#include "number-edit.h"
#include "object/sp-defs.h"
#include "object/sp-marker.h"
#include "object/sp-marker-loc.h"
#include "object/sp-root.h"
#include "popup-menu.h"
#include "simple-grid.h"
#include "svg/css-ostringstream.h"
#include "util/object-renderer.h"
#include "util/static-doc.h"

namespace Linea::UI {

namespace {

constexpr int ITEM_WIDTH  = 35;
constexpr int ITEM_HEIGHT = 28;

// Render a "no marker" placeholder image showing a horizontal line with end caps
QImage createNoMarkerImage(int width, int height, qreal dpr, int location) {
    QImage img(std::lround(width * dpr), std::lround(height * dpr), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(128, 128, 128), 2.0));

    double midY = height / 2.0;
    p.drawLine(QPointF(0, midY), QPointF(width, midY));

    double h = 5.0;
    if (location == SP_MARKER_LOC_START) {
        p.drawLine(QPointF(1, midY - h), QPointF(1, midY + h));
    } else if (location == SP_MARKER_LOC_END) {
        p.drawLine(QPointF(width - 1, midY - h), QPointF(width - 1, midY + h));
    }
    p.end();
    return img;
}

SPMarker* findMarker(SPDocument* document, const std::string& markerId) {
    if (!document || markerId.empty()) return nullptr;

    auto* defs = document->getDefs();
    if (!defs) return nullptr;

    for (auto& child : defs->children) {
        if (auto* marker = cast<SPMarker>(&child)) {
            auto id = marker->getId();
            if (id && markerId == id) return marker;
        }
    }
    return nullptr;
}

std::string getAttrib(SPMarker* marker, const char* attrib) {
    auto value = marker->getAttribute(attrib);
    return value ? value : "";
}

double getAttribNum(SPMarker* marker, const char* attrib, double defaultValue = 0) {
    auto val = getAttrib(marker, attrib);
    return val.empty() ? defaultValue : strtod(val.c_str(), nullptr);
}

} // namespace

// --- MarkerComboBox ---

MarkerComboBox::MarkerComboBox(const std::string& id, int loc, QWidget* parent)
    : QWidget(parent)
    , _comboId(id)
    , _loc(loc)
{
    construct();
}

MarkerComboBox::~MarkerComboBox() = default;

void MarkerComboBox::construct() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _button = new QPushButton(this);
    _button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _button->setMinimumSize(40, 24);
    _button->setMaximumSize(240, 24);
    // _button->setPopupMode(QPushButton::InstantPopup);
    layout->addWidget(_button);

    if (_loc == SP_MARKER_LOC_START) {
        _button->setToolTip(tr("Start marker is drawn on the first node of a path"));
    } else if (_loc == SP_MARKER_LOC_MID) {
        _button->setToolTip(tr("Middle markers are drawn on every node except the first and last"));
    } else if (_loc == SP_MARKER_LOC_END) {
        _button->setToolTip(tr("End marker is drawn on the last node of a path"));
    }

    _sandbox = Inkscape::ink_markers_preview_doc(_comboId);

    buildPopup();
    setupIcons();
    connectSignals();
    initMarkerList();
    updateButtonPreview();
}

void MarkerComboBox::setupIcons() {
    _popupUi->clearButton->setIcon(QIcon(":/icons/edit-clear-value"));
    _popupUi->editButton->setIcon(QIcon(":/icons/edit"));
    _popupUi->orientAutoRev->setIcon(QIcon(":/icons/orient-auto-reverse"));
    _popupUi->orientAuto->setIcon(QIcon(":/icons/orient-auto"));
    _popupUi->orientAngle->setIcon(QIcon(":/icons/orient-angle"));
    _popupUi->flipHorz->setIcon(QIcon(":/icons/object-flip-horizontal"));
    updateScaleLinkIcon();
}

void MarkerComboBox::buildPopup() {
    _popup = new PopupMenu(this);
    _popupUi = std::make_unique<Ui::MarkerPopup>();
    _popupContent = new QWidget();
    _popupUi->setupUi(_popupContent);

    _markerGrid = _popupUi->markerGrid;
    _markerGrid->setCellSize(ITEM_WIDTH, ITEM_HEIGHT);
    _markerGrid->setShowGap(false);
    _markerGrid->setGap(4, 4);
    _markerGrid->setSelectable(true);
    _markerGrid->setHasFrame(true);

    // Set up the draw callback for marker cells
    _markerGrid->setDrawFunc([this](QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool selected) {
        if (index >= _allItems.size()) return;

        auto& item = _allItems[index];
        auto* marker = findMarker(item.source, item.id);

        QImage img = marker ? renderMarker(marker, rect.width(), rect.height())
                            : renderNoMarker(rect.width(), rect.height());

        QRect qr(rect.left(), rect.top(), rect.width(), rect.height());
        if (selected) {
            painter->fillRect(qr, _markerGrid->palette().highlight());
        }
        painter->drawImage(qr, img);
        if (!item.stock) {
            // draw a small triangle indicator for markers from the current document
            QPolygon triangle;
            int const size = 6;
            triangle << QPoint(qr.right(), qr.bottom())
                     << QPoint(qr.right() - size, qr.bottom())
                     << QPoint(qr.right(), qr.bottom() - size);
            painter->setBrush(Qt::darkGray);
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(triangle);
        }
    });

    _markerGrid->setTooltipFunc([this](int index) -> QString {
        if (index < 0 || static_cast<size_t>(index) >= _allItems.size()) return {};
        auto& item = _allItems[index];
        QString prefix = item.stock ? tr("Stock marker:") : tr("Document marker:");
        return prefix + "\n" + QString::fromStdString(item.label);
    });

    _popup->setContent(_popupContent);

    // Open popup on button click
    disconnect(_button, &QPushButton::clicked, this, nullptr);
    connect(_button, &QPushButton::clicked, this, [this]() {
        if (!_isUpToDate) refreshAfterModified();
        auto scoped(_update.block());
        auto marker = getCurrentMarker();
        int idx = findMarkerIndex(marker);
        if (idx >= 0) {
            _markerGrid->setSelectedCell(idx);
        }
        updateWidgetsFromMarker(marker);
        updatePreview();
        _popup->showBelowWidget(_button);
    });
}

void MarkerComboBox::connectSignals() {
    // Grid selection
    connect(_markerGrid, &SimpleGrid::cellSelected, this, [this](int index) {
        if (_update.pending()) return;
        // selectMarkerAt(index);
        Q_EMIT markerChanged();
    });

    connect(_markerGrid, &SimpleGrid::cellOpened, this, [this](int index) {
        if (_update.pending()) return;
        // selectMarkerAt(index);
        // _popup->hide();
        Q_EMIT markerChanged();
    });

    // Clear marker
    connect(_popupUi->clearButton, &QPushButton::clicked, this, [this]() {
        _markerGrid->clear();
        _currentMarkerId.clear();
        updateButtonPreview();
        updatePreview();
        // _popup->hide();
        Q_EMIT markerChanged();
    });

    // Edit marker
    connect(_popupUi->editButton, &QPushButton::clicked, this, [this]() {
        _popup->hide();
        Q_EMIT editRequested();
    });

    // Scale
    connect(_popupUi->scaleX, &NumberEdit::valueChanged, this, [this](double) {
        setScale(true);
    });
    connect(_popupUi->scaleY, &NumberEdit::valueChanged, this, [this](double) {
        setScale(false);
    });
    connect(_popupUi->linkScale, &QPushButton::toggled, this, [this](bool checked) {
        if (_update.pending()) return;
        _scaleLinked = checked;
        sp_marker_set_uniform_scale(getActiveDocumentMarker(), _scaleLinked);
        updateScaleLinkIcon();
    });
    connect(_popupUi->scaleWithStroke, &QCheckBox::toggled, this, [this](bool checked) {
        if (_update.pending()) return;
        sp_marker_scale_with_stroke(getActiveDocumentMarker(), checked);
    });

    // Orientation
    connect(_popupUi->orientAutoRev, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) setOrientation(false, "auto-start-reverse");
    });
    connect(_popupUi->orientAuto, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) setOrientation(false, "auto");
    });
    connect(_popupUi->orientAngle, &QPushButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        CSSOStringStream os;
        os << _popupUi->angle->value();
        setOrientation(true, os.str().c_str());
    });
    connect(_popupUi->angle, &NumberEdit::valueChanged, this, [this](double angle) {
        if (_update.pending() || !_popupUi->angle->isEnabled()) return;
        CSSOStringStream os;
        os << angle;
        sp_marker_set_orient(getActiveDocumentMarker(), os.str().c_str());
    });
    connect(_popupUi->flipHorz, &QPushButton::clicked, this, [this]() {
        sp_marker_flip_horizontally(getActiveDocumentMarker());
    });

    // Offset
    auto setOffset = [this]() {
        if (_update.pending()) return;
        sp_marker_set_offset(getActiveDocumentMarker(),
                             _popupUi->offsetX->value(),
                             _popupUi->offsetY->value());
    };
    connect(_popupUi->offsetX, &NumberEdit::valueChanged, this, setOffset);
    connect(_popupUi->offsetY, &NumberEdit::valueChanged, this, setOffset);

    // Opacity
    connect(_popupUi->opacity, &NumberEdit::valueChanged, this, [this](double alpha) {
        if (_update.pending()) return;
        sp_marker_set_opacity(getActiveDocumentMarker(), alpha);
    });
}

void MarkerComboBox::setOrientation(bool enableAngle, const char* value) {
    if (_update.pending()) return;
    _popupUi->angle->setEnabled(enableAngle);
    sp_marker_set_orient(getActiveDocumentMarker(), value);
}

void MarkerComboBox::setScale(bool changeWidth) {
    if (_update.pending()) return;

    auto* marker = getActiveDocumentMarker();
    if (!marker) return;

    auto sx = _popupUi->scaleX->value();
    auto sy = _popupUi->scaleY->value();
    auto width  = getAttribNum(marker, "markerWidth");
    auto height = getAttribNum(marker, "markerHeight");

    if (_scaleLinked && width > 0.0 && height > 0.0) {
        auto scoped(_update.block());
        if (changeWidth) {
            sy = height * (sx / width);
            _popupUi->scaleY->setValue(sy);
        } else {
            sx = width * (sy / height);
            _popupUi->scaleX->setValue(sx);
        }
    }
    sp_marker_set_size(marker, sx, sy);
}

void MarkerComboBox::updateScaleLinkIcon() {
    _popupUi->linkScale->setIcon(QIcon(
        _scaleLinked ? ":/icons/entries-linked" : ":/icons/entries-unlinked"));
}

// --- Document / marker list ---

void MarkerComboBox::setDocument(SPDocument* doc) {
    if (_document == doc) return;

    _modifiedConnection.disconnect();
    _document = doc;

    if (_document) {
        _modifiedConnection = _document->getDefs()->connectModified(
            [this](SPObject*, unsigned int) {
                _isUpToDate = false;
            });
    }

    _currentMarkerId.clear();
    refreshAfterModified();
}

void MarkerComboBox::setCurrent(SPObject* marker) {
    auto id = marker ? marker->getId() : nullptr;
    _currentMarkerId = id ? id : "";

    if (_popup->isVisible()) {
        if (!_isUpToDate) {
            refreshAfterModified();
        }
        auto* sp_marker = cast<SPMarker>(marker);
        auto scoped(_update.block());
        updateWidgetsFromMarker(sp_marker);
        int idx = findMarkerIndex(sp_marker);
        if (idx >= 0) {
            _markerGrid->setSelectedCell(idx);
        }
    }
    updateButtonPreview();
}

void MarkerComboBox::initMarkerList() {
    auto const markersDoc = Inkscape::Util::cache_static_doc([] {
        using namespace Inkscape::IO::Resource;
        auto source = get_path_string(SYSTEM, MARKERS, "markers.svg");
        return SPDocument::createNewDoc(source.c_str());
    });

    if (markersDoc) {
        markerListFromDoc(markersDoc, false);
    }
    refreshAfterModified();
}

void MarkerComboBox::markerListFromDoc(SPDocument* source, bool history) {
    auto markers = getMarkerList(source);
    if (history) {
        _historyItems.clear();
    } else {
        _stockItems.clear();
    }
    addMarkers(markers, source, history);
    rebuildStore();
}

std::vector<SPMarker*> MarkerComboBox::getMarkerList(SPDocument* source) {
    std::vector<SPMarker*> list;
    if (!source) return list;

    auto* defs = source->getDefs();
    if (!defs) return list;

    for (auto& child : defs->children) {
        if (auto* marker = cast<SPMarker>(&child)) {
            list.push_back(marker);
        }
    }
    return list;
}

void MarkerComboBox::addMarkers(const std::vector<SPMarker*>& markers, SPDocument* source, bool history) {
    for (auto* m : markers) {
        auto* repr = m->getRepr();
        const char* markid = repr->attribute("inkscape:stockid");
        if (!markid) markid = repr->attribute("id");

        MarkerItem item;
        item.source = source;
        if (auto id = repr->attribute("id")) {
            item.id = id;
        }
        item.label = markid ? markid : "";
        item.stock = !history;
        item.history = history;

        if (history) {
            _historyItems.push_back(std::move(item));
        } else {
            _stockItems.push_back(std::move(item));
        }
    }
}

void MarkerComboBox::rebuildStore() {
    _allItems.clear();
    _allItems.reserve(_historyItems.size() + _stockItems.size());
    _allItems.insert(_allItems.end(), _historyItems.begin(), _historyItems.end());
    _allItems.insert(_allItems.end(), _stockItems.begin(), _stockItems.end());

    _markerGrid->setCellCount(_allItems.size());
    _markerGrid->invalidate();
}

void MarkerComboBox::refreshAfterModified() {
    if (_update.pending()) return;

    auto scoped(_update.block());
    markerListFromDoc(_document, true);
    updateButtonPreview();
    updatePreview();
    _isUpToDate = true;
}

// --- Rendering ---

QImage MarkerComboBox::renderMarker(SPMarker* marker, int width, int height, double scale, std::optional<uint32_t> checkerboard) {
    if (!marker || !_sandbox) return renderNoMarker(width, height);

    qreal dpr = devicePixelRatioF();

    Inkscape::Drawing drawing;
    unsigned visionkey = SPItem::display_key_new(1);
    drawing.setRoot(_sandbox->getRoot()->invoke_show(drawing, visionkey, SP_ITEM_SHOW_DISPLAY));

    QColor fg = isDarkTheme() ? Qt::white : Qt::black;
    // pixel_size is in logical pixels; create_marker_image multiplies by device_scale internally
    auto surface = Inkscape::create_marker_image(
        _comboId, _sandbox.get(), fg,
        {width, height}, marker->getId(), marker->document,
        drawing, checkerboard, true, scale, std::lround(dpr), false);

    _sandbox->getRoot()->invoke_hide(visionkey);

    if (!surface) return renderNoMarker(width, height);

    // Convert Cairo surface to QImage
    surface->flush();
    auto data = surface->get_data();
    int w = surface->get_width();
    int h = surface->get_height();
    QImage img(data, w, h, surface->get_stride(), QImage::Format_ARGB32_Premultiplied);
    img = img.copy(); // detach from Cairo memory
    img.setDevicePixelRatio(dpr);
    return img;
}

QImage MarkerComboBox::renderNoMarker(int width, int height) {
    qreal dpr = devicePixelRatioF();
    return createNoMarkerImage(width, height, dpr, _loc);
}

// --- UI updates ---

void MarkerComboBox::updateButtonPreview() {
    auto marker = getCurrentMarker();
    QImage img = marker ? renderMarker(marker, ITEM_WIDTH, ITEM_HEIGHT)
                        : renderNoMarker(ITEM_WIDTH, ITEM_HEIGHT);
    _button->setIcon(QIcon(QPixmap::fromImage(img)));
    _button->setIconSize(QSize(ITEM_WIDTH, ITEM_HEIGHT));
}

void MarkerComboBox::updatePreview() {
    auto marker = getCurrentMarker();
    if (!marker) {
        _popupUi->previewLabel->clear();
        _popupUi->markerName->clear();
        return;
    }

    /* checkerboard:
    auto backgnd = palette().color(QPalette::Base);
    // Convert Qt's 0xAARRGGBB to Inkscape's 0xRRGGBBAA
    auto qt_rgba = backgnd.rgba();
    guint32 ink_rgba = ((qt_rgba & 0x00ffffff) << 8) | (qt_rgba >> 24);
    */
    QImage img = renderMarker(marker, 200, 80, 2.6);
    _popupUi->previewLabel->setPixmap(QPixmap::fromImage(img));
    updateMarkerName();
}

void MarkerComboBox::updateMarkerName() {
    auto* marker = getCurrentMarker();
    if (!marker) {
        _popupUi->markerName->clear();
        return;
    }

    int idx = findMarkerIndex(marker);
    if (idx >= 0 && static_cast<size_t>(idx) < _allItems.size()) {
        _popupUi->markerName->setText(QString::fromStdString(_allItems[idx].label));
    }
}

void MarkerComboBox::updateWidgetsFromMarker(SPMarker* marker) {
    bool enabled = marker != nullptr;
    _popupUi->scaleX->setEnabled(enabled);
    _popupUi->scaleY->setEnabled(enabled);
    _popupUi->linkScale->setEnabled(enabled);
    _popupUi->scaleWithStroke->setEnabled(enabled);
    _popupUi->orientAutoRev->setEnabled(enabled);
    _popupUi->orientAuto->setEnabled(enabled);
    _popupUi->orientAngle->setEnabled(enabled);
    _popupUi->angle->setEnabled(enabled);
    _popupUi->flipHorz->setEnabled(enabled);
    _popupUi->offsetX->setEnabled(enabled);
    _popupUi->offsetY->setEnabled(enabled);
    _popupUi->opacity->setEnabled(enabled);

    if (!marker) return;

    _popupUi->scaleX->setValue(getAttribNum(marker, "markerWidth"));
    _popupUi->scaleY->setValue(getAttribNum(marker, "markerHeight"));

    auto units = getAttrib(marker, "markerUnits");
    _popupUi->scaleWithStroke->setChecked(units == "strokeWidth" || units.empty());

    auto aspect = getAttrib(marker, "preserveAspectRatio");
    _scaleLinked = aspect != "none";
    _popupUi->linkScale->setChecked(_scaleLinked);
    updateScaleLinkIcon();

    _popupUi->offsetX->setValue(getAttribNum(marker, "refX"));
    _popupUi->offsetY->setValue(getAttribNum(marker, "refY"));
    _popupUi->opacity->setValue(getAttribNum(marker, "fill-opacity", 100.0));

    auto orient = getAttrib(marker, "orient");
    _popupUi->angle->setValue(strtod(orient.c_str(), nullptr));

    if (orient == "auto-start-reverse") {
        _popupUi->orientAutoRev->setChecked(true);
        _popupUi->angle->setEnabled(false);
    } else if (orient == "auto") {
        _popupUi->orientAuto->setChecked(true);
        _popupUi->angle->setEnabled(false);
    } else {
        _popupUi->orientAngle->setChecked(true);
        _popupUi->angle->setEnabled(true);
    }
}

// --- Selection / queries ---

SPMarker* MarkerComboBox::getCurrentMarker() const {
    return findMarker(_document, _currentMarkerId);
}

int MarkerComboBox::findMarkerIndex(SPMarker* marker) const {
    if (!marker) return -1;
    auto id = marker->getId();
    if (!id) return -1;

    for (size_t i = 0; i < _allItems.size(); ++i) {
        if (_allItems[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

SPMarker* MarkerComboBox::getActiveDocumentMarker() {
    auto* item = getSelectedItem();
    if (!item || item->id.empty()) return nullptr;

    if (item->stock) {
        std::string markurn = "urn:inkscape:marker:" + item->id;
        return cast<SPMarker>(get_stock_item(_document, markurn.c_str(), true));
    } else {
        return findMarker(_document, item->id);
    }
}

const MarkerComboBox::MarkerItem* MarkerComboBox::getSelectedItem() const {
    int idx = _markerGrid->selectedCell();
    if (idx < 0 || static_cast<size_t>(idx) >= _allItems.size()) return nullptr;
    return &_allItems[idx];
}

void MarkerComboBox::selectMarkerAt(int index) {
    if (index < 0 || static_cast<size_t>(index) >= _allItems.size()) return;

    auto& item = _allItems[index];
    _currentMarkerId = item.id;

    auto* marker = getCurrentMarker();
    auto scoped(_update.block());
    updateWidgetsFromMarker(marker);
    updatePreview();
    updateButtonPreview();
}

std::pair<std::string, std::string> MarkerComboBox::getActiveMarkerUri() {
    auto* item = getSelectedItem();
    if (!item) return {};

    if (item->id == "none") {
        return {"none", "none"};
    }
    if (item->id.empty()) return {};

    bool stockid = item->stock;
    std::string markurn = stockid ? "urn:inkscape:marker:" + item->id : item->id;
    auto* mark = cast<SPMarker>(get_stock_item(_document, markurn.c_str(), stockid));
    if (!mark) return {};

    auto* repr = mark->getRepr();
    auto id = repr->attribute("id");
    if (!id) return {};

    std::string markerId = id;
    std::string markerUri = "url(#" + markerId + ")";

    if (stockid) {
        repr->setAttribute("inkscape:collect", "always");
    }
    // adjust marker's attributes (or add missing ones) to stay in sync with marker tool
    sp_validate_marker(mark, _document);

    return {markerUri, markerId};
}

} // namespace Linea::UI
