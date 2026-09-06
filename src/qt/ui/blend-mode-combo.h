// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * BlendModeCombo — blend mode selector based on IconComboBox.
 */

#ifndef LINEA_UI_BLEND_MODE_COMBO_H
#define LINEA_UI_BLEND_MODE_COMBO_H

#include <QWidget>

#include "style-enums.h"

namespace Linea::UI {

class IconComboBox;

/**
 * Blend mode selector wrapping IconComboBox.
 *
 * Populates the combo with all CSS blend modes grouped by category
 * (darken, lighten, contrast, difference, color).
 */
class BlendModeCombo : public QWidget {
    Q_OBJECT

public:
    explicit BlendModeCombo(QWidget* parent = nullptr);

    void setActiveById(SPBlendMode mode);
    SPBlendMode activeMode() const;

    // Deselect all — used for mixed state
    void selectNone();

Q_SIGNALS:
    void modeChanged(SPBlendMode mode);

private:
    void populate();

    IconComboBox* _combo = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_BLEND_MODE_COMBO_H
