// SPDX-License-Identifier: GPL-2.0-or-later

#include "blend-mode-combo.h"

#include <QHBoxLayout>

#include "icon-combobox.h"

namespace Linea::UI {

namespace {

struct BlendEntry {
    SPBlendMode mode;
    const char* label;
};

// Blend modes grouped by category (matching GTK order)
const BlendEntry blendModes[] = {
    { SP_CSS_BLEND_NORMAL,     "Normal" },
    // Darken group
    { SP_CSS_BLEND_DARKEN,     "Darken" },
    { SP_CSS_BLEND_MULTIPLY,   "Multiply" },
    { SP_CSS_BLEND_COLORBURN,  "Color Burn" },
    // Lighten group
    { SP_CSS_BLEND_LIGHTEN,    "Lighten" },
    { SP_CSS_BLEND_SCREEN,     "Screen" },
    { SP_CSS_BLEND_COLORDODGE, "Color Dodge" },
    // Contrast group
    { SP_CSS_BLEND_OVERLAY,    "Overlay" },
    { SP_CSS_BLEND_SOFTLIGHT,  "Soft Light" },
    { SP_CSS_BLEND_HARDLIGHT,  "Hard Light" },
    // Difference group
    { SP_CSS_BLEND_DIFFERENCE, "Difference" },
    { SP_CSS_BLEND_EXCLUSION,  "Exclusion" },
    // Color group
    { SP_CSS_BLEND_HUE,        "Hue" },
    { SP_CSS_BLEND_SATURATION, "Saturation" },
    { SP_CSS_BLEND_COLOR,      "Color" },
    { SP_CSS_BLEND_LUMINOSITY, "Luminosity" },
};

} // namespace

BlendModeCombo::BlendModeCombo(QWidget* parent)
    : QWidget(parent)
{
    _combo = new IconComboBox(this);
    _combo->setHeaderType(IconComboBox::LabelOnly);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_combo);

    populate();

    connect(_combo, &IconComboBox::currentChanged, this, [this](int id) {
        Q_EMIT modeChanged(static_cast<SPBlendMode>(id));
    });
}

void BlendModeCombo::populate() {
    for (auto& entry : blendModes) {
        _combo->addRow(QString(), tr(entry.label), static_cast<int>(entry.mode));
    }
    _combo->setActiveById(static_cast<int>(SP_CSS_BLEND_NORMAL));
}

void BlendModeCombo::setActiveById(SPBlendMode mode) {
    _combo->setActiveById(static_cast<int>(mode));
}

SPBlendMode BlendModeCombo::activeMode() const {
    return static_cast<SPBlendMode>(_combo->getActiveRowId());
}

void BlendModeCombo::selectNone() {
    _combo->setActiveById(-1);
}

} // namespace Linea::UI
