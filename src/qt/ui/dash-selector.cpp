// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DashSelector — widget for selecting stroke dash patterns.
 */

#include "dash-selector.h"
#include "ui_dash-selector.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>

#include "drawing-area.h"
#include "popup-menu.h"
#include "number-edit.h"
#include "preferences.h"
#include "style.h"
#include "svg/css-ostringstream.h"

#include <numeric>
#include <sstream>

namespace Linea::UI {

namespace {

// Default custom pattern
const std::vector<double> DEFAULT_CUSTOM_PATTERN = {1, 2, 1, 4};
constexpr int ITEM_WIDTH = 100;
constexpr int ITEM_HEIGHT = 20;

// Parse dash pattern from string
std::vector<double> parseDashPattern(const QString& input) {
    std::vector<double> output;
    if (input.isEmpty()) return output;

    std::istringstream stream(input.toStdString());
    while (stream) {
        double val;
        stream >> val;
        if (stream) {
            output.push_back(val);
        }
    }
    return output;
}

// Format dash pattern to string
QString formatDashPattern(const std::vector<double>& pattern) {
    if (pattern.empty()) return QString();

    CSSOStringStream ost;
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (i > 0) ost << " ";
        ost << pattern[i];
    }
    return QString::fromStdString(ost.str());
}

// Load predefined dash patterns from preferences
std::vector<std::vector<double>> loadPredefinedPatterns() {
    std::vector<std::vector<double>> patterns;

    auto prefs = Preferences::get();
    auto const dashPrefs = prefs->getAllDirs("/palette/dashes");

    SPStyle style;
    for (auto const& dashPref : dashPrefs) {
        style.readFromPrefs(dashPref);
        std::vector<double> pattern;
        for (auto const& v : style.stroke_dasharray.values) {
            pattern.push_back(v.value);
        }
        patterns.emplace_back(std::move(pattern));
    }

    return patterns;
}

// Helper to draw a dash pattern within a given rectangle
void drawDashPattern(QPainter& p, const QRect& rect, const std::vector<double>& pattern,
                     const QColor& color, double offset = 0.0, int penWidth = 2) {
    p.setPen(QPen(color, penWidth, Qt::SolidLine, Qt::FlatCap));

    if (pattern.empty()) {
        // Solid line
        p.drawLine(rect.left(), rect.center().y(), rect.right(), rect.center().y());
        return;
    }

    double total = 0;
    for (double d : pattern) {
        total += d;
    }
    if (total <= 0) return;

    double scale = 2.0; // Match Gtkmm/Cairo: 2 units = 1 pixel
    double y = rect.center().y();
    double patternLength = total * scale;
    double offsetPixels = offset * scale;
    int repetitions = static_cast<int>(std::ceil((rect.width() + offsetPixels) / patternLength)) + 1;

    bool isDash = true;
    double currentX = rect.left() - offsetPixels;

    for (int rep = 0; rep < repetitions && currentX < rect.right(); ++rep) {
        for (double dashValue : pattern) {
            double segLen = dashValue * scale;

            if (currentX + segLen < rect.left()) {
                currentX += segLen;
                isDash = !isDash;
                continue;
            }

            if (currentX < rect.left()) {
                segLen -= (rect.left() - currentX);
                currentX = rect.left();
            }
            if (currentX + segLen > rect.right()) {
                segLen = rect.right() - currentX;
            }

            if (isDash && segLen > 0) {
                p.drawLine(QPointF(currentX, y), QPointF(currentX + segLen, y));
            }

            currentX += dashValue * scale;
            isDash = !isDash;

            if (currentX >= rect.right()) break;
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// DashPatternItem - internal implementation
// ---------------------------------------------------------------------------

class DashPatternItem : public QWidget {
    Q_OBJECT

public:
    explicit DashPatternItem(int index, DashSelector* selector, bool isCustom,
                             const QString& label, QWidget* parent = nullptr)
        : QWidget(parent)
        , _index(index)
        , _selector(selector)
        , _isCustom(isCustom)
        , _label(label)
    {
        setFixedSize(ITEM_WIDTH, ITEM_HEIGHT);
        setCursor(Qt::PointingHandCursor);
    }

    void setSelected(bool selected) {
        if (_selected != selected) {
            _selected = selected;
            update();
        }
    }

    int index() const { return _index; }

Q_SIGNALS:
    void clicked(int index);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (_selected) {
            p.fillRect(rect(), palette().highlight());
            p.setPen(palette().highlightedText().color());
        } else if (_hovered) {
            p.fillRect(rect(), palette().alternateBase());
            p.setPen(palette().windowText().color());
        } else {
            p.setPen(palette().windowText().color());
        }

        // Draw label if present (e.g., "Custom"), otherwise draw pattern
        if (!_label.isEmpty()) {
            p.drawText(rect(), Qt::AlignCenter, _label);
        } else if (_selector && _index >= 0 && static_cast<size_t>(_index) < _selector->_dashPatterns.size()) {
            QRect r = rect().adjusted(4, 4, -4, -4);
            drawDashPattern(p, r, _selector->_dashPatterns[_index], p.pen().color());
        }
    }

    void mousePressEvent(QMouseEvent*) override {
        Q_EMIT clicked(_index);
    }

    void enterEvent(QEnterEvent*) override {
        _hovered = true;
        update();
    }

    void leaveEvent(QEvent*) override {
        _hovered = false;
        update();
    }

private:
    int _index;
    DashSelector* _selector;
    bool _isCustom;
    QString _label;
    bool _selected = false;
    bool _hovered = false;
};

// ---------------------------------------------------------------------------
// DashSelector
// ---------------------------------------------------------------------------

DashSelector::DashSelector(bool compact, QWidget* parent)
    : QWidget(parent)
    , _compact(compact)
{
    construct(compact);
}

DashSelector::~DashSelector() = default;

void DashSelector::construct(bool compact) {
    //NOTE: 'compact' is not used

    setObjectName("DashSelector");

    _dashButton = new QPushButton(this);
    _dashButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _dashButton->setMinimumSize(60, 24);
    _dashButton->setMaximumSize(260, 24);
    _dashButton->setToolTip(tr("Select dash pattern"));
    connect(_dashButton, &QPushButton::clicked, this, [this]() {
        if (_popup) {
            _offsetEdit->setValue(_offset);
            updatePatternEntry();
            _popup->showBelowWidget(_dashButton);
        }
    });

    // Dash preview area
    _dashArea = new DrawingArea(_dashButton);
    _dashArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _dashArea->setAttribute(Qt::WA_TransparentForMouseEvents);
    _dashArea->setDrawCallback([this](QPainter* p, const QRect& rect) {
        QColor color = palette().windowText().color();
        if (!_placeholder.isEmpty()) {
            p->setPen(color);
            p->drawText(rect, Qt::AlignCenter, _placeholder);
        } else {
            drawDashPattern(*p, rect.adjusted(2, 2, -2, -2), _dashPattern, color, _offset, 2);
        }
    });

    auto buttonLayout = new QHBoxLayout(_dashButton);
    buttonLayout->setContentsMargins(4, 0, 4, 0);
    buttonLayout->setSpacing(0);
    buttonLayout->addWidget(_dashArea);
    auto iconLabel = new QLabel(_dashButton);
    iconLabel->setPixmap(QIcon(":/icons/pan-down").pixmap(16, 16));
    iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    buttonLayout->addWidget(iconLabel);


    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(_dashButton);

    loadDashPatterns();   // Must load patterns BEFORE building popup grid
    buildPopupMenu();
    setDashPattern({}, 0.0);
}

void DashSelector::buildPopupMenu() {
    _popup = new PopupMenu(this);
    _popupUi = std::make_unique<Ui::DashSelectorPopup>();
    _popupContent = new QWidget();
    _popupUi->setupUi(_popupContent);

    _patternEdit = _popupUi->patternEdit;
    _offsetEdit = _popupUi->offsetEdit;

    createPatternGrid();

    connect(_offsetEdit, &NumberEdit::valueChanged, this, &DashSelector::onOffsetValueChanged);
    connect(_patternEdit, &QLineEdit::textChanged, this, &DashSelector::onPatternTextChanged);

    _popup->setContent(_popupContent);
}

void DashSelector::createPatternGrid() {
    auto placeholder = _popupUi->patternGridPlaceholder;
    auto gridLayout = new QGridLayout(placeholder);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(2);

    // Build complete _dashPatterns list before creating items
    // This ensures indices match between _dashPatterns and _patternItems

    // Insert custom pattern at CUSTOM_POS (position 2)
    // If list is shorter than CUSTOM_POS, pad with custom patterns first
    while (_dashPatterns.size() < static_cast<size_t>(CUSTOM_POS)) {
        _dashPatterns.push_back(DEFAULT_CUSTOM_PATTERN);
    }
    _dashPatterns.insert(_dashPatterns.begin() + CUSTOM_POS, DEFAULT_CUSTOM_PATTERN);

    // Now create items with stable indices
    int col = 0;
    int row = 0;
    for (size_t i = 0; i < _dashPatterns.size(); ++i) {
        bool isCustom = (static_cast<int>(i) == CUSTOM_POS);
        QString label = isCustom ? tr("Custom") : QString();
        auto item = new DashPatternItem(static_cast<int>(i), this, isCustom, label, placeholder);
        connect(item, &DashPatternItem::clicked, this, &DashSelector::onPatternItemClicked);
        gridLayout->addWidget(item, row, col);
        _patternItems.push_back(item);

        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }
}

void DashSelector::loadDashPatterns() {
    _dashPatterns = loadPredefinedPatterns();

    // Ensure solid line (empty pattern) is always first
    if (_dashPatterns.empty() || !_dashPatterns[0].empty()) {
        _dashPatterns.insert(_dashPatterns.begin(), {});
    }

    // Add defaults if still just solid
    if (_dashPatterns.size() <= 1) {
        _dashPatterns.insert(_dashPatterns.end(), {
            {3, 3},
            {1, 3},
            {6, 3, 1, 3},
            {6, 3, 1, 3, 1, 3},
        });
    }
}

void DashSelector::setDashPattern(const std::vector<double>& dash, double offset) {
    auto scoped = _update.block();

    // Find matching pattern in _dashPatterns
    double const delta = std::accumulate(dash.begin(), dash.end(), 0.0)
                       / (10000.0 * (dash.empty() ? 1.0 : dash.size()));

    int position = CUSTOM_POS;
    for (size_t i = 0; i < _dashPatterns.size(); ++i) {
        if (std::equal(dash.begin(), dash.end(), _dashPatterns[i].begin(), _dashPatterns[i].end(),
                       [=](double a, double b) { return std::abs(a - b) < delta; })) {
            position = static_cast<int>(i);
            break;
        }
    }

    // Update selection visuals
    for (auto item : _patternItems) {
        item->setSelected(item->index() == position);
    }

    _dashPattern = dash;
    _offset = dash.empty() ? 0.0 : offset;

    updateButtonDisplay();
    _offsetEdit->setValue(_offset);
    updatePatternEntry();
}

void DashSelector::updateButtonDisplay() {
    if (_dashArea) _dashArea->update();
}

void DashSelector::updatePatternEntry() {
    if (!_patternEdit->hasFocus()) {
        _patternEdit->setText(formatDashPattern(_dashPattern));
    }
}

void DashSelector::setPlaceholder(const QString& placeholder) {
    _placeholder = placeholder;
    updateButtonDisplay();
}

std::vector<double> DashSelector::getCustomDashPattern() const {
    return parseDashPattern(_patternEdit->text());
}

void DashSelector::paintEvent(QPaintEvent*) {
}

void DashSelector::onPatternItemClicked(int index) {
    auto scoped = _update.block();

    for (auto item : _patternItems) {
        item->setSelected(item->index() == index);
    }

    if (index == CUSTOM_POS) {
        _dashPattern = getCustomDashPattern();
        if (_dashPattern.empty()) {
            _dashPattern = DEFAULT_CUSTOM_PATTERN;
        }
    } else if (index >= 0 && static_cast<size_t>(index) < _dashPatterns.size()) {
        _dashPattern = _dashPatterns[index];
    }

    _offset = 0.0;
    _offsetEdit->setValue(_offset);

    updateButtonDisplay();
    updatePatternEntry();

    _popup->hide();
    Q_EMIT dashChanged();
}

void DashSelector::onOffsetValueChanged(double value) {
    if (_update.pending()) return;
    _offset = value;
    Q_EMIT offsetChanged();
}

void DashSelector::onPatternTextChanged() {
    if (_update.pending()) return;

    auto pattern = getCustomDashPattern();
    if (!pattern.empty()) {
        _dashPattern = pattern;

        for (auto item : _patternItems) {
            item->setSelected(item->index() == CUSTOM_POS);
        }

        updateButtonDisplay();
        Q_EMIT patternChanged();
    }
}

} // namespace Linea::UI

#include "dash-selector.moc"
