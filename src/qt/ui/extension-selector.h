// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ExtensionSelector — export extension selector based on IconComboBox.
 */

#ifndef LINEA_UI_EXTENSION_SELECTOR_H
#define LINEA_UI_EXTENSION_SELECTOR_H

#include <vector>

#include "icon-combobox.h"

namespace Inkscape::Extension {
class Output;
}

namespace Linea::UI {

class ExtensionSelector final : public IconComboBox {
    Q_OBJECT

public:
    explicit ExtensionSelector(QWidget* parent = nullptr);

    Inkscape::Extension::Output* selectedExtension() const;

private:
    void populate();

    std::vector<Inkscape::Extension::Output*> _extensions;
};

} // namespace Linea::UI

#endif // LINEA_UI_EXTENSION_SELECTOR_H
