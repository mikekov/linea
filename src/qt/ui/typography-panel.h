// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Typography panel — character styles, decoration, spacing and kerning editor
 */

#ifndef LINEA_UI_TYPOGRAPHY_PANEL_H
#define LINEA_UI_TYPOGRAPHY_PANEL_H

#include <QWidget>
#include <memory>
#include <sigc++/scoped_connection.h>

#include "ui/operation-blocker.h"

class SPDesktop;
class SPDocument;

QT_BEGIN_NAMESPACE
namespace Ui {
class TypographyPanel;
}
QT_END_NAMESPACE

namespace Inkscape::UI::Tools {
class TextTool;
}

namespace Linea::Props {
class Binder;
}

namespace Linea::UI {

class DecorationOptions;
class PopupMenu;

/**
 * Typography section of the text panel.
 *
 * Styles (super/subscript), text decoration, letter/word spacing,
 * kerning, character rotation, writing mode and direction.
 */
class TypographyPanel : public QWidget {
    Q_OBJECT

public:
    explicit TypographyPanel(QWidget* parent = nullptr);
    ~TypographyPanel() override;

    void setDocument(SPDocument* document);
    void setDesktop(SPDesktop* desktop);

    /// Declarative binding (called by TextPanel from its own bind()).
    void bind(Props::Binder& binder);

    /// Push text-tool cursor state (kerning, char rotation) that
    /// cannot be expressed as bound properties.
    void updateTypographyState(Inkscape::UI::Tools::TextTool* text_tool);

private:
    Inkscape::UI::Tools::TextTool* getTextTool() const;
    bool canUpdate() const;

    std::unique_ptr<Ui::TypographyPanel> _ui;
    SPDocument* _document = nullptr;
    SPDesktop* _desktop = nullptr;
    sigc::scoped_connection _cursor_moved;
    OperationBlocker _update;

    DecorationOptions* _decorationOptions = nullptr;
    PopupMenu* _decorationPopup = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_TYPOGRAPHY_PANEL_H
