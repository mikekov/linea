// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SwatchEditor — edits document color swatches stored as CSS custom properties.
 */

#ifndef LINEA_UI_SWATCH_EDITOR_H
#define LINEA_UI_SWATCH_EDITOR_H

#include <QWidget>
#include <memory>
#include <string>
#include <vector>
#include <sigc++/scoped_connection.h>

#include "swatch-type.h"
#include "ui/operation-blocker.h"

class SPDocument;
class SPDesktop;
namespace Inkscape::Css {
class DocumentStyleSheet;
} // namespace Inkscape::Css

QT_BEGIN_NAMESPACE
class QListWidgetItem;
namespace Ui {
class SwatchEditor;
}
QT_END_NAMESPACE

namespace Linea::UI {

class ColorHolder;
class ColorPickerPanel;

/**
 * Editor for document color swatches.
 *
 * Swatches are the custom properties defined in the ':root' rule of the
 * first <svg:style> element under the document root; a variable counts as a
 * swatch when its value parses as a CSS color. The list shows the colors
 * with editable names, the picker below edits the selected swatch, and
 * buttons add or remove swatches. The <style> element (and the ':root'
 * rule) is created lazily on the first edit.
 */
class SwatchEditor : public QWidget {
    Q_OBJECT

public:
    explicit SwatchEditor(QWidget* parent = nullptr);
    ~SwatchEditor() override;

    void setDocument(SPDocument* document);
    void setDesktop(SPDesktop* desktop);

private:
    // Reloads the swatches unless an update is in flight.
    void onStyleContentChanged();
    // Re-parse the ':root' custom properties and rebuild the swatch list.
    void reload();
    // Copy the selected swatch's color into the picker.
    void pushSelectionToPicker();

    void onSwatchSelected(int row);
    void onItemRenamed(QListWidgetItem* item);
    void onColorChanged();
    void onAdd();
    void onDelete();

    // Replace the stylesheet text in the document, coalescing undo steps.
    void writeCss(std::string&& css);

    SPDocument* _document = nullptr;
    SPDesktop* _desktop = nullptr;
    std::vector<Swatch> _swatches;
    int _selected = -1;

    std::unique_ptr<Ui::SwatchEditor> _ui;
    std::unique_ptr<ColorPickerPanel> _picker;
    std::shared_ptr<ColorHolder> _colors;
    std::unique_ptr<Inkscape::Css::DocumentStyleSheet> _sheet;
    sigc::scoped_connection _colorConn;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_SWATCH_EDITOR_H
